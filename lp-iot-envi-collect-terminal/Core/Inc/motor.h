#ifndef __MOTOR_H
#define __MOTOR_H
#include <stdint.h>

#define MOTOR_PWM_PERIOD 20000u

typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_REVERSE
} MotorState;

void Motor_Init(void);
void Motor_SetState(MotorState state);
void Motor_SetSpeed(uint8_t duty_percent);
void Motor_Stop(void);
#endif
