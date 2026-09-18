#ifndef _MPU6050_H
#define _MPU6050_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>

#define MPU6050_SDA_PIN 15
#define MPU6050_SCL_PIN 16
#define MPU6050_DELAY_US 30
#define MPU6050_ADDR 0x68

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    float pitch;
    float roll;
    uint32_t steps;
} MPU6050_Data_t;

void mpu6050_task(void *arg);
extern uint32_t mpu6050_step_count;  // 全局步数变量（供main.c使用）

#endif