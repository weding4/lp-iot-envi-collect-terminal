#ifndef __BSP_KEY_H
#define __BSP_KEY_H
#include <stdint.h>

void BSP_Key_Init(void);
uint8_t BSP_Key_IsPressed(void);   // 返回1表示按下，读取后自动清零
#endif
