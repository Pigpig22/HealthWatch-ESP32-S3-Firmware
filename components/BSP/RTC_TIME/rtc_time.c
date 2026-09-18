#include "rtc_time.h"


static const char *TAG = "rtc";
static time_t s_rtc_time = 0; // RTC基准时间
static TimerHandle_t s_rtc_timer = NULL; // 1秒定时器，用于离线走时

// 定时器回调：每秒更新RTC时间
static void rtc_timer_callback(TimerHandle_t xTimer) {
    s_rtc_time++;
}

void my_rtc_time_init(time_t init_time) {
    // 初始化基准时间（NTP同步后的值）
    s_rtc_time = init_time;
    
    // 创建1秒周期定时器（用于离线走时）
    if (s_rtc_timer == NULL) {
        s_rtc_timer = xTimerCreate(
            "rtc_timer",
            pdMS_TO_TICKS(1000), // 1秒触发一次
            pdTRUE,              // 周期触发
            NULL,
            rtc_timer_callback
        );
    }
    
    // 启动定时器（无论在线/离线都运行，保证时间持续更新）
    if (xTimerStart(s_rtc_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "RTC定时器启动失败");
    }
    ESP_LOGI(TAG, "RTC本地时钟初始化完成，基准时间：%s", ctime(&s_rtc_time));
}

time_t my_rtc_time_get(void) {
    return s_rtc_time;
}

void my_rtc_time_update(time_t new_time) {
    // NTP同步成功后，校准RTC时间
    if (new_time > s_rtc_time) {
        s_rtc_time = new_time;
        ESP_LOGI(TAG, "RTC时间校准为：%s", ctime(&s_rtc_time));
    }
}