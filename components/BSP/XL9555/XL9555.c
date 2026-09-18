#include "XL9555.h"

static i2c_port_t _i2c_num;

// I2C写寄存器（缩短超时到10ms）
static esp_err_t xl9555_write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (XL9555_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    // 超时从100ms→10ms，减少I2C通信卡顿
    esp_err_t ret = i2c_master_cmd_begin(_i2c_num, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// I2C读寄存器（缩短超时到10ms）
static esp_err_t xl9555_read_reg(uint8_t reg, uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (XL9555_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (XL9555_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    // 超时从100ms→10ms
    esp_err_t ret = i2c_master_cmd_begin(_i2c_num, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// 初始化XL9555（不变）
esp_err_t xl9555_init(i2c_port_t i2c_num, gpio_num_t sda_pin, gpio_num_t scl_pin) {
    _i2c_num = i2c_num;
    
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(i2c_num, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_num, I2C_MODE_MASTER, 0, 0, 0));
    
    xl9555_write_reg(XL9555_CONFIG_IO0, 0xFF);
    xl9555_write_reg(XL9555_CONFIG_IO1, 0xFF);
    
    return ESP_OK;
}

// 读取指定组输入状态（不变）
esp_err_t xl9555_read_input(uint8_t io_group, uint8_t *value) {
    if (io_group > 1) return ESP_ERR_INVALID_ARG;
    return xl9555_read_reg(io_group == 0 ? XL9555_INPUT_IO0 : XL9555_INPUT_IO1, value);
}

bool xl9555_check_key(uint8_t io_group, uint8_t io_pin) {
    if (io_pin > 7 || io_group > 1) return false;
    
    uint8_t val1, val2, val3;
    // 三次读取验证（微秒级间隔，无硬延时）
    xl9555_read_input(io_group, &val1);
    xl9555_read_input(io_group, &val2);
    xl9555_read_input(io_group, &val3);
    
    // 三次状态一致才判定为按下（消抖）
    bool state1 = !(val1 & (1 << io_pin));
    bool state2 = !(val2 & (1 << io_pin));
    bool state3 = !(val3 & (1 << io_pin));
    
    return (state1 && state2 && state3);
}