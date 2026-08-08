#include "control.h"
#include "servo.h"
#include "motor.h"
#include "buzzer.h"
#include "sensor.h"
#include "stdio.h"       // 解决 printf 隐式声明
#include "stm32f1xx_hal.h"  // 解决 HAL_GetTick()

#define TEMP_THRESHOLD      35.0f   // 温度阈值 (℃)
#define LIGHT_THRESHOLD     80.0f   // 光照阈值 (百分比，模拟lux标定)

static bool alarm_active = false;

void Control_Init(void)
{
    Servo_Init();
    Motor_Init();
    Buzzer_Init();
}

void Control_Execute(float temperature, float light_percent)
{
    bool condition = (temperature > TEMP_THRESHOLD) || (light_percent > LIGHT_THRESHOLD);

    if (condition && !alarm_active) {
        // 触发报警
        alarm_active = true;
		printf("[%lu] Alarm ON  (T:%.1f L:%.0f)\r\n", HAL_GetTick(), temperature, light_percent);
        Buzzer_On();
        Motor_SetState(MOTOR_FORWARD);   // 模拟通风
        Servo_SetAngle(0);               // 舵机打到一边（模拟遮阳帘关闭）
    } else if (!condition && alarm_active) {
        // 解除报警
        alarm_active = false;
		printf("[%lu] Alarm OFF (T:%.1f L:%.0f)\r\n", HAL_GetTick(), temperature, light_percent);
		Sensor_ResetFilters();  // 清空滤波历史，立刻响应新值
        Buzzer_Off();
        Motor_SetState(MOTOR_STOP);
        Servo_SetAngle(90);              // 回到中位
    }
    // 若状态无变化则保持
}
