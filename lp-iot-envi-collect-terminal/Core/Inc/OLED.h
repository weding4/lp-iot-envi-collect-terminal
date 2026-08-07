#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

// OLED 初始化
void OLED_Init(void);

// 清屏
void OLED_Clear(void);

// 全屏填充
void OLED_Fill(unsigned char fill_Data);

// 设置光标位置（页地址 0~7，列地址 0~127）
void OLED_SetPos(unsigned char x, unsigned char y);

// 显示一个字符 (6x8)
void OLED_ShowChar(unsigned char x, unsigned char y, unsigned char chr);

// 显示字符串
void OLED_ShowString(unsigned char x, unsigned char y, const char *str);

// 显示数字（直接转字符串）
void OLED_ShowNum(unsigned char x, unsigned char y, int num, unsigned char len);

// 显示浮点数（保留 1 位小数）
void OLED_ShowFloat(unsigned char x, unsigned char y, float num);

// 显示 ADC 采集数据界面
void OLED_Display_Env(float temp, float light);

#endif
