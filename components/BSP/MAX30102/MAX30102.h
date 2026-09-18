#ifndef MAX30102_H
#define MAX30102_H

#include "driver/i2c.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX30102_I2C_SDA_PIN  GPIO_NUM_18
#define MAX30102_I2C_SCL_PIN  GPIO_NUM_19
#define MAX30102_I2C_NUM      I2C_NUM_1
#define MAX30102_ADDR         0x57

esp_err_t max30102_init(void);
esp_err_t max30102_read_fifo(uint32_t *ir, uint32_t *red);
bool max30102_is_wrist_detected(void);
uint8_t max30102_get_heart_rate(void);
uint8_t max30102_get_spo2(void);  // 新增血氧获取函数

#ifdef __cplusplus
}
#endif

#endif