#ifndef _EXIT_KEY_H
#define _EXIT_KEY_H

#include "xl9555.h"
#include "driver/i2c.h"  // 补充i2c_port_t等类型定义

#define KEY_XL9555_GROUP 1    
#define KEY_XL9555_PIN   7     

// 新增告警按键（你实际硬件是哪个就改哪个）
#define KEY_ALARM_GROUP   1
#define KEY_ALARM_PIN     6    // <-- 这里必须和你硬件一致

// 长按3秒
#define KEY_LONG_PRESS_MS 3000

// 1. 定义枚举类型（替代原宏定义，统一类型）
typedef enum {
    OLED_MODE_NORMAL = 0,    // 正常时间显示模式
    OLED_MODE_TEST = 1       // 测试模式（心率/血氧等）
} OLED_Display_Mode;

// 2. 修正外部变量声明：改为枚举类型
extern OLED_Display_Mode oled_display_mode;
extern bool oled_show_alarm_flag;

void exit_init(i2c_port_t i2c_num, gpio_num_t sda_pin, gpio_num_t scl_pin);
void exit_check_key_and_switch_mode(void);

#endif