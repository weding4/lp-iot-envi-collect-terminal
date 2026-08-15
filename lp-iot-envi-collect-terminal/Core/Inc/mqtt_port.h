/**
  ******************************************************************************
  * @file    mqtt_port.h
  * @brief   MQTT 移植层 (paho.mqtt.embedded-c + ESP-01S AT 透传) 头文件
  * @note    STM32F103C8T6, HAL + FreeRTOS
  *
  * 本文件同时充当 paho 的平台头 (MQTTCLIENT_PLATFORM_HEADER)：
  * 在包含 MQTTClient.h 之前定义 Timer/Network 结构，paho 即可编译。
  * 网络完全不使用 LWIP/FreeRTOS+TCP，底层是 ESP-01S AT 透传串口。
  ******************************************************************************
  */
#ifndef __MQTT_PORT_H
#define __MQTT_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stddef.h>

/* ==================== paho 平台类型定义 ==================== */
/* Timer：基于 FreeRTOS tick 的毫秒定时器，供 paho 内部保活使用 */
typedef struct Timer
{
  TickType_t xEndTick;   /* 绝对到期 tick */
} Timer;

/* Network：paho 只通过这两个函数指针收发数据 */
typedef struct Network Network;
struct Network
{
  int (*mqttread)(Network *n, unsigned char *buffer, int len, int timeout_ms);
  int (*mqttwrite)(Network *n, unsigned char *buffer, int len, int timeout_ms);
};

/* 告诉 paho：平台头就是本文件 (include guard 保证不重复包含) */
#ifndef MQTTCLIENT_PLATFORM_HEADER
#define MQTTCLIENT_PLATFORM_HEADER "mqtt_port.h"
#endif

#include "MQTTClient.h"

/* ==================== MQTT 应用配置 (按实际环境修改) ==================== */
/* EMQX 所在 PC 的局域网 IP (本机 WLAN: 192.168.192.28)，ESP-01S 需在同一网段 */
#define MQTT_BROKER_HOST        "192.168.192.28"
#define MQTT_BROKER_PORT        1883U
/* 匿名登录：不配置用户名/密码 (EMQX 需允许匿名访问) */
#define MQTT_CLIENT_ID_PREFIX   "stm32f103-"
#define MQTT_KEEPALIVE_S        40U
#define MQTT_CMD_TIMEOUT_MS     5000U
#define MQTT_PUBLISH_PERIOD_MS  2000U

/* 上行发布 / 下行订阅主题 */
#define MQTT_TOPIC_TELEMETRY    "esp01s/device/telemetry"
#define MQTT_TOPIC_CMD          "esp01s/device/cmd"

#define MQTT_WIFI_SSID          "weding"
#define MQTT_WIFI_PWD           "1234567890"

/* ==================== MQTT 应用接口 ==================== */
void mqtt_init(void);            /* ESP-01S AT 初始化 + paho 客户端对象初始化 */
int  mqtt_connect(void);         /* 完整建连链：WiFi -> TCP -> 透传 -> CONNACK */
void mqtt_disconnect(void);      /* 发送 DISCONNECT + 退出透传 + 关闭 TCP */
int  mqtt_is_connected(void);    /* 查询 MQTT 会话是否在线 */
int  mqtt_yield(int timeout_ms); /* 循环调用：保活 + 接收下行消息 (必须周期执行) */
int  mqtt_subscribe_cmd_topic(void); /* 订阅 esp01s/device/cmd 并注册消息回调 */
int  mqtt_publish_telemetry(float temp, uint16_t light, uint8_t alarm); /* 上报 QoS0 JSON */

/* ==================== MQTT 连接状态机 ==================== */
typedef enum
{
  MQTT_STATE_IDLE = 0,      /* 初始/断开，禁止任何 MQTT 操作 */
  MQTT_STATE_WIFI_READY,    /* WiFi 已连接 (WIFI GOT IP) */
  MQTT_STATE_TCP_READY,     /* TCP 透传就绪 (CONNECT + CIPMODE + CIPSEND) */
  MQTT_STATE_MQTT_READY,    /* MQTT CONNACK 握手完成 (才允许订阅/发布) */
  MQTT_STATE_SUBSCRIBED     /* 已订阅 /device/cmd，进入正常双向通信 */
} Mqtt_State_t;

Mqtt_State_t mqtt_get_state(void);
const char  *mqtt_err_str(int rc);

/* paho 需要的 Timer 平台函数 (定义在 mqtt_port.c) */
void TimerInit(Timer *timer);
char TimerIsExpired(Timer *timer);
void TimerCountdownMS(Timer *timer, unsigned int timeout_ms);
void TimerCountdown(Timer *timer, unsigned int timeout);
int  TimerLeftMS(Timer *timer);

#ifdef __cplusplus
}
#endif

#endif /* __MQTT_PORT_H */
