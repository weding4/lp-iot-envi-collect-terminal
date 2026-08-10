#include "control.h"
#include "servo.h"
#include "motor.h"
#include "buzzer.h"
#include "sensor.h"
#include <stdio.h>
#include "stm32f1xx_hal.h"

volatile uint8_t motor_duty = 0;
static bool alarm_active = false;
static uint8_t over_count = 0;
static bool manual_alarm_mode = false;

bool Control_IsManualAlarmActive(void)
{
    return manual_alarm_mode;
}

/**
  * @brief  Initialize the actuator chain used by the control layer.
  * @retval None
  */
void Control_Init(void)
{
    Servo_Init();
    Motor_Init();
    Buzzer_Init();
    Motor_SetSpeed(0);
}

/**
  * @brief  Let an external key press pull the actuator alarm latch.
  * @param enable True to force an alarm-active state, false to release it.
  * @retval None
  */
void Control_ManualOverride(bool enable)
{
    if (enable && !manual_alarm_mode) {
        manual_alarm_mode = true;
        alarm_active = true;
        printf("[KEY] Manual alarm enabled -> buzzer/motor/servo alarm action\r\n");
        Buzzer_On();
        Motor_SetState(MOTOR_FORWARD);
        Servo_SetAngle(0);
    } else if (!enable && manual_alarm_mode) {
        manual_alarm_mode = false;
        alarm_active = false;
        over_count = 0;
        printf("[KEY] Manual alarm released -> system recovered\r\n");
        Buzzer_Off();
        Motor_SetState(MOTOR_STOP);
        Servo_SetAngle(90);
        Sensor_ResetFilters();
    }
}

/**
  * @brief  Execute threshold-based control decision on one sensor snapshot.
  * @param temperature Current temperature in degrees Celsius.
  * @param light_percent Current light channel in percent.
  * @retval None
  */
void Control_Execute(float temperature, float light_percent)
{
    bool condition = (temperature > TEMP_THRESHOLD) || (light_percent > LIGHT_THRESHOLD);

    if (condition) {
        if (motor_duty == 0U) {
            motor_duty = 25U;
            Motor_SetSpeed(motor_duty);
            printf("[AUTO] Threshold reached -> motor started at 25%%\r\n");
        }
        /* If the user already stepped motor_duty via the button, keep it. */
    }
    else {
        if (motor_duty != 0U) {
            motor_duty = 0U;
            Motor_SetSpeed(0);
            printf("[AUTO] Conditions normal -> motor stopped\r\n");
        }
    }

    if (condition) {
        if (over_count < 2U) {
            over_count++;
        }
        if (over_count >= 2U && !alarm_active) {
            alarm_active = true;
            printf("[%lu] Alarm ON  (T:%.1f L:%.0f)\r\n", HAL_GetTick(), temperature, light_percent);
            Buzzer_On();
            Servo_SetAngle(0);
        }
    } else {
        over_count = 0U;
        if (alarm_active) {
            alarm_active = false;
            printf("[%lu] Alarm OFF (T:%.1f L:%.0f)\r\n", HAL_GetTick(), temperature, light_percent);
            Sensor_ResetFilters();
            Buzzer_Off();
            Servo_SetAngle(90);
        }
    }
}

