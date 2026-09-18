#ifndef _XL9555_H
#define _XL9555_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_err.h"

// XL9555 I2C地址（A0/A1/A2接地 → 0x20）
#define XL9555_I2C_ADDR    0x20
// 匹配原理图：XL9555的I2C引脚
#define XL9555_I2C_NUM I2C_NUM_0
#define XL9555_SDA_PIN GPIO_NUM_41
#define XL9555_SCL_PIN GPIO_NUM_42
// XL9555寄存器定义（严格对应IO0/IO1分组）
#define XL9555_INPUT_IO0   0x00  // IO0组输入寄存器（IO0_0~IO0_7）
#define XL9555_INPUT_IO1   0x01  // IO1组输入寄存器（IO1_0~IO1_7）
#define XL9555_OUTPUT_IO0  0x02  // IO0组输出寄存器
#define XL9555_OUTPUT_IO1  0x03  // IO1组输出寄存器
#define XL9555_INVERT_IO0  0x04  // IO0组极性反转
#define XL9555_INVERT_IO1  0x05  // IO1组极性反转
#define XL9555_CONFIG_IO0  0x06  // IO0组配置（1=输入，0=输出）
#define XL9555_CONFIG_IO1  0x07  // IO1组配置

/**
 * @brief 初始化XL9555
 * @param i2c_num I2C端口号（I2C_NUM_0/I2C_NUM_1）
 * @param sda_pin SDA引脚（原理图=IO41）
 * @param scl_pin SCL引脚（原理图=IO42）
 * @return esp_err_t 错误码
 */
esp_err_t xl9555_init(i2c_port_t i2c_num, gpio_num_t sda_pin, gpio_num_t scl_pin);

/**
 * @brief 读取XL9555指定组的输入状态
 * @param io_group 组号（0=IO0组，1=IO1组）
 * @param value 读取到的8位值（每bit对应1个IO）
 * @return esp_err_t 错误码
 */
esp_err_t xl9555_read_input(uint8_t io_group, uint8_t *value);

/**
 * @brief 检测指定IO的按键状态（消抖）
 * @param io_group 组号（0=IO0组，1=IO1组）
 * @param io_pin 引脚号（0-7）
 * @return bool true=按键按下，false=按键释放
 */
bool xl9555_check_key(uint8_t io_group, uint8_t io_pin);

#endif