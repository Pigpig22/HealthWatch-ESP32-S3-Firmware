#ifndef _FALL_DETECT_H
#define _FALL_DETECT_H

#include "MPU6050.h"
#include <stdint.h>

// 摔倒检测配置参数（可根据需求调整）
#define FALL_ACCEL_THRESHOLD  15000    // 加速度阈值（摔倒时的剧烈变化）
#define FALL_ANGLE_THRESHOLD  45.0f    // 角度阈值（倾斜超过该角度判定为摔倒）
#define FALL_DETECT_INTERVAL  500      // 摔倒判定持续时间（ms）
#define FALL_SAMPLE_COUNT     3        // 连续采样次数（多次判定提高准确性）

// 健康数据结构体（扩展：整合加速度、角度、心率、血氧）
typedef struct {
    // MPU6050数据
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    float pitch;
    float roll;
    // 健康指标（预留心率、血氧字段）
    uint8_t heart_rate;    // 心率值（0-255）
    uint8_t blood_oxygen;  // 血氧值（0-100）
    // 摔倒检测状态
    uint8_t is_fall;       // 0=未摔倒 1=摔倒
} Health_Data_t;

// 函数声明
void fall_detect_init(void);                  // 初始化摔倒检测
void fall_detect_task(void *arg);             // 摔倒检测任务
void fall_detect_update_health_data(Health_Data_t *data); // 更新健康数据（供外部调用）
extern Health_Data_t g_health_data;           // 全局健康数据（供其他模块访问）

#endif