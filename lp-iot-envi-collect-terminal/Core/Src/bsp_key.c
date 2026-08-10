#include "bsp_key.h"
#include "main.h"
#include "stm32f1xx_hal.h"

extern volatile uint8_t key_pressed;

static volatile uint8_t key_flag = 0;

void BSP_Key_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI4_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}

uint8_t BSP_Key_IsPressed(void)
{
    if (key_flag)
    {
        key_flag = 0;
        key_pressed = 0;
        return 1;
    }
    return 0;
}
