#include "motor.h"
#include "tim.h"
#include "main.h"
#include "stm32f1xx_hal.h"

/* USER CODE BEGIN 0 */
/* Wiring: PB0 -> TB6612 PWMA (TIM3_CH3, 50Hz); PB1 -> TB6612 AIN1 (fixed high = forward);
   AIN2 -> GND; STBY -> 3.3V. */
/* USER CODE END 0 */

void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PB1 = AIN1, fixed forward direction. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* AIN1 fixed high; AIN2 grounded by hardware; STBY tied high. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

    /* Ensure TIM3 channel 3 PWM is configured and running with 50Hz period. */
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
    Motor_Stop();
}

void Motor_SetState(MotorState state)
{
    switch(state) {
        case MOTOR_STOP:
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
            break;
        case MOTOR_FORWARD:
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            break;
        case MOTOR_REVERSE:
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
            break;
    }
}

void Motor_SetSpeed(uint8_t duty_percent)
{
    if (duty_percent > 100U) {
        duty_percent = 100U;
    }

    if (duty_percent == 0U) {
        Motor_Stop();
        return;
    }

    /* 50Hz: timer clock 72MHz / 72 = 1MHz; ARR 20000-1 => 1MHz / 20000 = 50Hz. */
    uint32_t pulse = (uint32_t)((MOTOR_PWM_PERIOD * duty_percent) / 100U);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pulse);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}

void Motor_Stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}
/* USER CODE END 0 */
