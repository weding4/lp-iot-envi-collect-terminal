/**
  ******************************************************************************
  * @file    device_ctrl.h
  * @brief   设备控制适配层 (电机/蜂鸣器/SG90舵机) + 全局设备状态结构体
  * @note    新增业务层，不改动已有驱动 motor.c / buzzer.c / servo.c
  *          仅对现有驱动做轻量封装，供 MQTT 下行 JSON 指令与遥测上报使用
  ******************************************************************************
  */
#ifndef __DEVICE_CTRL_H
#define __DEVICE_CTRL_H

#include <stdint.h>

/* 全局设备状态：MQTT 下行指令更新，遥测上报同步读取 */
typedef struct
{
  uint8_t  motor;        /* 0=停止, 1/2=运行(不区分转向) */
  uint16_t motor_duty;   /* 电机 PWM 占空比 0~10000 (50% 即 5000) */
  uint8_t  buzzer;       /* 0=关, 1=开 */
  uint8_t  servo_angle;  /* SG90 舵机角度 0~180° */
} Device_State_t;

extern Device_State_t g_device_state;

/**
  * @brief  电机控制：mode=0 停止；mode=1/2 启动(不区分转向)。
  * @param  mode  0=停止, 1/2=启动
  * @param  duty  PWM 占空比 0~10000，越界自动截断
  */
void Motor_Set(uint8_t mode, uint16_t duty);

/**
  * @brief  蜂鸣器控制。
  * @param  en  0=关闭, 非0=打开
  */
void Buzzer_Set(uint8_t en);

/**
  * @brief  SG90 舵机角度控制。
  * @param  angle  0~180°，越界自动容错到 [0,180]
  */
void Servo_Set_Angle(uint16_t angle);

#endif /* __DEVICE_CTRL_H */
