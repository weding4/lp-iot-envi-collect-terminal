#ifndef __SENSOR_H
#define __SENSOR_H

#include <stdint.h>

typedef enum {
    SENSOR_OK = 0,
    SENSOR_ERR_ADC_OVERVOLTAGE,
    SENSOR_ERR_ADC_UNDERVOLTAGE,
    SENSOR_ERR_RANGE,
    SENSOR_ERR_CIRCUIT
} SensorStatus;

void Sensor_Init(void);
float Sensor_ReadTemperature(SensorStatus *status);
float Sensor_ReadLightPercent(SensorStatus *status);
uint16_t Sensor_GetRawTempADC(void);
uint16_t Sensor_GetRawLightADC(void);

#endif
