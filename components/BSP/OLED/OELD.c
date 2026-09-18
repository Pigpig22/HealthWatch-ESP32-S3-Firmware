#include "OLED.h"
#include "OLED_font.h"

// OLED显存缓冲区（128*64/8=1024字节）
uint8_t OLED_Buffer[128 * 64 / 8];

// ==================== 软件I2C基础函数 ====================
void I2C_Init(void)
{
    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << OLED_SDA_PIN) | (1ULL << OLED_SCL_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, // 开漏输出
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&conf);
    // 初始化引脚电平
    gpio_set_level(OLED_SDA_PIN, 1);
    gpio_set_level(OLED_SCL_PIN, 1);
}

void I2C_Start(void)
{
    gpio_set_level(OLED_SDA_PIN, 1);
    gpio_set_level(OLED_SCL_PIN, 1);
    I2C_DELAY();
    gpio_set_level(OLED_SDA_PIN, 0); // SDA拉低表示起始
    I2C_DELAY();
    gpio_set_level(OLED_SCL_PIN, 0); // SCL拉低，开始传输数据
}

void I2C_Stop(void)
{
    gpio_set_level(OLED_SDA_PIN, 0);
    I2C_DELAY();
    gpio_set_level(OLED_SCL_PIN, 1); // SCL拉高
    I2C_DELAY();
    gpio_set_level(OLED_SDA_PIN, 1); // SDA拉高表示停止
    I2C_DELAY();
}

void I2C_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        gpio_set_level(OLED_SCL_PIN, 0);
        I2C_DELAY();
        // 发送最高位
        gpio_set_level(OLED_SDA_PIN, (data & 0x80) ? 1 : 0);
        data <<= 1; // 左移，准备下一位
        I2C_DELAY();
        gpio_set_level(OLED_SCL_PIN, 1); // 拉高SCL，让从机读取数据
        I2C_DELAY();
    }
    // 读取ACK（SSH1106必须，否则通信异常）
    gpio_set_level(OLED_SCL_PIN, 0);
    gpio_set_direction(OLED_SDA_PIN, GPIO_MODE_INPUT);
    I2C_DELAY();
    gpio_set_level(OLED_SCL_PIN, 1);
    I2C_DELAY();
    // 恢复SDA为输出模式
    gpio_set_direction(OLED_SDA_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(OLED_SDA_PIN, 1);
    gpio_set_level(OLED_SCL_PIN, 0);
    I2C_DELAY();
}

// 写命令（SSH1106命令模式）
void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR << 1); // 设备地址+写位
    I2C_WriteByte(0x00);           // 命令寄存器
    I2C_WriteByte(cmd);            // 具体命令
    I2C_Stop();
}

// 写数据（SSH1106数据模式）
void OLED_WriteData(uint8_t data)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR << 1); // 设备地址+写位
    I2C_WriteByte(0x40);           // 数据寄存器
    I2C_WriteByte(data);           // 具体数据
    I2C_Stop();
}

// ==================== OLED核心功能函数 ====================
// SSH1106 OLED 初始化（优化版：稳定、无冗余、时序规范、修复偏移）
void OLED_Init(void)
{
    // 1. 初始化硬件接口
    I2C_Init();

    // 2. 上电稳定延时（SSD1306/SH1106 标准推荐延时）
    esp_rom_delay_us(100000);  // 100ms 足够稳定，无需500ms

    // ==================== SH1106 标准初始化序列 ====================
    OLED_WriteCmd(0xAE);        // 关闭显示（初始化第一步）

    OLED_WriteCmd(0xD5);        // 时钟分频设置
    OLED_WriteCmd(0x80);        // 分频=1，频率默认（最优）

    OLED_WriteCmd(0xA8);        // 多路复用率
    OLED_WriteCmd(0x3F);        // 64行（1/64 Duty）

    OLED_WriteCmd(0xD3);        // 显示偏移（修复整屏偏移关键）
    OLED_WriteCmd(0x00);        // 偏移=0，无位移

    OLED_WriteCmd(0x40);        // 显示起始行 = 0

    OLED_WriteCmd(0xA1);        // 段重映射（0xA1 正常 / 0xA0 镜像）
    OLED_WriteCmd(0xC8);        // COM扫描方向（0xC8 正常 / 0xC0 反向）

    OLED_WriteCmd(0xDA);        // COM引脚硬件配置
    OLED_WriteCmd(0x12);        // 64行屏标准配置

    OLED_WriteCmd(0x81);        // 对比度控制
    OLED_WriteCmd(0xCF);        // 推荐值 0xCF（比0xFF更柔和、不发烫）

    OLED_WriteCmd(0xD9);        // 预充电周期
    OLED_WriteCmd(0xF1);        // 最优配置

    OLED_WriteCmd(0xDB);        // VCOMH 电压
    OLED_WriteCmd(0x30);        // SH1106 推荐 0x30（比0x40对比度更均匀）

    OLED_WriteCmd(0xA4);        // 恢复显存显示（非全屏点亮）
    OLED_WriteCmd(0xA6);        // 正常显示模式（非反色）

    OLED_WriteCmd(0x8D);        // 电荷泵使能（**必选，否则黑屏**）
    OLED_WriteCmd(0x14);        // 开启电荷泵

    OLED_WriteCmd(0xAF);        // 开启显示（初始化最后一步）
    // =================================================================

    // 3. 清屏 + 刷新（确保上电无杂点）
    OLED_Clear();
    OLED_Refresh();
}
// 真正清屏（显存全部填 0）
void OLED_Clear(void)
{
    // 整块显存清零，彻底无残留
    for (int i = 0; i < 1024; i++)
    {
        OLED_Buffer[i] = 0x00;
    }
}

// 【完美版】刷新显存到屏幕 → 解决右边残留、错位、残影
void OLED_Refresh(void)
{
    uint8_t page, col;

    for (page = 0; page < 8; page++)
    {
        // 1. 设置页地址
        OLED_WriteCmd(0xB0 + page);

        // 2. 强制从 0 列开始（标准！不会错位）
        OLED_WriteCmd(0x00);  // 低4位
        OLED_WriteCmd(0x10);  // 高4位

        // 3. 连续写入 128 列（必须完整写，才能消除右边残留）
        for (col = 0; col < 128; col++)
        {
            OLED_WriteData(OLED_Buffer[page * 128 + col]);
        }
    }
}

/// 显示6x8字符串（适配自定义字库+修复越界/索引问题）
void OLED_ShowString(uint8_t x, uint8_t page, const char *str)
{
    // 临时变量保存起始列，避免修改传入的x
    uint8_t current_x = x;
    
    // 遍历字符串直到结束符
    while (*str != '\0')
    {
        uint8_t font_index = 0;
        
        // 1. 适配你的6x8字库索引规则（核心修复）
        // 字库索引：0=空格,1=0,2=1,3=2,4=3,5=4,6=5,7=6,8=7,9=8,10=9,11=:
        if (*str == ' ') {
            font_index = 0;          // 空格→索引0
        } else if (*str >= '0' && *str <= '9') {
            font_index = (*str - '0') + 1; // 数字'0'→1, '1'→2, ..., '9'→10
        } else if (*str == ':') {
            font_index = 11;         // 冒号→索引11
        } else {
            font_index = 0;          // 未知字符显示空格
        }

        // 2. 越界保护（先判断再写入，避免无效操作）
        // 6x8字符占6列，current_x+5是最后一列，超出则停止
        if (current_x + 5 >= 128 || page >= 8) {
            break;
        }
        // 字库索引越界（比如超过冒号的11），显示空格
        if (font_index >= sizeof(oled_font_6x8)/sizeof(oled_font_6x8[0])) {
            font_index = 0;
        }

        // 3. 写入当前字符的6x8点阵到显存
        for (uint8_t i = 0; i < 6; i++)
        {
            OLED_Buffer[current_x + i + page * 128] = oled_font_6x8[font_index][i];
        }

        // 4. 列偏移：每个字符间隔6列（可改7增加字符间距）
        current_x += 6;
        // 字符串指针后移
        str++;
    }
}
// 显示6x8单个字符（适配自定义字库+修复显示问题）
void OLED_ShowChar6x8(uint8_t x, uint8_t page, char ch)
{
    // 1. 越界保护：6x8字符占6列，超出屏幕直接返回
    if (x + 5 >= 128 || page >= 8) return;

    // 2. 核心修复：适配你的自定义字库索引规则
    uint8_t font_index = 0;
    if (ch == ' ') {
        font_index = 0;          // 空格 → 字库索引0
    } else if (ch >= '0' && ch <= '9') {
        font_index = (ch - '0') + 1; // 数字'0'→1, '1'→2, ..., '9'→10
    } else if (ch == ':') {
        font_index = 11;         // 冒号 → 字库索引11
    } else {
        font_index = 0;          // 未知字符显示空格
    }

    // 3. 字库索引越界保护（避免访问非法内存）
    if (font_index >= sizeof(oled_font_6x8)/sizeof(oled_font_6x8[0])) {
        font_index = 0;
    }

    // 4. 写入字符点阵到显存
    for (uint8_t i = 0; i < 6; i++)
    {
        OLED_Buffer[x + i + page * 128] = oled_font_6x8[font_index][i];
    }
}
// 显示6x8单个数字（0-9）
// 6x8数字显示函数（0-9）
// x：起始列（0-127），page：起始页（0-7），num：要显示的数字（0-9）
void OLED_ShowNum6x8(uint8_t x, uint8_t page, uint8_t num)
{
    // 1. 越界保护（6列宽度，避免超出屏幕）
    if(num > 9 || x + 5 >= 128 || page >= 8){
        return;
    }
    
    // 2. 数字0-9对应6x8字库的索引0-9（关键：索引不能错）
    uint8_t font_index = num;
    
    // 3. 从字库中读取点阵，逐列写入显存
    for(uint8_t i=0; i<6; i++){
        // 显存地址：x+i（列偏移） + page*128（页偏移）
        OLED_Buffer[x + i + page * 128] = oled_font_6x8[font_index][i];
    }
}
// 显示12x12汉字（修复越界/点阵错误）
void OLED_ShowChinese(uint8_t x, uint8_t page, uint8_t index)
{
    // 12x12 汉字 = 横向12列，纵向占 2 页（16像素），只显示中间12行
    // 越界保护：x+11 不能超过127，page+1 不能超过7
    if (index >= 43 || x > 115 || page > 6) return;

    for (uint8_t i = 0; i < 12; i++)
    {
        // 上半部分（当前页）
        OLED_Buffer[x + i + page * 128] = oled_font_chinese[index][i];
        // 下半部分（下一页）
        OLED_Buffer[x + i + (page + 1) * 128] = oled_font_chinese[index][i + 12];
    }
}

// 显示8x16单个字符（0-9 + 冒号:）num=10 就是冒号
void OLED_ShowNum8x16(uint8_t x, uint8_t page, uint8_t num)
{
    // 把 num>9 改成 num>10，允许 10（冒号）
    if (num > 13 || x + 7 >= 128 || page + 1 >= 8) return;

    for (uint8_t i = 0; i < 8; i++)
    {
        // 上半部分
        OLED_Buffer[x + i + page * 128] = oled_font_8x16[num][i];
        // 下半部分
        OLED_Buffer[x + i + (page + 1) * 128] = oled_font_8x16[num][i + 8];
    }
}
// 显示8x16时间（如12:34）
void OLED_ShowTime8x16(uint8_t x, uint8_t page, uint8_t hour, uint8_t min)
{
    // 小时十位+个位
    OLED_ShowNum8x16(x, page, hour / 10);
    OLED_ShowNum8x16(x + 8, page, hour % 10);
    // 冒号（x+16位置）
    uint8_t colon_x = x + 16;
    if (colon_x + 7 < 128 && page + 1 < 8)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            OLED_Buffer[colon_x + i + page * 128] = oled_font_8x16[10][i];
            OLED_Buffer[colon_x + i + (page + 1) * 128] = oled_font_8x16[10][i + 8];
        }
    }
    // 分钟十位+个位
    OLED_ShowNum8x16(x + 24, page, min / 10);
    OLED_ShowNum8x16(x + 32, page, min % 10);
}
/**
 * @brief 自动适配数字显示函数（默认8x16字库，自动对齐）
 * @param x_base 基准X坐标（右对齐：数字右边界；居中：数字中心）
 * @param page 显示页（0~7，8x16占2页，自动校验page+1越界）
 * @param num 要显示的数字（0~65535）
 * @param font_type 字库类型：0=6x8，1=8x16（默认1）
 * @param align 对齐方式：0=右对齐，1=居中（默认0）
 * @return 实际显示的起始X坐标
 */
uint8_t OLED_ShowNumAuto(uint8_t x_base, uint8_t page, uint16_t num, uint8_t font_type, uint8_t align)
{
    // 1. 参数默认值：未传参时(font_type/align=0xFF)，默认8x16、右对齐
    if (font_type > 1) font_type = 1; // 默认8x16
    if (align > 1) align = 0;         // 默认右对齐
    if (page >= 8) return 0;          // 页越界保护

    // 2. 提取数字的每一位（最大5位：0~65535）
    uint8_t digits[5] = {0};
    uint8_t digit_count = 0;
    uint16_t temp = num;

    if (temp == 0) {
        digits[0] = 0;
        digit_count = 1;
    } else {
        while (temp > 0 && digit_count < 5) {
            digits[digit_count++] = temp % 10;
            temp /= 10;
        }
    }

    // 3. 计算字符宽度（适配原有字库：8x16=8列，6x8=6列）
    uint8_t char_width = (font_type == 0) ? 6 : 8;
    uint8_t total_width = digit_count * char_width;

    // 4. 计算起始X坐标（右对齐/居中）
    uint8_t start_x = 0;
    if (align == 0) { // 右对齐：x_base是数字右边界
        start_x = x_base - total_width + 1;
    } else { // 居中对齐：x_base是数字中心
        start_x = x_base - (total_width / 2);
    }

    // 5. 严格越界保护（适配128x64屏幕）
    if (start_x > 127) start_x = 0;
    uint8_t end_x = start_x + total_width - 1;
    if (end_x >= 128) {
        start_x = 128 - total_width;
        start_x = (start_x < 0) ? 0 : start_x;
    }

    // 6. 8x16字库占2页，校验page+1是否越界
    uint8_t page_valid = (font_type == 1 && (page + 1) >= 8) ? 0 : 1;
    if (!page_valid) return 0;

    // 7. 显示数字（高位在前，适配原有OLED_ShowNum6x8/8x16函数）
    uint8_t current_x = start_x;
    for (int8_t i = digit_count - 1; i >= 0; i--) {
        if (font_type == 0) {
            OLED_ShowNum6x8(current_x, page, digits[i]);
        } else {
            OLED_ShowNum8x16(current_x, page, digits[i]);
        }
        current_x += char_width;
    }

    return start_x;
}

/**
 * @brief 极简版自动显示数字（默认8x16、右对齐，仅需传坐标和数字）
 * @param x_right 数字右边界X坐标
 * @param page 显示页（0~6，因8x16占2页）
 * @param num 要显示的数字（0~65535）
 */
void OLED_ShowNumAutoSimple(uint8_t x_right, uint8_t page, uint16_t num)
{
    OLED_ShowNumAuto(x_right, page, num, 1, 0); // 强制8x16、右对齐
}
// 调试函数：绘制坐标网格，定位位置偏移问题
void OLED_TestGrid(void)
{
    OLED_Clear();
    // 绘制竖线（每16列一条）
    for(uint8_t x=0;x<128;x+=16){
        for(uint8_t y=0;y<64;y++){
            uint8_t page = y/8;
            uint8_t bit = y%8;
            OLED_Buffer[x + page*128] |= (1<<bit);
        }
    }
    // 绘制横线（每8行一条）
    for(uint8_t y=0;y<64;y+=8){
        uint8_t page = y/8;
        for(uint8_t x=0;x<128;x++){
            OLED_Buffer[x + page*128] = 0xFF;
        }
    }
    OLED_Refresh();
}