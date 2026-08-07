#include "servo.h"
#include "tim.h"

// 周期20000对应50Hz，脉宽范围 500~2500 (0.5ms~2.5ms)
#define PWM_MIN  500
#define PWM_MAX  2500

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    Servo_SetAngle(90);  // 初始中位
}

void Servo_SetAngle(uint8_t angle)
{
    if(angle > 180) angle = 180;
    uint16_t pulse = PWM_MIN + (uint32_t)(PWM_MAX - PWM_MIN) * angle / 180;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);
}
