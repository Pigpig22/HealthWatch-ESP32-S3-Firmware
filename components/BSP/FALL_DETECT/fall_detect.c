#include "fall_detect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"
#include <math.h>

// 全局健康数据（整合加速度、角度、心率、血氧）
Health_Data_t g_health_data = {0};

// 静态函数：核心摔倒检测算法
static void fall_detect_core(Health_Data_t *data) {
    static uint8_t fall_sample_cnt = 0;
    
    // 1. 计算合加速度（判定是否有剧烈冲击）
    float accel_total = sqrt(pow(data->accel_x, 2) + pow(data->accel_y, 2) + pow(data->accel_z, 2));
    // 2. 判定角度是否超出正常范围（摔倒后姿态异常）
    float pitch_abs = fabs(data->pitch);
    float roll_abs = fabs(data->roll);

    // 摔倒判定逻辑：合加速度超阈值 + 角度超阈值（连续多次采样确认）
    if (accel_total > FALL_ACCEL_THRESHOLD && (pitch_abs > FALL_ANGLE_THRESHOLD || roll_abs > FALL_ANGLE_THRESHOLD)) {
        fall_sample_cnt++;
        if (fall_sample_cnt >= FALL_SAMPLE_COUNT) {
            data->is_fall = 1; // 判定为摔倒
            fall_sample_cnt = 0;
            printf("[紧急] 检测到摔倒！合加速度：%.0f | 角度(P/R)：%.1f/%.1f | 心率：%d | 血氧：%d\n",
                   accel_total, pitch_abs, roll_abs, data->heart_rate, data->blood_oxygen);
        }
    } else {
        fall_sample_cnt = 0;
        data->is_fall = 0; // 未摔倒
    }
}

// 初始化摔倒检测（可扩展：初始化心率/血氧传感器）
void fall_detect_init(void) {
    // 初始化健康数据
    g_health_data.heart_rate = 0;
    g_health_data.blood_oxygen = 0;
    g_health_data.is_fall = 0;
    
    printf("摔倒检测模块初始化完成\n");
}

// 更新健康数据（供MPU6050/心率/血氧模块调用）
void fall_detect_update_health_data(Health_Data_t *data) {
    if (data != NULL) {
        // 更新加速度和角度
        g_health_data.accel_x = data->accel_x;
        g_health_data.accel_y = data->accel_y;
        g_health_data.accel_z = data->accel_z;
        g_health_data.pitch = data->pitch;
        g_health_data.roll = data->roll;
        // 可选：更新心率/血氧（外部模块调用时传入）
        if (data->heart_rate > 0) g_health_data.heart_rate = data->heart_rate;
        if (data->blood_oxygen > 0) g_health_data.blood_oxygen = data->blood_oxygen;
    }
}

// 摔倒检测任务（独立任务，可调整检测频率）
void fall_detect_task(void *arg) {
    fall_detect_init();
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待传感器初始化

    while (1) {
        // 执行摔倒检测核心逻辑
        fall_detect_core(&g_health_data);
        
        // 调试输出（可选）
        printf("[健康状态] 摔倒：%s | 心率：%d | 血氧：%d | 合加速度：%.0f\n",
               g_health_data.is_fall ? "是" : "否",
               g_health_data.heart_rate,
               g_health_data.blood_oxygen,
               sqrt(pow(g_health_data.accel_x, 2) + pow(g_health_data.accel_y, 2) + pow(g_health_data.accel_z, 2)));

        vTaskDelay(pdMS_TO_TICKS(FALL_DETECT_INTERVAL)); // 检测间隔
    }
}