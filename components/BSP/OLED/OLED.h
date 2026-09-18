#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <stdint.h>
#include "driver/gpio.h"
#include <string.h>
#include "esp_rom_sys.h"  

// 引脚定义（可根据硬件修改）
#define OLED_SDA_PIN   GPIO_NUM_4
#define OLED_SCL_PIN   GPIO_NUM_5
#define OLED_ADDR      0x3C  // SSH1106默认地址，少数为0x3D

// I2C延时（适配SSH1106，增大延时避免通信错误）
#define I2C_DELAY()  esp_rom_delay_us(10)
// 新增：声明OLED显存缓冲区（供外部文件访问）
extern uint8_t OLED_Buffer[128 * 64 / 8];
// 16x16汉字索引定义
typedef enum {
    CHN_XIN = 0,    // 心
    CHN_LV = 1,     // 率
    CHN_XUE = 2,    // 血
    CHN_YANG = 3,   // 氧
    CHN_BAO = 4,    // 饱
    CHN_HE = 5,     // 和
    CHN_DU = 6,     // 度
    CHN_SHUI = 7,   // 睡
    CHN_MIAN = 8,   // 眠
    CHN_ZHI = 9,    // 质
    CHN_LIANG = 10, // 量
    CHN_SHI = 11,   // 时
    CHN_CHANG = 12, // 长
    CHN_JING = 13,  // 警
    CHN_GAO = 14,   // 告
    CHN_WEN = 15,   // 温
    CHN_SHUAI = 16, // 摔
    CHN_DAO = 17,   // 倒
    CHN_YI = 18,    // 异
    CHN_CHANG2 = 19,// 常
    CHN_EXCLAM = 20 // ！
} OLED_Chinese_Index;

// 核心函数声明
void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_WriteByte(uint8_t data);
void OLED_WriteCmd(uint8_t cmd);
void OLED_WriteData(uint8_t data);

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);
void OLED_ShowString(uint8_t x, uint8_t page, const char *str);
void OLED_ShowChar6x8(uint8_t x, uint8_t page, char ch);
void OLED_ShowNum6x8(uint8_t x, uint8_t page, uint8_t num);
void OLED_ShowChinese(uint8_t x, uint8_t page, uint8_t index);
void OLED_ShowNum8x16(uint8_t x, uint8_t page, uint8_t num);
void OLED_ShowTime8x16(uint8_t x, uint8_t page, uint8_t hour, uint8_t min);
// 自动适配数字显示函数（默认8x16）
uint8_t OLED_ShowNumAuto(uint8_t x_base, uint8_t page, uint16_t num, uint8_t font_type, uint8_t align);
// 极简版：仅传坐标+数字（默认8x16、右对齐）
void OLED_ShowNumAutoSimple(uint8_t x_right, uint8_t page, uint16_t num);
// 调试函数：绘制坐标网格，定位位置问题
void OLED_TestGrid(void);

#endif