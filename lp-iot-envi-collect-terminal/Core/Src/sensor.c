#include "sensor.h"
#include "filter.h"
#include "adc.h"
#include <math.h>

// 硬件参数
#define VCC          3.3f
#define ADC_MAX      4095.0f
#define R_FIXED      10000.0f
#define NTC_R25      10000.0f
#define NTC_B        3950.0f
#define T0           298.15f

// 缓冲区放模块内部，DMA要求4字节对齐
static volatile uint16_t adc_dma_buf[2] __attribute__((aligned(4)));
static SlidingFilter temp_filter;
static SlidingFilter light_filter;
static uint8_t init_flag = 0;

void Sensor_Init(void)
{
    if(init_flag != 0)
    {
        return; // 防止重复初始化、重复启动DMA
    }

    SlidingFilter_Init(&temp_filter);
    SlidingFilter_Init(&light_filter);

    // 启动ADC DMA循环采集，模块内部完成
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buf, 2);

    init_flag = 1;
}

static float adc_to_temperature(uint16_t adc, SensorStatus *status)
{
    if (adc <= 5)
    {
        *status = SENSOR_ERR_ADC_UNDERVOLTAGE;
        return -274.0f;
    }
    if (adc >= 4090)
    {
        *status = SENSOR_ERR_ADC_OVERVOLTAGE;
        return -274.0f;
    }

    float v = (float)adc * VCC / ADC_MAX;
    float r_ntc = R_FIXED * v / (VCC - v);

    if (r_ntc < 100.0f || r_ntc > 1000000.0f)
    {
        *status = SENSOR_ERR_RANGE;
        return -274.0f;
    }

    float temp_k = 1.0f / (1.0f / T0 + logf(r_ntc / NTC_R25) / NTC_B);
    float temp_c = temp_k - 273.15f;

    if (temp_c < -30.0f || temp_c > 90.0f)
    {
        *status = SENSOR_ERR_RANGE;
        return -274.0f;
    }

    *status = SENSOR_OK;
    return temp_c;
}

float Sensor_ReadTemperature(SensorStatus *status)
{
    uint16_t raw = adc_dma_buf[0];
    SlidingFilter_Update(&temp_filter, raw);
    uint16_t f = SlidingFilter_GetAvg(&temp_filter);
    return adc_to_temperature(f, status);
}

float Sensor_ReadLightPercent(SensorStatus *status)
{
    uint16_t raw = adc_dma_buf[1];
    SlidingFilter_Update(&light_filter, raw);
    uint16_t f = SlidingFilter_GetAvg(&light_filter);

    if (f <= 5)
    {
        *status = SENSOR_ERR_ADC_UNDERVOLTAGE;
        return 0.0f;
    }
    if (f >= 4090)
    {
        *status = SENSOR_ERR_ADC_OVERVOLTAGE;
        return 100.0f;
    }
    *status = SENSOR_OK;
    return (float)f * 100.0f / ADC_MAX;
}


//uint16_t Sensor_GetRawTempADC(void)  { return adc_dma_buf[0]; }
//uint16_t Sensor_GetRawLightADC(void) { return adc_dma_buf[1]; }
