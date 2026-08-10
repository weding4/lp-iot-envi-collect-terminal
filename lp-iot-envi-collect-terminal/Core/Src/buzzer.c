#include "buzzer.h"
#include "gpio.h"
#include "stm32f1xx_hal.h"

void Buzzer_Init(void)
{
    // 1. �������ͷ� JTAG��ȷ�� PB3/PB4/PA15 ��������ͨ GPIO
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();   // ֻ���� SWD (PA13/PA14)

    // 2. ǿ�ƶ��Ʒ������������ã���ֹ CubeMX ���ñ����ǣ�
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BUZZER_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;   // �������ϵ�˲��͵�ƽ
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

    // 3. ȷ����ʼ״̬Ϊ idle: ���ߵ�ƽ
    // ���ݱ�Ƭ������Ϊ��ƽ��Ч������ˣ���ƽ=��ʱ=��
    Buzzer_Off();
}

/**
  * @brief  Generate an active-low buzzer alarm.
  *         The idle state is high (GPIO_SET). When a threshold violation
  *         is latched, this routine pulls the output low to turn the buzzer on.
  */
void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
}

void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On();
    HAL_Delay(ms);
    Buzzer_Off();
}
