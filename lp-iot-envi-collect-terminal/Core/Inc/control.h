#ifndef __CONTROL_H
#define __CONTROL_H
#include <stdbool.h>

void Control_Init(void);
void Control_Execute(float temperature, float light_percent);
#endif
