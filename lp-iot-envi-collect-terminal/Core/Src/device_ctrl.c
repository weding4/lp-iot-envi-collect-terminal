/**
  ******************************************************************************
  * @file    device_ctrl.c
  * @brief   设备控制适配层实现 (电机/蜂鸣器/SG90舵机)
  * @note    仅调用已有驱动接口，不修改任何驱动源码：
  *          motor.c   -> Motor_SetState / Motor_SetSpeed / Motor_Stop
  *          buzzer.c  -> Buzzer_On / Buzzer_Off
  *          servo.c   -> Servo_SetAngle
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "device_ctrl.h"

#include "motor.h"
#include "buzzer.h"
#include "servo.h"

/* 全局设备状态（静态初始化：电机停止、蜂鸣器关、舵机 0°） */
Device_State_t g_device_state = { 0U, 0U, 0U, 0U };

/* 默认电机占空比：JSON 未携带 duty 时启动电机使用 50% (5000/10000) */
#define MOTOR_DEFAULT_DUTY  5000U

void Motor_Set(uint8_t mode, uint16_t duty)
{
  uint8_t percent;

  /* 容错：duty 限制 0~10000 */
  if (duty > 10000U)
  {
    duty = 10000U;
  }
  /* JSON 未下发过 duty 时使用默认 50% */
  if (duty == 0U)
  {
    duty = MOTOR_DEFAULT_DUTY;
  }

  if (mode == 0U)
  {
    /* motor=0：电机停止 */
    Motor_Stop();
    g_device_state.motor      = 0U;
    g_device_state.motor_duty = 0U;
    return;
  }

  /* motor=1/2：电机启动，不区分转向，统一按正转 */
  Motor_SetState(MOTOR_FORWARD);
  percent = (uint8_t)(duty / 100U);           /* 0~10000 -> 0~100% */
  Motor_SetSpeed(percent);

  g_device_state.motor      = mode;
  g_device_state.motor_duty = duty;
}

void Buzzer_Set(uint8_t en)
{
  if (en != 0U)
  {
    Buzzer_On();
    g_device_state.buzzer = 1U;
  }
  else
  {
    Buzzer_Off();
    g_device_state.buzzer = 0U;
  }
}

void Servo_Set_Angle(uint16_t angle)
{
  /* 容错：servo_angle 限制 0~180 */
  if (angle > 180U)
  {
    angle = 180U;
  }
  Servo_SetAngle((uint8_t)angle);
  g_device_state.servo_angle = (uint8_t)angle;
}
