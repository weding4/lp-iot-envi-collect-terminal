#include "OLED.h"
#include "OLED_Font.h"
#include "i2c.h"          // 提供 hi2c1
#include <stdio.h>
#include <string.h>

#define OLED_ADDR  (0x78)  // 7位地址左移1位，通常为 0x78

// 发送命令
static void OLED_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};  // 控制字节：0x00 表示命令
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 10);
}

// 发送数据
static void OLED_WriteData(uint8_t dat)
{
    uint8_t buf[2] = {0x40, dat};  // 控制字节：0x40 表示数据
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 10);
}

// SSD1306 初始化序列
void OLED_Init(void)
{
    HAL_Delay(100);   // 等待上电稳定

    OLED_WriteCmd(0xAE); // display off
    OLED_WriteCmd(0x20); // Set Memory Addressing Mode
    OLED_WriteCmd(0x00); // horizontal addressing mode
    OLED_WriteCmd(0xB0); // Set Page Start Address for Page Addressing Mode
    OLED_WriteCmd(0xC8); // COM Output Scan Direction: remapped mode
    OLED_WriteCmd(0x00); // low column start address
    OLED_WriteCmd(0x10); // high column start address
    OLED_WriteCmd(0x40); // start line address
    OLED_WriteCmd(0x81); // contrast control
    OLED_WriteCmd(0xFF); // max contrast
    OLED_WriteCmd(0xA1); // segment re-map (column 127 mapped to SEG0)
    OLED_WriteCmd(0xA6); // normal display (not inverted)
    OLED_WriteCmd(0xA8); // multiplex ratio
    OLED_WriteCmd(0x3F); // duty = 1/64
    OLED_WriteCmd(0xA4); // Entire Display ON resume
    OLED_WriteCmd(0xD3); // Set Display Offset
    OLED_WriteCmd(0x00); // no offset
    OLED_WriteCmd(0xD5); // set display clock divide ratio/oscillator frequency
    OLED_WriteCmd(0xF0);
    OLED_WriteCmd(0xD9); // set pre-charge period
    OLED_WriteCmd(0x22);
    OLED_WriteCmd(0xDA); // set COM pins hardware configuration
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0xDB); // set vcomh deselect level
    OLED_WriteCmd(0x20);
    OLED_WriteCmd(0x8D); // charge pump setting
    OLED_WriteCmd(0x14); // enable charge pump
    OLED_WriteCmd(0xAF); // display on

    OLED_Clear();
}

void OLED_Clear(void)
{
    uint8_t i, n;
    for(i = 0; i < 8; i++) {          // 8 页
        OLED_WriteCmd(0xB0 + i);      // 设置页地址
        OLED_WriteCmd(0x00);          // 列低地址
        OLED_WriteCmd(0x10);          // 列高地址
        for(n = 0; n < 128; n++) {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_Fill(unsigned char fill_Data)
{
    uint8_t m, n;
    for(m = 0; m < 8; m++) {
        OLED_WriteCmd(0xB0 + m);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);
        for(n = 0; n < 128; n++) {
            OLED_WriteData(fill_Data);
        }
    }
}

void OLED_SetPos(unsigned char x, unsigned char y)
{
    OLED_WriteCmd(0xB0 + y);                 // 页地址（0~7）
    OLED_WriteCmd(((x & 0xF0) >> 4) | 0x10); // 列地址高4位
    OLED_WriteCmd(x & 0x0F);                 // 列地址低4位
}

void OLED_ShowChar(unsigned char x, unsigned char y, unsigned char chr)
{
    unsigned char c = chr - ' ';
    if(x > 122) x = 122;   // 超出屏幕限制
    if(y > 7)   y = 7;

    OLED_SetPos(x, y);
    for(uint8_t i = 0; i < 6; i++) {
        OLED_WriteData(F6x8[c][i]);
    }
}

void OLED_ShowString(unsigned char x, unsigned char y, const char *str)
{
    while(*str) {
        OLED_ShowChar(x, y, *str);
        x += 6;
        if(x > 122) {
            x = 0;
            y++;
        }
        str++;
    }
}

void OLED_ShowNum(unsigned char x, unsigned char y, int num, unsigned char len)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%*d", len, num);
    OLED_ShowString(x, y, buf);
}

void OLED_ShowFloat(unsigned char x, unsigned char y, float num)
{
    char buf[10];
    snprintf(buf, sizeof(buf), "%.1f", num);
    OLED_ShowString(x, y, buf);
}

// 专门显示环境数据
void OLED_Display_Env(float temp, float light)
{
    OLED_ShowString(0, 1, "Temp:");
    OLED_ShowFloat(36, 1, temp);
    OLED_ShowChar(36 + 6 * 4, 1, 'C');  // 后面接 'C'

    OLED_ShowString(0, 3, "Light:");
    OLED_ShowFloat(36, 3, light);
    OLED_ShowString(36 + 6 * 4, 3, "%");
}
