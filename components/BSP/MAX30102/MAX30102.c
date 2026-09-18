#include "MAX30102.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// 商用手环最终参数
#define WRIST_THRESHOLD        100000
#define STABLE_COUNT           80
#define WRIST_DELAY_MS         10
#define WRIST_TRIGGER_PERCENT  70

#define HR_FILTER_SIZE         8
#define HR_MIN_INTERVAL        450
#define HR_MAX_INTERVAL        1200
#define HR_PEAK_MIN            20

#define SPO2_SAMPLE_COUNT      64
#define SPO2_FILTER_SIZE       24
#define DC_FILTER_ALPHA        0.97f

// MAXIM 标准 SpO2 查表
static const uint8_t spo2_lut[100] = {
    99,99,99,99,99,98,98,98,98,98,
    97,97,97,97,97,97,96,96,96,96,
    96,95,95,95,95,95,94,94,94,94,
    93,93,93,93,92,92,92,92,91,91,
    90,90,89,89,89,88,88,87,87,86,
    85,85,84,84,83,83,82,82,81,80,
    79,79,78,78,77,76,75,75,74,73,
    72,72,71,70,69,69,68,67,66,66,
    65,64,63,62,62,61,60,60,59,58,
    57,56,56,55,54,53,52,51,50,49
};

static const uint8_t REG_FIFO_DATA      = 0x07;
static const uint8_t REG_MODE_CONFIG    = 0x09;
static const uint8_t REG_SPO2_CONFIG    = 0x0A;
static const uint8_t REG_LED1_PA        = 0x0C;
static const uint8_t REG_LED2_PA        = 0x0D;
static const uint8_t REG_FIFO_WR_PTR    = 0x04;
static const uint8_t REG_FIFO_RD_PTR    = 0x05;
static const uint8_t REG_FIFO_OVF_FLOW  = 0x06;

// 心率
static int32_t dc_ir_hr = 0;
static uint32_t last_beat = 0;
static float hr_buffer[HR_FILTER_SIZE];
static uint8_t hr_idx = 0;
static float stable_hr = 0.0f;
static int32_t prev_ac = 0;
static bool peak_flag = false;

// 血氧
static float dc_ir_spo2 = 0.0f;
static float dc_red_spo2 = 0.0f;
static float ac_ir[SPO2_SAMPLE_COUNT];
static float ac_red[SPO2_SAMPLE_COUNT];
static uint8_t spo2_sample_idx = 0;
static float spo2_buffer[SPO2_FILTER_SIZE];
static uint8_t spo2_buf_idx = 0;
static float stable_spo2 = 0.0f;

static esp_err_t max30102_write_reg(uint8_t reg, uint8_t data);
static void process_heart_rate(uint32_t ir);
static void process_spo2(uint32_t ir, uint32_t red);

static esp_err_t max30102_write_reg(uint8_t reg, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(MAX30102_I2C_NUM, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static void max30102_i2c_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MAX30102_I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = MAX30102_I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(MAX30102_I2C_NUM, &conf);
    i2c_driver_install(MAX30102_I2C_NUM, conf.mode, 0, 0, 0);
}

esp_err_t max30102_read_fifo(uint32_t *ir, uint32_t *red) {
    uint8_t buf[6];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, REG_FIFO_DATA, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_READ, true);

    for(int i=0; i<5; i++)
        i2c_master_read_byte(cmd, &buf[i], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &buf[5], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(MAX30102_I2C_NUM, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);

    *ir  = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    *red = ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5];

    process_heart_rate(*ir);
    process_spo2(*ir, *red);
    return ret;
}

esp_err_t max30102_init(void) {
    max30102_i2c_init();
    vTaskDelay(100);
    max30102_write_reg(REG_MODE_CONFIG, 0x40);
    vTaskDelay(100);
    max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
    max30102_write_reg(REG_FIFO_OVF_FLOW, 0x00);
    max30102_write_reg(REG_MODE_CONFIG, 0x03);
    max30102_write_reg(REG_SPO2_CONFIG, 0x27);
    max30102_write_reg(REG_LED1_PA, 0x30);
    max30102_write_reg(REG_LED2_PA, 0x30);

    dc_ir_hr = 0;
    dc_ir_spo2 = 0.0f;
    dc_red_spo2 = 0.0f;
    last_beat = 0;
    stable_hr = 0.0f;
    stable_spo2 = 0.0f;
    prev_ac = 0;
    peak_flag = false;
    memset(hr_buffer, 0, sizeof(hr_buffer));
    memset(spo2_buffer, 0, sizeof(spo2_buffer));
    spo2_sample_idx = 0;
    spo2_buf_idx = 0;
    hr_idx = 0;

    return ESP_OK;
}

bool max30102_is_wrist_detected(void) {
    int valid_count = 0;
    for(int i = 0; i < STABLE_COUNT; i++) {
        uint32_t ir, red;
        max30102_read_fifo(&ir, &red);
        if(ir > WRIST_THRESHOLD) valid_count++;
        vTaskDelay(WRIST_DELAY_MS);
    }
    return (valid_count >= (STABLE_COUNT * WRIST_TRIGGER_PERCENT / 100));
}

static void process_heart_rate(uint32_t ir) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (dc_ir_hr == 0) dc_ir_hr = ir;
    dc_ir_hr = (dc_ir_hr * 95 + ir) / 100;
    int32_t ac = ir - dc_ir_hr;

    if (now - last_beat > 2000) {
        stable_hr = 0;
        peak_flag = false;
    }

    if (ac > prev_ac && ac > HR_PEAK_MIN) {
        peak_flag = true;
    }
    else if (ac < prev_ac && peak_flag) {
        peak_flag = false;
        uint32_t interval = now - last_beat;

        if (last_beat != 0 && interval > HR_MIN_INTERVAL && interval < HR_MAX_INTERVAL) {
            float hr = 60000.0f / interval;
            if (hr >= 50 && hr <= 120) {
                hr_buffer[hr_idx++] = hr;
                if (hr_idx >= HR_FILTER_SIZE) hr_idx = 0;
            }
        }
        last_beat = now;
    }
    prev_ac = ac;

    float sum = 0;
    uint8_t cnt = 0;
    for (int i = 0; i < HR_FILTER_SIZE; i++) {
        if (hr_buffer[i] >= 50 && hr_buffer[i] <= 120) {
            sum += hr_buffer[i];
            cnt++;
        }
    }
    stable_hr = (cnt > 0) ? (sum / cnt) : 0;
}

static void process_spo2(uint32_t ir, uint32_t red) {
    if (dc_ir_spo2 == 0) dc_ir_spo2 = ir;
    if (dc_red_spo2 == 0) dc_red_spo2 = red;

    dc_ir_spo2  = DC_FILTER_ALPHA * dc_ir_spo2  + (1.0f - DC_FILTER_ALPHA) * ir;
    dc_red_spo2 = DC_FILTER_ALPHA * dc_red_spo2 + (1.0f - DC_FILTER_ALPHA) * red;

    float ac_ir_val  = fabsf((float)ir - dc_ir_spo2);
    float ac_red_val = fabsf((float)red - dc_red_spo2);

    ac_ir[spo2_sample_idx]  = ac_ir_val;
    ac_red[spo2_sample_idx] = ac_red_val;
    spo2_sample_idx++;

    if (spo2_sample_idx >= SPO2_SAMPLE_COUNT) {
        spo2_sample_idx = 0;

        float ir_max = -1.0f, ir_min = 1e9f;
        float rd_max = -1.0f, rd_min = 1e9f;
        for (int i = 0; i < SPO2_SAMPLE_COUNT; i++) {
            ir_max = fmaxf(ir_max, ac_ir[i]);
            ir_min = fminf(ir_min, ac_ir[i]);
            rd_max = fmaxf(rd_max, ac_red[i]);
            rd_min = fminf(rd_min, ac_red[i]);
        }

        float ac_ir_amp  = ir_max - ir_min;
        float ac_red_amp = rd_max - rd_min;

        if (ac_ir_amp < 8 || ac_red_amp < 5 || dc_ir_spo2 < 20000.0f) {
            return;
        }

        float R = (ac_red_amp / dc_red_spo2) / (ac_ir_amp / dc_ir_spo2);
        int idx = (int)((R - 0.55f) * 75.0f);
        if (idx < 0)  idx = 0;
        if (idx > 99) idx = 99;

        float spo2_val = spo2_lut[idx];

        spo2_buffer[spo2_buf_idx++] = spo2_val;
        if (spo2_buf_idx >= SPO2_FILTER_SIZE) {
            spo2_buf_idx = 0;
        }

        float sum = 0.0f;
        int cnt = 0;
        for (int i = 0; i < SPO2_FILTER_SIZE; i++) {
            if (spo2_buffer[i] >= 90.0f && spo2_buffer[i] <= 100.0f) {
                sum += spo2_buffer[i];
                cnt++;
            }
        }

        if (cnt > 0) {
            stable_spo2 = sum / cnt;
        }
    }
}

uint8_t max30102_get_heart_rate(void) {
    return (stable_hr >= 50 && stable_hr <= 120) ? (uint8_t)stable_hr : 0;
}

uint8_t max30102_get_spo2(void) {
    return (stable_spo2 >= 90 && stable_spo2 <= 100) ? (uint8_t)stable_spo2 : 0;
}