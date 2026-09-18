#include "EXIT_KEY.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 核心修改：把 uint8_t 改为 OLED_Display_Mode
OLED_Display_Mode oled_display_mode = OLED_MODE_NORMAL;
bool oled_show_alarm_flag = false;

static TickType_t key_alarm_press_start = 0;
static bool alarm_flag_set = false; // 新增：标记告警已触发，避免重复置位

void exit_init(i2c_port_t i2c_num, gpio_num_t sda_pin, gpio_num_t scl_pin) {
    xl9555_init(i2c_num, sda_pin, scl_pin);
}

void exit_check_key_and_switch_mode(void) {
    TickType_t now = xTaskGetTickCount();

    // ---------------------
    // 原来的按键 KEY0 切换模式
    // ---------------------
    static TickType_t last_key_time = 0;
    bool key0 = xl9555_check_key(KEY_XL9555_GROUP, KEY_XL9555_PIN);
    if (key0 && (now - last_key_time) > pdMS_TO_TICKS(200)) {
        // 枚举类型切换：替代 ^1，更直观且避免类型隐患
        oled_display_mode = (oled_display_mode == OLED_MODE_NORMAL) ? OLED_MODE_TEST : OLED_MODE_NORMAL;
        oled_show_alarm_flag = false;
        alarm_flag_set = false; // 切换模式时重置告警标记
        last_key_time = now;
    }

    // ---------------------
    // 新增 KEY1 长按3秒 → 异常警告
    // ---------------------
    bool key1 = xl9555_check_key(KEY_ALARM_GROUP, KEY_ALARM_PIN);

    if (key1) {
        if (key_alarm_press_start == 0) {
            key_alarm_press_start = now;
            alarm_flag_set = false; // 按下时重置标记
        }

        // 长按满3秒 且 未触发过警告
        if (!alarm_flag_set && (now - key_alarm_press_start >= pdMS_TO_TICKS(KEY_LONG_PRESS_MS))) {
            oled_show_alarm_flag = true;
            alarm_flag_set = true; // 标记已触发，避免重复置位
        }
    } else {
        // 松开按键：重置计时和标记
        key_alarm_press_start = 0;
        alarm_flag_set = false;
        // 可选：松开后是否取消警告？如需保留警告则注释下面一行
        oled_show_alarm_flag = false;
    }
}