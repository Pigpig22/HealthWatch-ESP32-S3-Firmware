#include "ntp_time.h"
#include "rtc_time.h" // 引入RTC头文件
#include <time.h>
#include <string.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_err.h"

static const char *TAG = "ntp";

void ntp_init(void) {
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_setservername(2, "time1.aliyun.com");
    esp_sntp_init();
    
    setenv("TZ", "CST-8", 1);
    tzset();
    
    ESP_LOGI(TAG, "NTP初始化完成 (UTC+8)，开始同步时间...");
}

void ntp_get_time(struct tm *tm) {
    time_t now = time(NULL);
    // 优先使用NTP时间；NTP未同步时，使用RTC本地时间
    if (now < 1000000000) { 
        now = my_rtc_time_get(); // 切到离线RTC时间
        ESP_LOGD(TAG, "使用离线RTC时间，时间戳：%ld", now);
    } else {
        my_rtc_time_update(now); // NTP同步成功，校准RTC时间
    }
    // 转换为本地时间（东八区）
    localtime_r(&now, tm);
}