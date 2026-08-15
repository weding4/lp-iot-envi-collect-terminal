/**
  ******************************************************************************
  * @file    json_util.c
  * @brief   简易 JSON 封装工具实现 (Day17)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "json_util.h"

#include <stdio.h>

int Json_FormatTelemetry(char *buf, uint16_t buf_len, float temp, uint16_t light, uint8_t alarm)
{
  int n;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return -1;
  }

  /* 输出格式：{"temp":25.6,"light":1200,"alarm":0} */
  n = snprintf(buf, buf_len, "{\"temp\":%.1f,\"light\":%u,\"alarm\":%u}",
               temp, (unsigned int)light, (unsigned int)alarm);
  if (n < 0)
  {
    return -1;
  }
  if (n >= (int)buf_len)
  {
    return -1;   /* 缓冲不足，字符串被截断 */
  }
  return n;
}

int Json_FormatTelemetryState(char *buf, uint16_t buf_len, float temp, uint16_t light,
                              uint8_t motor, uint8_t buzzer, uint8_t servo_angle)
{
  int n;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return -1;
  }

  /* 输出格式：{"temp":25.6,"light":1200,"motor":1,"buzzer":0,"servo":90} */
  n = snprintf(buf, buf_len,
               "{\"temp\":%.1f,\"light\":%u,\"motor\":%u,\"buzzer\":%u,\"servo\":%u}",
               temp, (unsigned int)light,
               (unsigned int)motor, (unsigned int)buzzer, (unsigned int)servo_angle);
  if (n < 0)
  {
    return -1;
  }
  if (n >= (int)buf_len)
  {
    return -1;   /* 缓冲不足，字符串被截断 */
  }
  return n;
}
