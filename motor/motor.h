#ifndef __MOTOR_H
#define __MOTOR_H
#include <stdint.h>

typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_REVERSE
} MotorState;

void Motor_Init(void);
void Motor_SetState(MotorState state);
#endif
