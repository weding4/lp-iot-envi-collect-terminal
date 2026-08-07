#include "motor.h"
#include "gpio.h"  // Ìá¹©Òý½Åºê

void Motor_Init(void)
{
    Motor_SetState(MOTOR_STOP);
}

void Motor_SetState(MotorState state)
{
    switch(state) {
        case MOTOR_STOP:
            HAL_GPIO_WritePin(DC_MOTOR_IN1_GPIO_Port, DC_MOTOR_IN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(DC_MOTOR_IN2_GPIO_Port, DC_MOTOR_IN2_Pin, GPIO_PIN_RESET);
            break;
        case MOTOR_FORWARD:
            HAL_GPIO_WritePin(DC_MOTOR_IN1_GPIO_Port, DC_MOTOR_IN1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(DC_MOTOR_IN2_GPIO_Port, DC_MOTOR_IN2_Pin, GPIO_PIN_RESET);
            break;
        case MOTOR_REVERSE:
            HAL_GPIO_WritePin(DC_MOTOR_IN1_GPIO_Port, DC_MOTOR_IN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(DC_MOTOR_IN2_GPIO_Port, DC_MOTOR_IN2_Pin, GPIO_PIN_SET);
            break;
    }
}
