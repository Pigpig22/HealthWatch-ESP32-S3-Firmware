#include "NTC.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include <math.h>

#define TAG "NTC"
static float calibration_offset = 0.0f;

// 适配模块的默认配置（10k/B3950，模块内部分压10k）
ntc_config_t ntc_get_default_config(void) {
    ntc_config_t config = {
        .adc_channel = NTC_ADC_CHANNEL,
        .do_pin = NTC_DO_PIN,
        .series_res = 10000.0f,   // 模块内部分压电阻10k
        .nominal_res = 10000.0f,  // NTC在25℃的阻值10k
        .b_coeff = 3950.0f,       // 标准B值
        .nominal_temp = 25.0f
    };
    return config;
}

static void init_adc(ntc_config_t *config) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    // 模块AO输出是3.3V兼容，用11dB衰减
    adc1_config_channel_atten(config->adc_channel, ADC_ATTEN_DB_11);
}

static void init_gpio(ntc_config_t *config) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->do_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void ntc_init(ntc_config_t *config) {
    init_adc(config);
    init_gpio(config);
    calibration_offset = 0.0f;
    ESP_LOGI(TAG, "NTC模块初始化完成");
}

// 适配模块的ADC采样（轻量滤波，避免反应迟钝）
int ntc_read_adc_raw(ntc_config_t *config) {
    int32_t sum = 0;
    const int samples = 5;  // 模块输出稳定，少量采样即可
    for(int i=0; i<samples; i++){
        sum += adc1_get_raw(config->adc_channel);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return sum / samples;
}

float ntc_read_voltage(ntc_config_t *config) {
    int raw = ntc_read_adc_raw(config);
    // 模块AO输出电压范围0~3.3V，对应ADC 0~4095
    return (float)raw * 3.3f / 4095.0f;
}

// 关键修复：模块内部是NTC接3.3V、分压电阻接地的电路
float ntc_read_resistance(ntc_config_t *config) {
    float v = ntc_read_voltage(config);
    // 模块跟随器输出不会到0/3.3V，限制有效范围
    if (v < 0.1f || v > 3.2f) return -1.0f;

    // 模块分压公式：Rntc = R分压 * (3.3V - Vadc) / Vadc
    return config->series_res * (3.3f - v) / v;
}

float ntc_read_temperature(ntc_config_t *config) {
    float r = ntc_read_resistance(config);
    if (r <= 100.0f || r > 100000.0f) {
        return -273.15f;
    }

    float R0 = config->nominal_res;
    float T0 = config->nominal_temp + 273.15f;
    float B = config->b_coeff;

    float kelvin = 1.0f / (1.0f / T0 + log(r / R0) / B);
    // 去掉硬编码+3℃，只用校准值
    return kelvin - 273.15f + calibration_offset;
}

// 适配体温测量的快速平均，反应更快
float ntc_read_temperature_avg(ntc_config_t *config, int samples) {
    float sum = 0;
    int count = 0;
    for (int i = 0; i < samples; i++) {
        float t = ntc_read_temperature(config);
        // 体温有效范围，过滤异常值
        if (t > 10 && t < 50) {
            sum += t;
            count++;
        }
        // 缩短延时，温度变化反应更快
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return count > 0 ? sum / count : -273.15f;
}

bool ntc_read_alarm(ntc_config_t *config) {
    return gpio_get_level(config->do_pin) == 0;
}

temp_status_t ntc_get_temp_status(float t) {
    if (t < 36.0f) return TEMP_LOW;
    if (t <= 37.2f) return TEMP_NORMAL;
    if (t <= 37.9f) return TEMP_HIGH;
    return TEMP_FEVER;
}

const char* ntc_get_temp_status_string(temp_status_t s) {
    switch(s) {
        case TEMP_LOW: return "偏低";
        case TEMP_NORMAL: return "正常";
        case TEMP_HIGH: return "偏高";
        case TEMP_FEVER: return "发热";
        default: return "未知";
    }
}

void ntc_calibrate(ntc_config_t *config, float ref) {
    float now = ntc_read_temperature_avg(config, 50);
    calibration_offset = ref - now;
    ESP_LOGI(TAG, "校准完成: 测量值=%.2f℃ → 偏移量=%.2f℃", now, calibration_offset);
}