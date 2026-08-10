#ifndef __CONTROL_H
#define __CONTROL_H
#include <stdbool.h>
#include <stdint.h>

#define TEMP_THRESHOLD      35.0f
#define LIGHT_THRESHOLD     80.0f

extern volatile uint8_t motor_duty;

void Control_Init(void);
void Control_Execute(float temperature, float light_percent);
void Control_ManualOverride(bool enable);
bool Control_IsManualAlarmActive(void);
#endif
