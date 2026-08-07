#include "buzzer.h"
#include "gpio.h"
#include "stm32f1xx_hal.h"
//#include "cmsis_os.h"  // 如果用了 FreeRTOS 就用 osDelay，裸机用 HAL_Delay

void Buzzer_Init(void) { Buzzer_Off(); }
void Buzzer_On(void)  { HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET); }
void Buzzer_Off(void) { HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET); }

void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On();
    HAL_Delay(ms);
    Buzzer_Off();
}
