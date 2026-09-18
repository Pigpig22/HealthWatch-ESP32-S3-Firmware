#include "MPU6050.h"
#include "fall_detect.h"
// 新增全局步数变量（供main.c读取）
uint32_t mpu6050_step_count = 0;

static void i2c_start(void) {
    gpio_set_level(MPU6050_SDA_PIN, 1);
    gpio_set_level(MPU6050_SCL_PIN, 1);
    esp_rom_delay_us(MPU6050_DELAY_US);
    gpio_set_level(MPU6050_SDA_PIN, 0);
    esp_rom_delay_us(MPU6050_DELAY_US);
    gpio_set_level(MPU6050_SCL_PIN, 0);
}

static void i2c_stop(void) {
    gpio_set_level(MPU6050_SDA_PIN, 0);
    gpio_set_level(MPU6050_SCL_PIN, 1);
    esp_rom_delay_us(MPU6050_DELAY_US);
    gpio_set_level(MPU6050_SDA_PIN, 1);
    esp_rom_delay_us(MPU6050_DELAY_US);
}

static int i2c_write(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        gpio_set_level(MPU6050_SDA_PIN, (data & 0x80) ? 1 : 0);
        esp_rom_delay_us(MPU6050_DELAY_US);
        gpio_set_level(MPU6050_SCL_PIN, 1);
        esp_rom_delay_us(MPU6050_DELAY_US);
        gpio_set_level(MPU6050_SCL_PIN, 0);
        data <<= 1;
    }
    gpio_set_direction(MPU6050_SDA_PIN, GPIO_MODE_INPUT);
    esp_rom_delay_us(MPU6050_DELAY_US);
    gpio_set_level(MPU6050_SCL_PIN, 1);
    esp_rom_delay_us(MPU6050_DELAY_US);
    int ack = gpio_get_level(MPU6050_SDA_PIN);
    gpio_set_level(MPU6050_SCL_PIN, 0);
    gpio_set_direction(MPU6050_SDA_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
    return ack == 0;
}

static uint8_t i2c_read(uint8_t ack) {
    uint8_t data = 0;
    gpio_set_direction(MPU6050_SDA_PIN, GPIO_MODE_INPUT);
    for (int i = 0; i < 8; i++) {
        gpio_set_level(MPU6050_SCL_PIN, 1);
        esp_rom_delay_us(MPU6050_DELAY_US);
        data = (data << 1) | gpio_get_level(MPU6050_SDA_PIN);
        gpio_set_level(MPU6050_SCL_PIN, 0);
        esp_rom_delay_us(MPU6050_DELAY_US);
    }
    gpio_set_direction(MPU6050_SDA_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(MPU6050_SDA_PIN, ack ? 0 : 1);
    gpio_set_level(MPU6050_SCL_PIN, 1);
    esp_rom_delay_us(MPU6050_DELAY_US);
    gpio_set_level(MPU6050_SCL_PIN, 0);
    return data;
}

static void mpu_write(uint8_t reg, uint8_t data) {
    i2c_start();
    i2c_write(MPU6050_ADDR << 1);
    i2c_write(reg);
    i2c_write(data);
    i2c_stop();
}

static void mpu_read_buf(uint8_t reg, uint8_t *buf, uint8_t len) {
    i2c_start();
    i2c_write(MPU6050_ADDR << 1);
    i2c_write(reg);
    i2c_start();
    i2c_write((MPU6050_ADDR << 1) | 1);
    for (int i=0; i<len; i++) buf[i] = i2c_read(i < len-1);
    i2c_stop();
}

// ====================== 【步数检测：修复完成】 ======================
static int32_t last_z = 0;
static uint8_t step_state = 0; // 0=等待峰值 1=等待谷值

static void step_detect(MPU6050_Data_t *d) {
    int32_t now_z = d->accel_z;
    int32_t delta = now_z - last_z;
    const int32_t UP_THRES   = 10000;   // 上升阈值
    const int32_t DOWN_THRES = -10000;  // 下降阈值

    if (step_state == 0 && delta > UP_THRES) {
        step_state = 1;
    }
    if (step_state == 1 && delta < DOWN_THRES) {
        d->steps++;
        step_state = 0;
    }
    last_z = now_z;
}

static void mpu_read_data(MPU6050_Data_t *d) {
    uint8_t buf[14];
    mpu_read_buf(0x3B, buf, 14);
    d->accel_x = (buf[0] << 8) | buf[1];
    d->accel_y = (buf[2] << 8) | buf[3];
    d->accel_z = (buf[4] << 8) | buf[5];

    float ax = d->accel_x / 16384.0f;
    float ay = d->accel_y / 16384.0f;
    float az = d->accel_z / 16384.0f;
    d->pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0f / M_PI;
    d->roll  = atan2(ay, az) * 180.0f / M_PI;
}

void mpu6050_task(void *arg) {
    MPU6050_Data_t d = {0};
    Health_Data_t health_data = {0}; // 健康数据载体
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << MPU6050_SDA_PIN) | (1ULL << MPU6050_SCL_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(MPU6050_SDA_PIN, 1);
    gpio_set_level(MPU6050_SCL_PIN, 1);
    mpu_write(0x6B, 0x00); // 唤醒MPU6050
    vTaskDelay(pdMS_TO_TICKS(100));

    printf("\n=========================================\n");
    printf("           MPU6050 运行正常              \n");
    printf("=========================================\n");

    while (1) {
        mpu_read_data(&d);
        step_detect(&d);
        mpu6050_step_count = d.steps; // 更新全局步数

        // 同步MPU6050数据到健康数据结构体
        health_data.accel_x = d.accel_x;
        health_data.accel_y = d.accel_y;
        health_data.accel_z = d.accel_z;
        health_data.pitch = d.pitch;
        health_data.roll = d.roll;
        // 暂不更新心率/血氧（后续由对应模块补充）
        health_data.heart_rate = g_health_data.heart_rate;
        health_data.blood_oxygen = g_health_data.blood_oxygen;
        
        // 推送到摔倒检测模块
        fall_detect_update_health_data(&health_data);

        // 美化输出（可选）
        //printf("| Pitch: %6.1f ° | Roll: %6.1f ° | 步数: %3d |\n",d.pitch, d.roll, d.steps);

        vTaskDelay(pdMS_TO_TICKS(200)); // 200ms采集一次
    }
}
