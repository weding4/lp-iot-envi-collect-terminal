#ifndef __BUZZER_H
#define __BUZZER_H
#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Beep(uint16_t ms);  // ¶ÌÃù
#endif
