/**
  ******************************************************************************
  * @file    mqtt_port.c
  * @brief   MQTT 移植层实现 (paho.mqtt.embedded-c + ESP-01S AT 透传)
  * @note    STM32F103C8T6, HAL + FreeRTOS
  *
  * 链路：USART2(PA2/PA3) --AT--> ESP-01S --WiFi/TCP--> EMQX Broker
  * mqtt_send(): 透传模式下把 MQTT 报文原样写到 USART2
  * mqtt_recv(): 从 ESP 环形缓冲读取服务器返回的 MQTT 报文 (信号量等待)
  *
  * ===== MQTT 架构说明 (参考 B站 MQTT 全套教学视频) =====
  * - Broker(代理/服务器)：消息中转站，本工程连接 PC 上本地 EMQX (1883)。
  * - Publisher(发布者)：本设备上行，向 esp01s/device/telemetry 发布遥测 JSON。
  * - Subscriber(订阅者)：本设备下行，订阅 esp01s/device/cmd 接收控制指令；
  *   PC 端 MQTTX 同时订阅 telemetry 查看上报、向 cmd 发布指令完成双向通信。
  * - 报文类型：CONNECT/CONNACK(建连)、PUBLISH/PUBACK(发布)、
  *   SUBSCRIBE/SUBACK(订阅)、PINGREQ/PINGRESP(保活)、DISCONNECT(断开)。
  * - QoS 等级：0=最多一次，1=至少一次，2=恰好一次；本工程上下行均用 QoS0。
  * - 保活机制：keepAlive=60s，paho 在 MQTTYield 中自动发 PINGREQ，
  *   Broker 在 1.5 倍 keepalive 内收不到任何报文即判定离线断开。
  * - 主题通配符：'+' 匹配单层 (如 esp01s/+/cmd)，'#' 匹配多层后缀 (如 esp01s/#)；
  *   本工程使用精确主题，避免通配符造成越权或误订阅。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "mqtt_port.h"
#include "esp_at.h"
#include "json_util.h"
#include "device_ctrl.h"
#include "cJSON.h"

#include "cmsis_os.h"
#include "control.h"
#include "motor.h"
#include "servo.h"
#include "buzzer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 调试开关：1=打印 paho 从环形缓冲读到的字节数 (联调确认报文进入协议栈后置 0) */
#define MQTT_DEBUG_RX   0U

/* 串口调试打印统一加互斥锁 (printf_mutex 定义于 main.c)，
   避免多任务 printf 抢线导致同一串口内数据混行 */
extern osMutexId_t printf_mutex;
#define MQTT_PRINTF(...)  do { if (printf_mutex != NULL) { osMutexAcquire(printf_mutex, osWaitForever); } printf(__VA_ARGS__); if (printf_mutex != NULL) { osMutexRelease(printf_mutex); } } while (0)

/* 私有变量 -------------------------------------------------------------------*/
static unsigned char mqtt_sendbuf[256U]; /* paho 发送缓冲 */
static unsigned char mqtt_readbuf[256U];  /* paho 接收缓冲 */
static Network      mqtt_network;
static MQTTClient   mqtt_client;
static Mqtt_State_t mqtt_state = MQTT_STATE_IDLE;  /* 连接状态机 */
static char         mqtt_client_id[32];             /* 唯一随机 clientId */

/* 私有函数声明 ---------------------------------------------------------------*/
static int  mqtt_recv(Network *n, unsigned char *buffer, int len, int timeout_ms);
static int  mqtt_send(Network *n, unsigned char *buffer, int len, int timeout_ms);
static void Mqtt_ParseCmd(const char *cmd);

/* ==================== paho Timer 平台函数 (FreeRTOS tick) ==================== */
void TimerInit(Timer *timer)
{
  timer->xEndTick = 0U;
}

void TimerCountdownMS(Timer *timer, unsigned int timeout_ms)
{
  timer->xEndTick = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
}

void TimerCountdown(Timer *timer, unsigned int timeout)
{
  /* timeout 单位：秒 */
  TimerCountdownMS(timer, timeout * 1000U);
}

char TimerIsExpired(Timer *timer)
{
  /* 带符号差值 <= 0 视为到期 (对 32 位 tick 回绕安全) */
  return ((int32_t)(timer->xEndTick - xTaskGetTickCount()) <= 0) ? (char)1 : (char)0;
}

int TimerLeftMS(Timer *timer)
{
  int32_t diff = (int32_t)(timer->xEndTick - xTaskGetTickCount());

  if (diff <= 0)
  {
    return 0;
  }
  return (int)((uint32_t)diff * portTICK_PERIOD_MS);
}

/* ==================== paho Network 收发适配 (AT 透传) ==================== */
/**
  * @brief  mqtt_recv：从 ESP 环形缓冲读取 MQTT 报文。
  *         先读已有数据；无数据时用信号量阻塞等待 (超时由 timeout_ms 控制)。
  *         同时扫描原始流中的 "CLOSED"/"WIFI DISCONNECT" 更新链路状态。
  */
static int mqtt_recv(Network *n, unsigned char *buffer, int len, int timeout_ms)
{
  TickType_t start = xTaskGetTickCount();
  uint32_t   timeout_ticks;
  int        total = 0;

  (void)n;
  if ((buffer == NULL) || (len <= 0))
  {
    return 0;
  }
  timeout_ticks = pdMS_TO_TICKS(timeout_ms);

  while (total < len)
  {
    uint16_t got = esp_rx_read(&buffer[total], (uint16_t)(len - total));

    if (got > 0U)
    {
      total += (int)got;
#if (MQTT_DEBUG_RX == 1U)
      MQTT_PRINTF("[MQTT] net recv %u bytes (want %d)\r\n", (unsigned int)got, len);
#endif
      /* 原始流中混入链路状态字符串 -> 连接已断，立即返回 */
      if (esp_rx_scan_link(&buffer[total - (int)got], got) != 0)
      {
        break;
      }
      continue;
    }

    /* 无数据可读：剩余时间为 0 则立即返回 */
    if (timeout_ms <= 0)
    {
      break;
    }
    {
      uint32_t elapsed = (uint32_t)((uint32_t)xTaskGetTickCount() - (uint32_t)start);
      uint32_t remain_ms;

      if (elapsed >= timeout_ticks)
      {
        break;
      }
      remain_ms = (uint32_t)(timeout_ticks - elapsed) * portTICK_PERIOD_MS;
      if (esp_rx_wait(remain_ms) != ESP_OK)
      {
        break;   /* 超时 */
      }
    }
  }
  return total;
}

/**
  * @brief  mqtt_send：透传模式下把 MQTT 报文原样发送到 USART2 (ESP 转发到 TCP)。
  */
static int mqtt_send(Network *n, unsigned char *buffer, int len, int timeout_ms)
{
  int rc;

  (void)n;
  (void)timeout_ms;

  rc = esp_tcp_send(buffer, (uint16_t)len);
  if (rc < 0)
  {
    return -1;
  }
  return rc;
}

/* ==================== MQTT 应用逻辑 ==================== */
void mqtt_init(void)
{
  /* 0. 生成唯一随机 clientId：取 STM32F103 芯片 96 位唯一 ID (0x1FFFF7E8)，
     确保多台设备 clientId 不冲突 (MQTT 协议要求同一 Broker 下 clientId 唯一) */
  {
    uint32_t uid0 = *(volatile uint32_t *)0x1FFFF7E8U;
    uint32_t uid1 = *(volatile uint32_t *)0x1FFFF7ECU;
    uint32_t uid2 = *(volatile uint32_t *)0x1FFFF7F0U;

    (void)snprintf(mqtt_client_id, sizeof(mqtt_client_id),
                   MQTT_CLIENT_ID_PREFIX "%08lX%08lX%08lX",
                   (unsigned long)uid0, (unsigned long)uid1, (unsigned long)uid2);
    MQTT_PRINTF("[MQTT] clientId=%s keepAlive=%us\r\n",
           mqtt_client_id, (unsigned int)MQTT_KEEPALIVE_S);
  }

  /* 1. ESP-01S USART2 DMA + 空闲中断 + 信号量初始化 */
  esp_at_init();

  /* 2. paho Network 适配层绑定 */
  mqtt_network.mqttread  = mqtt_recv;
  mqtt_network.mqttwrite = mqtt_send;

  /* 3. paho 客户端对象初始化 */
  MQTTClientInit(&mqtt_client, &mqtt_network, MQTT_CMD_TIMEOUT_MS,
                 mqtt_sendbuf, sizeof(mqtt_sendbuf),
                 mqtt_readbuf, sizeof(mqtt_readbuf));
}

int mqtt_connect(void)
{
  static uint32_t attempt = 0U;
  MQTTPacket_connectData opts = MQTTPacket_connectData_initializer;
  uint8_t at_try;
  int rc;

  attempt++;
  mqtt_state = MQTT_STATE_IDLE;  /* 每次建连从 IDLE 开始，逐步推进状态 */

  /* 0. AT 自检：发送 "AT" 必须返回 OK (失败重试 3 次)。
     WiFi/TCP 失败不复位模块；只有 AT 完全不响应时才复位一次，
     避免反复 AT+RST 打断重试流程。 */
  rc = ESP_ERR_TIMEOUT;
  for (at_try = 0U; at_try < 3U; at_try++)
  {
    if (esp_at_test() == ESP_OK)
    {
      rc = ESP_OK;
      break;
    }
    osDelay(1000U);
  }
  if (rc != ESP_OK)
  {
    (void)esp_reset();   /* 模块完全不响应时最后复位一次 */
    if (esp_at_test() != ESP_OK)
    {
      return -1;
    }
  }

  /* 1. 凭据检查：仍是占位符时直接报错，避免 AT+CWJAP 反复 ERROR */
  if ((strcmp(MQTT_WIFI_SSID, "YourSSID") == 0) ||
      (strcmp(MQTT_WIFI_PWD, "YourPassword") == 0))
  {
    MQTT_PRINTF("[AT] ERROR: set real WiFi SSID/PWD in mqtt_port.h first\r\n");
    return -2;
  }

  /* 调试辅助：首次连接前扫描热点，确认目标 SSID 可见及信号强度 (RSSI) */
  if (attempt == 1U)
  {
    (void)esp_scan_ap();
  }

  /* 2. WiFi 连接：内部含 CWMODE=1 + CWJAP 等待 "WIFI GOT IP" + 重试 5 次，
     失败不复位模块；未连接或每 3 次连接尝试强制重连一次 */
  if ((esp_wifi_is_connected() == 0) || ((attempt % 3U) == 0U))
  {
    rc = esp_wifi_connect(MQTT_WIFI_SSID, MQTT_WIFI_PWD);
    if (rc != ESP_OK)
    {
      return -3;
    }
  }
  mqtt_state = MQTT_STATE_WIFI_READY;

  /* 3. 清理可能残留的旧 TCP，再向 Broker 建连并进入透传。
     esp_tcp_connect 内含：CIPSTART->CONNECT -> CIPMODE=1 -> CIPSEND->'>'，
     TCP 连接成功后才允许开启透传 (禁止提前 CIPMODE=1)。 */
  if (esp_tcp_is_connected() != 0)
  {
    (void)esp_tcp_close();
  }
  rc = esp_tcp_connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  if (rc != ESP_OK)
  {
    return -4;
  }
  mqtt_state = MQTT_STATE_TCP_READY;
  /* ESP-01S 收到 '>' 后稍作稳定再发 MQTT 报文，避免首包丢失 */
  osDelay(100U);

  /* 4. MQTT CONNECT：paho 发送 CONNECT 报文并等待 CONNACK */
  opts.clientID.cstring    = mqtt_client_id;   /* 唯一随机 clientId */
  opts.keepAliveInterval   = MQTT_KEEPALIVE_S;
  opts.cleansession        = 1;   /* clean-session=1：旧会话/旧订阅全部清空 */
  /* 匿名登录：不设置 username/password */
  rc = MQTTConnect(&mqtt_client, &opts);
  if (rc != MQTT_SUCCESS)
  {
    mqtt_state = MQTT_STATE_IDLE;
    (void)esp_exit_transparent();
    (void)esp_tcp_close();
    MQTT_PRINTF("[MQTT] CONNECT failed rc=%d (%s)\r\n", rc, mqtt_err_str(rc));
    return rc;
  }
  mqtt_state = MQTT_STATE_MQTT_READY;
  MQTT_PRINTF("[MQTT] state -> MQTT_READY (CONNACK ok)\r\n");
  return 0;
}

void mqtt_disconnect(void)
{
  mqtt_state = MQTT_STATE_IDLE;  /* 断开后状态回 IDLE，禁止发布/订阅 */

  /* 发送 MQTT DISCONNECT 报文 (透传模式)，失败也继续清理链路 */
  if (MQTTIsConnected(&mqtt_client))
  {
    (void)MQTTDisconnect(&mqtt_client);
  }
  (void)esp_exit_transparent();
  (void)esp_tcp_close();
}

int mqtt_is_connected(void)
{
  return MQTTIsConnected(&mqtt_client);
}

int mqtt_yield(int timeout_ms)
{
  if (!MQTTIsConnected(&mqtt_client))
  {
    return -1;
  }
  /* MQTTYield 内部：readPacket 处理入站 PUBLISH/SUBACK/PINGRESP，
     同时自动发送 keepalive PINGREQ。必须周期性循环调用，否则：
     1) 收不到下行订阅消息；2) 保活超时被 Broker 踢下线。 */
  return (MQTTYield(&mqtt_client, timeout_ms) == MQTT_SUCCESS) ? 0 : -1;
}

int mqtt_publish_telemetry(float temp, uint16_t light, uint8_t alarm)
{
  char        json[64];
  MQTTMessage msg;
  int         len;
  int         rc;

  /* 状态机门控：MQTT 握手 (CONNACK) 未完成禁止发布 */
  if ((mqtt_state < MQTT_STATE_MQTT_READY) || (!MQTTIsConnected(&mqtt_client)))
  {
    return -1;
  }

  (void)alarm;   /* 新协议字段：temp/light/motor/buzzer/servo，不含 alarm */
  /* 1. 组装 JSON 字符串：{"temp":25.6,"light":1200,"motor":1,"buzzer":0,"servo":90}
     上报时同步读取全局设备状态 g_device_state */
  len = Json_FormatTelemetryState(json, sizeof(json), temp, light,
                                  g_device_state.motor,
                                  g_device_state.buzzer,
                                  g_device_state.servo_angle);
  if (len <= 0)
  {
    return -2;
  }

  /* 2. 向 esp01s/device/telemetry 发布 QoS0 消息 */
  msg.qos        = QOS0;
  msg.retained   = 0;
  msg.dup        = 0;
  msg.id         = 0;
  msg.payload    = json;
  msg.payloadlen = (size_t)len;

  rc = MQTTPublish(&mqtt_client, MQTT_TOPIC_TELEMETRY, &msg);
  if (rc != MQTT_SUCCESS)
  {
    return rc;   /* MQTT_FAILURE(-1) / MQTT_BUFFER_OVERFLOW(-2) */
  }
  MQTT_PRINTF("[MQTT] TX %s: %s\r\n", MQTT_TOPIC_TELEMETRY, json);
  return 0;
}

/* ==================== 下行指令处理 (订阅回调) ==================== */
/**
  * @brief  解析服务器下发的控制指令 (MQTTX 向 esp01s/device/cmd 发布纯文本)。
  *         支持指令：
  *           "buzzer beep"  蜂鸣器短鸣 200ms
  *           "motor fwd"    电机正转
  *           "servo 90"     舵机转到 90° (0~180)
  *         兼容扩展：buzzer on/off、motor rev/stop。
  */
static void Mqtt_ParseCmd(const char *cmd)
{
  cJSON *root = NULL;
  cJSON *item = NULL;

  /* cJSON 解析下行 JSON：{"motor":0/1/2,"buzzer":0/1,"servo_angle":0~180}
     解析失败直接丢弃本条指令，不执行任何动作 */
  root = cJSON_Parse(cmd);
  if (root == NULL)
  {
    MQTT_PRINTF("[MQTT] cmd JSON parse failed, discard\r\n");
    return;
  }

  /* motor: 0=停止, 1/2=启动(不区分转向)；duty 沿用当前值(无则默认50%) */
  item = cJSON_GetObjectItemCaseSensitive(root, "motor");
  if (cJSON_IsNumber(item))
  {
    Motor_Set((uint8_t)item->valueint, g_device_state.motor_duty);
  }

  /* buzzer: 0=关闭, 1=打开 */
  item = cJSON_GetObjectItemCaseSensitive(root, "buzzer");
  if (cJSON_IsNumber(item))
  {
    Buzzer_Set((uint8_t)item->valueint);
  }

  /* servo_angle: 0~180，越界容错 (内部截断) */
  item = cJSON_GetObjectItemCaseSensitive(root, "servo_angle");
  if (cJSON_IsNumber(item))
  {
    Servo_Set_Angle((uint16_t)item->valueint);
  }

  cJSON_Delete(root);
}

/**
  * @brief  MQTT 下行订阅消息回调。
  *         paho 在 MQTTYield -> cycle -> deliverMessage 中调用，
  *         运行在 vTask_Cloud 任务上下文 (不是中断)，可安全调用驱动 API。
  */
static void Mqtt_CmdHandler(MessageData *md)
{
  char   topic[64];
  char   payload[64];
  size_t tlen;
  size_t plen;

  /* 原始 topic：paho 反序列化 PUBLISH 时填充 lenstring (data/len) */
  tlen = (size_t)md->topicName->lenstring.len;
  if ((tlen == 0U) && (md->topicName->cstring != NULL))
  {
    tlen = strlen(md->topicName->cstring);
  }
  if (tlen >= sizeof(topic))
  {
    tlen = sizeof(topic) - 1U;
  }
  if ((tlen > 0U) && (md->topicName->lenstring.data != NULL))
  {
    memcpy(topic, md->topicName->lenstring.data, tlen);
  }
  else if ((tlen > 0U) && (md->topicName->cstring != NULL))
  {
    memcpy(topic, md->topicName->cstring, tlen);
  }
  topic[tlen] = '\0';

  /* 原始 payload */
  plen = md->message->payloadlen;
  if (plen >= sizeof(payload))
  {
    plen = sizeof(payload) - 1U;
  }
  memcpy(payload, md->message->payload, plen);
  payload[plen] = '\0';

  /* 打印收到的 topic 与 payload 原始数据及长度到调试串口 (USART1) */
  MQTT_PRINTF("[MQTT] RX topic=%s (len=%u) payload=%s (len=%u)\r\n",
         topic, (unsigned int)tlen, payload, (unsigned int)plen);

  /* 指令解析：cJSON 解析下行 JSON，控制电机/蜂鸣器/SG90舵机 */
  Mqtt_ParseCmd(payload);
}

int mqtt_subscribe_cmd_topic(void)
{
  int rc;

  /* 状态机门控：只有 MQTT 握手完成 (MQTT_READY) 才允许订阅 */
  if ((mqtt_state < MQTT_STATE_MQTT_READY) || (!MQTTIsConnected(&mqtt_client)))
  {
    return -1;
  }
  /* 发送 SUBSCRIBE 报文并等待 SUBACK；成功后 paho 自动注册消息回调。
     clean-session=1 时每次 CONNECT 都会清空旧会话与消息处理器，
     因此重连握手成功后必须立即重新调用本函数，不能依赖旧订阅。 */
  rc = MQTTSubscribe(&mqtt_client, MQTT_TOPIC_CMD, QOS0, Mqtt_CmdHandler);
  if (rc != MQTT_SUCCESS)
  {
    MQTT_PRINTF("[MQTT] subscribe %s failed rc=%d (%s)\r\n", MQTT_TOPIC_CMD, rc, mqtt_err_str(rc));
    return rc;
  }
  mqtt_state = MQTT_STATE_SUBSCRIBED;
  MQTT_PRINTF("[MQTT] subscribed %s (state=SUBSCRIBED)\r\n", MQTT_TOPIC_CMD);
  /* 调试：确认消息回调已注册 (handler[0] 非空，paho 才会把 PUBLISH 递交给回调) */
  MQTT_PRINTF("[MQTT] handler[0]=%s isconnected=%d\r\n",
         (mqtt_client.messageHandlers[0].topicFilter != NULL) ? mqtt_client.messageHandlers[0].topicFilter : "(null)",
         MQTTIsConnected(&mqtt_client));
  return 0;
}

Mqtt_State_t mqtt_get_state(void)
{
  return mqtt_state;
}

const char *mqtt_err_str(int rc)
{
  switch (rc)
  {
    case 0:  return "OK";
    case -1: return "failure: no CONNACK / transport error";
    case -2: return "buffer overflow / command rejected";
    case -3: return "WiFi credential missing or connect failed";
    case -4: return "TCP/transparent setup failed";
    case 1:  return "CONNACK: unacceptable protocol version";
    case 2:  return "CONNACK: client identifier rejected";
    case 3:  return "CONNACK: server unavailable";
    case 4:  return "CONNACK: bad username or password";
    case 5:  return "CONNACK: not authorized";
    default: return "unknown error";
  }
}
