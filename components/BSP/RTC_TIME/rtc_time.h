#ifndef RTC_TIME_H
#define RTC_TIME_H

#include "time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
/**
 * @brief 初始化RTC本地时钟（离线备用）
 * @param init_time 初始时间（NTP同步成功后传入，作为离线基准）
 */
void my_rtc_time_init(time_t init_time);

/**
 * @brief 获取RTC本地时间（离线走时）
 * @return 当前RTC时间戳
 */
time_t my_rtc_time_get(void);

/**
 * @brief 更新RTC时间（NTP同步成功后校准）
 * @param new_time NTP同步后的时间戳
 */
void my_rtc_time_update(time_t new_time);

#endif // RTC_TIME_H
