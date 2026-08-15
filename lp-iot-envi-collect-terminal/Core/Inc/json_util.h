/**
  ******************************************************************************
  * @file    json_util.h
  * @brief   简易 JSON 封装工具 (Day17)
  ******************************************************************************
  */
#ifndef __JSON_UTIL_H
#define __JSON_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  生成遥测 JSON 字符串到 buf。
  *         输出格式：{"temp":25.6,"light":1200,"alarm":0}
  * @param  buf      输出缓冲
  * @param  buf_len  缓冲大小
  * @param  temp     温度 (℃)
  * @param  light    光照 (lx)
  * @param  alarm    告警标志 (0/1)
  * @return 成功返回字符串长度 (不含结尾 0)，失败返回负数
  */
int Json_FormatTelemetry(char *buf, uint16_t buf_len, float temp, uint16_t light, uint8_t alarm);

/**
  * @brief  生成遥测 JSON 字符串 (含设备状态)。
  *         输出格式：{"temp":25.6,"light":1200,"motor":1,"buzzer":0,"servo":90}
  * @param  motor       电机状态 0/1/2
  * @param  buzzer      蜂鸣器状态 0/1
  * @param  servo_angle 舵机角度 0~180
  * @return 成功返回字符串长度 (不含结尾 0)，失败返回负数
  */
int Json_FormatTelemetryState(char *buf, uint16_t buf_len, float temp, uint16_t light,
                              uint8_t motor, uint8_t buzzer, uint8_t servo_angle);

#ifdef __cplusplus
}
#endif

#endif /* __JSON_UTIL_H */
