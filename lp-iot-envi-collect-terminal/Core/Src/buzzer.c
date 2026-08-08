#include "buzzer.h"
#include "gpio.h"
#include "stm32f1xx_hal.h"

void Buzzer_Init(void)
{
    // 1. 彻底释放 JTAG，确保 PB3/PB4/PA15 可用作普通 GPIO
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();   // 只保留 SWD (PA13/PA14)

    // 2. 强制定制蜂鸣器引脚配置（防止 CubeMX 配置被覆盖）
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BUZZER_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;   // 下拉，上电瞬间低电平
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

    // 3. 确保初始状态为关闭
    Buzzer_Off();
}

void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On();
    HAL_Delay(ms);
    Buzzer_Off();
}
