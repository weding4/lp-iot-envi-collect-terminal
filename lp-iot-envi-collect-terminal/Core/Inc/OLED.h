#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>
#include "i2c.h"

extern I2C_HandleTypeDef hi2c1;

#define OLED_I2C_ADDR  0x78U
#define OLED_WIDTH     128U
#define OLED_HEIGHT    64U
#define OLED_PAGE_COUNT 8U

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Fill(uint8_t fill_Data);
void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t y, int num, uint8_t len);
void OLED_ShowFloat(uint8_t x, uint8_t y, float num);
void OLED_Display_Env(float temp, float light);

#endif
