#include "OLED.h"
#include "oledfont.h"
#include <stdio.h>
#include <string.h>

#define OLED_ADDR  (0x78U)

static void OLED_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00U, cmd};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 10);
}

static void OLED_WriteData(uint8_t dat)
{
    uint8_t buf[2] = {0x40U, dat};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 10);
}

void OLED_Init(void)
{
    HAL_Delay(100);

    OLED_WriteCmd(0xAE);  // Display off
    OLED_WriteCmd(0xD5);  // Set display clock divide ratio/oscillator frequency
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8);  // Multiplex ratio
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xD3);  // Set display offset
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40);  // Set start line
    OLED_WriteCmd(0x8D);  // Charge pump
    OLED_WriteCmd(0x14);
    OLED_WriteCmd(0x20);  // Memory addressing mode
    OLED_WriteCmd(0x00);  // Horizontal mode
    OLED_WriteCmd(0xA1);  // Segment remap
    OLED_WriteCmd(0xC8);  // COM scan direction
    OLED_WriteCmd(0xDA);  // COM pins hardware config
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81);  // Contrast
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xA6);  // Normal display
    OLED_WriteCmd(0xAF);  // Display on

    OLED_Clear();
}

void OLED_Clear(void)
{
    uint8_t page;
    for (page = 0; page < OLED_PAGE_COUNT; page++) {
        OLED_SetPos(0, page);
        for (uint8_t col = 0; col < OLED_WIDTH; col++) {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_Fill(uint8_t fill_Data)
{
    uint8_t page;
    for (page = 0; page < OLED_PAGE_COUNT; page++) {
        OLED_SetPos(0, page);
        for (uint8_t col = 0; col < OLED_WIDTH; col++) {
            OLED_WriteData(fill_Data);
        }
    }
}

void OLED_SetPos(uint8_t x, uint8_t y)
{
    if (y >= OLED_PAGE_COUNT) {
        return;
    }
    if (x >= OLED_WIDTH) {
        return;
    }

    OLED_WriteCmd(0xB0 + y);
    OLED_WriteCmd((x & 0x0F));
    OLED_WriteCmd(((x >> 4) & 0x0F) | 0x10);
}

void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr)
{
    if (chr < 0x20 || chr > 0x7E) {
        chr = '?';
    }

    uint8_t idx = (uint8_t)(chr - 0x20);
    uint8_t cx = x;
    uint8_t cy = y;

    if (cx > OLED_WIDTH - 6) {
        cx = 0;
        cy++;
    }

    if (cy >= OLED_PAGE_COUNT) {
        cy = 0;
    }

    OLED_SetPos(cx, cy);
    for (uint8_t i = 0; i < 6; i++) {
        OLED_WriteData(F6x8[idx][i]);
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str)
{
    while (*str) {
        OLED_ShowChar(x, y, *str);
        x += 6;
        if (x > OLED_WIDTH - 6) {
            x = 0;
            y++;
            if (y >= OLED_PAGE_COUNT) {
                y = 0;
            }
        }
        str++;
    }
}

void OLED_ShowNum(uint8_t x, uint8_t y, int num, uint8_t len)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%*d", len, num);
    OLED_ShowString(x, y, buf);
}

void OLED_ShowFloat(uint8_t x, uint8_t y, float num)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", num);
    OLED_ShowString(x, y, buf);
}

void OLED_Display_Env(float temp, float light)
{
    OLED_Clear();

    char line1[16];
    char line2[16];
    char line3[16];

    snprintf(line1, sizeof(line1), "Temp:%.1f C", temp);
    snprintf(line2, sizeof(line2), "Light:%.1f %%", light);
    snprintf(line3, sizeof(line3), "Status: Normal");

    OLED_ShowString(0, 1, line1);
    OLED_ShowString(0, 3, line2);
    OLED_ShowString(0, 5, line3);
}
