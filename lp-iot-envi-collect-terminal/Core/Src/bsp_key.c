#include "bsp_key.h"
#include "main.h"   // 包含 HAL 库和引脚定义

static volatile uint8_t key_flag = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_4)
    {
        key_flag = 1;
    }
}

void BSP_Key_Init(void)
{
    // EXTI 初始化已在 MX_GPIO_Init() 中完成，这里可留空或使能中断
    // 确保 NVIC 已使能
}

uint8_t BSP_Key_IsPressed(void)
{
    if (key_flag)
    {
        key_flag = 0;
        return 1;
    }
    return 0;
}
