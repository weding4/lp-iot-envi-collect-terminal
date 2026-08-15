/**
  ******************************************************************************
  * @file    esp_at.c
  * @brief   ESP-01S AT 指令封装实现 (USART2 + DMA + IDLE + FreeRTOS 信号量)
  * @note    STM32F103C8T6, HAL + FreeRTOS
  *
  * 接收架构：
  *   USART2 IDLE 空闲中断 (HAL_UARTEx_ReceiveToIdle_DMA + RxEventCallback)
  *   -> ISR 内仅把 DMA 收到的字节拷贝进软件环形缓冲，并 Give 一个计数信号量
  *   -> FreeRTOS 任务 (vTask_Cloud) 通过信号量等待 + 行解析处理 AT 应答
  *   全程无 HAL_Delay / 裸机轮询延时。
  *
  * 注：CubeF1 HAL 没有 HAL_UART_IDLE_CB_ID 枚举 (那是 F4/L4 系新 HAL)，
  *     空闲中断等价接口是 HAL_UARTEx_ReceiveToIdle_DMA() +
  *     HAL_UARTEx_RxEventCallback()，语义与"IDLE 回调"完全一致。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "esp_at.h"
#include "usart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"

/* 串口调试打印统一加互斥锁 (printf_mutex 定义于 main.c)，
   避免多任务 printf 抢线导致同一串口内数据混行 */
extern osMutexId_t printf_mutex;
#define AT_PRINTF(...)  do { if (printf_mutex != NULL) { osMutexAcquire(printf_mutex, osWaitForever); } printf(__VA_ARGS__); if (printf_mutex != NULL) { osMutexRelease(printf_mutex); } } while (0)

/* 内部配置 -------------------------------------------------------------------*/
#define ESP_CMD_BUF_SIZE   160U   /* AT 命令拼装缓冲 */
#define ESP_RESP_BUF_SIZE  512U   /* AT 应答原始捕获缓冲 (esp_get_ip 使用) */
#define ESP_LINE_BUF_SIZE  128U   /* 行匹配缓冲 */
#define ESP_TX_TIMEOUT     200U   /* USART2 发送超时 (ms) */
#define AT_DEBUG_LOG       0U     /* 1=打印每条 AT 收发字符串 (联调用)，0=关闭减少干扰 */

/* 内部应答状态 ---------------------------------------------------------------*/
typedef enum
{
  ESP_RESP_NONE   = 0,  /* 尚无决定性应答 */
  ESP_RESP_OK,          /* 命中 ok_str */
  ESP_RESP_ERROR,       /* 命中 err_str / FAIL / CONNECT FAIL */
  ESP_RESP_CLOSED       /* CLOSED / WIFI DISCONNECT */
} Esp_Resp_t;

/* 软件环形接收缓冲区 (写者=ISR, 读者=FreeRTOS任务) -----------------------------*/
typedef struct
{
  uint8_t           buf[ESP_RX_BUF_SIZE];
  volatile uint16_t head;   /* 写索引，ISR 更新 */
  volatile uint16_t tail;   /* 读索引，任务更新 */
  volatile uint16_t count;  /* 有效字节数 */
} Esp_RingBuf_t;

/* 私有变量 -------------------------------------------------------------------*/
static Esp_RingBuf_t      esp_rx_ring;
static uint8_t            esp_dma_buf[ESP_RX_BUF_SIZE];   /* DMA 接收缓冲 */
static uint8_t            esp_resp_buf[ESP_RESP_BUF_SIZE];/* 原始应答捕获 */
static uint8_t            esp_line_buf[ESP_LINE_BUF_SIZE];/* 行匹配缓冲 */
static uint16_t           esp_line_len;                   /* 当前行长度 */
static volatile uint16_t  esp_rx_overflow;                /* 环形溢出计数 */
static SemaphoreHandle_t  esp_rx_sem;                     /* 接收信号量 (ISR Give) */
static volatile uint8_t   esp_wifi_connected;
static volatile uint8_t   esp_tcp_connected;

/* 私有函数声明 ---------------------------------------------------------------*/
static void       Esp_Uart_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
static void       Esp_Uart_ErrorCallback(UART_HandleTypeDef *huart);
static void       Esp_RingBuf_Init(Esp_RingBuf_t *rb);
static uint16_t   Esp_RingBuf_Write(Esp_RingBuf_t *rb, const uint8_t *data, uint16_t len);
static uint16_t   Esp_RingBuf_Read(Esp_RingBuf_t *rb, uint8_t *data, uint16_t len);
static void       Esp_RingBuf_Clear(Esp_RingBuf_t *rb);
static Esp_Resp_t esp_classify_line(const char *line, const char *ok_str, const char *err_str);
static Esp_Resp_t esp_rx_consume(const char *ok_str, const char *err_str,
                                 uint8_t *resp, uint16_t resp_len,
                                 uint16_t *resp_idx, uint16_t *received);
static int        esp_send_cmd(const char *cmd);
static int        esp_wait_resp(const char *ok_str, const char *err_str, uint32_t timeout_ms,
                                uint8_t *resp, uint16_t resp_len);
static int        esp_wifi_check_ip(void);

/* ==================== 环形缓冲 (ISR 写 / 任务读) ==================== */
static void Esp_RingBuf_Init(Esp_RingBuf_t *rb)
{
  rb->head  = 0U;
  rb->tail  = 0U;
  rb->count = 0U;
}

static uint16_t Esp_RingBuf_Write(Esp_RingBuf_t *rb, const uint8_t *data, uint16_t len)
{
  uint16_t i;

  for (i = 0U; i < len; i++)
  {
    if (rb->count == ESP_RX_BUF_SIZE)
    {
      /* 环形满：丢弃最旧字节，保留最新数据，并累计溢出次数 */
      rb->tail = (uint16_t)((rb->tail + 1U) % ESP_RX_BUF_SIZE);
      rb->count--;
      esp_rx_overflow++;
    }
    rb->buf[rb->head] = data[i];
    rb->head = (uint16_t)((rb->head + 1U) % ESP_RX_BUF_SIZE);
    rb->count++;
  }
  return len;
}

static uint16_t Esp_RingBuf_Read(Esp_RingBuf_t *rb, uint8_t *data, uint16_t len)
{
  uint16_t i;
  uint32_t primask;

  /* 读操作关中断，避免与 ISR 写操作竞争 */
  primask = __get_PRIMASK();
  __disable_irq();

  for (i = 0U; i < len; i++)
  {
    if (rb->count == 0U)
    {
      break;
    }
    data[i] = rb->buf[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1U) % ESP_RX_BUF_SIZE);
    rb->count--;
  }

  __set_PRIMASK(primask);
  return i;
}

static void Esp_RingBuf_Clear(Esp_RingBuf_t *rb)
{
  uint32_t primask;

  primask = __get_PRIMASK();
  __disable_irq();
  Esp_RingBuf_Init(rb);
  __set_PRIMASK(primask);
}

/* ==================== USART2 中断回调 (ISR, 只做轻量搬运) ==================== */
/**
  * @brief  USART2 接收事件回调：DMA 满 或 总线空闲 (IDLE) 时由 HAL 调用。
  *         只做两件事：把 DMA 缓冲拷入环形缓冲；重新装载 DMA 接收。
  *         不在此处解析任何 AT 内容，解析全部交给 FreeRTOS 任务。
  */
static void Esp_Uart_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (huart == &huart2)
  {
    if (Size > 0U)
    {
      Esp_RingBuf_Write(&esp_rx_ring, esp_dma_buf, Size);
    }

    /* 每次空闲事件后重新启动 DMA 接收 (先停后启) */
    (void)HAL_UART_DMAStop(&huart2);
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, esp_dma_buf, ESP_RX_BUF_SIZE);

    /* 通知等待中的任务有新数据 */
    xSemaphoreGiveFromISR(esp_rx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/**
  * @brief  USART2 错误回调 (溢出/帧错误等)：重启 DMA，避免一次误码后接收永久停摆。
  */
static void Esp_Uart_ErrorCallback(UART_HandleTypeDef *huart)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (huart == &huart2)
  {
    (void)HAL_UART_DMAStop(&huart2);
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, esp_dma_buf, ESP_RX_BUF_SIZE);
    xSemaphoreGiveFromISR(esp_rx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/* ==================== AT 应答行解析 ==================== */
/**
  * @brief  解析一行 ESP-01S 应答。
  *         除了匹配调用者指定的 ok_str/err_str，还维护 WiFi/TCP 链路状态：
  *         "WIFI GOT IP" -> WiFi 已连接；"WIFI DISCONNECT"/"CLOSED" -> 链路断开。
  */
static Esp_Resp_t esp_classify_line(const char *line, const char *ok_str, const char *err_str)
{
  char   buf[ESP_LINE_BUF_SIZE];
  size_t n;

  n = strlen(line);
  if (n == 0U)
  {
    return ESP_RESP_NONE;
  }
  if (n >= sizeof(buf))
  {
    n = sizeof(buf) - 1U;
  }
  memcpy(buf, line, n);
  buf[n] = '\0';
  /* 去掉行尾 \r\n */
  while ((n > 0U) && ((buf[n - 1U] == '\r') || (buf[n - 1U] == '\n')))
  {
    buf[--n] = '\0';
  }

  /* 所有 AT 指令返回 ERROR 都视为失败 (ESP-01S 通用错误应答，立即返回不用等超时) */
  if (strcmp(buf, "ERROR") == 0)
  {
    return ESP_RESP_ERROR;
  }

  /* --- 链路事件解析 (与本次等待的目标无关，始终维护状态) --- */
  if (strstr(buf, "WIFI DISCONNECT") != NULL)
  {
    esp_wifi_connected = 0U;
    return ESP_RESP_CLOSED;
  }
  if (strstr(buf, "WIFI GOT IP") != NULL)
  {
    esp_wifi_connected = 1U;
    /* 不提前返回：让下方 ok_str 匹配有机会命中 "WIFI GOT IP" 成功标记 */
  }
  if (strstr(buf, "CLOSED") != NULL)
  {
    esp_tcp_connected = 0U;
    return ESP_RESP_CLOSED;
  }
  if (strcmp(buf, "CONNECT") == 0)
  {
    esp_tcp_connected = 1U;
  }
  else if ((strcmp(buf, "CONNECT FAIL") == 0) || (strcmp(buf, "DNS FAIL") == 0))
  {
    esp_tcp_connected = 0U;
    return ESP_RESP_ERROR;
  }

  /* --- 调用者指定的匹配字符串：err 优先，避免 "CONNECT FAIL" 被当成 OK --- */
  if ((err_str != NULL) && (strstr(buf, err_str) != NULL))
  {
    return ESP_RESP_ERROR;
  }
  if ((ok_str != NULL) && (strstr(buf, ok_str) != NULL))
  {
    return ESP_RESP_OK;
  }
  return ESP_RESP_NONE;
}

/**
  * @brief  把环形缓冲中的字节按行喂给解析器；命中决定性应答立即返回。
  */
static Esp_Resp_t esp_rx_consume(const char *ok_str, const char *err_str,
                                 uint8_t *resp, uint16_t resp_len,
                                 uint16_t *resp_idx, uint16_t *received)
{
  uint8_t   byte;
  Esp_Resp_t r;

  while (Esp_RingBuf_Read(&esp_rx_ring, &byte, 1U) == 1U)
  {
    (*received)++;
    /* 可选：把原始应答完整捕获到 resp 缓冲 (esp_get_ip 使用) */
    if ((resp != NULL) && (resp_len > 1U) && (*resp_idx < (uint16_t)(resp_len - 1U)))
    {
      resp[(*resp_idx)++] = byte;
      resp[*resp_idx] = '\0';
    }

    /* 行组装 */
    if (esp_line_len < (uint16_t)(ESP_LINE_BUF_SIZE - 1U))
    {
      esp_line_buf[esp_line_len++] = byte;
    }
    if (byte == '\n')
    {
      esp_line_buf[esp_line_len] = '\0';
#if (AT_DEBUG_LOG == 1U)
      AT_PRINTF("[AT] RX: %s", esp_line_buf);
#endif
      r = esp_classify_line((const char *)esp_line_buf, ok_str, err_str);
      esp_line_len = 0U;
      if (r != ESP_RESP_NONE)
      {
        return r;
      }
    }
  }
  return ESP_RESP_NONE;
}

/**
  * @brief  发送 AT 命令 (自动追加 \r\n)。
  *         发送前清空环形缓冲，避免上一次应答残留干扰本次匹配。
  */
static int esp_send_cmd(const char *cmd)
{
  Esp_RingBuf_Clear(&esp_rx_ring);
  esp_line_len = 0U;

#if (AT_DEBUG_LOG == 1U)
  /* 打印发送的完整 AT 字符串 (\\r\\n 表示实际附加在指令末尾的 CRLF) */
  AT_PRINTF("[AT] TX: %s\\r\\n\r\n", cmd);
#endif

  if (HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)strlen(cmd), ESP_TX_TIMEOUT) != HAL_OK)
  {
    return ESP_ERR_TX;
  }
  /* AT 指令结尾必须拼接 \r\n，否则 ESP-01S 不解析命令 */
  if (HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2U, ESP_TX_TIMEOUT) != HAL_OK)
  {
    return ESP_ERR_TX;
  }
  return ESP_OK;
}

/**
  * @brief  等待 AT 应答：在 timeout_ms 内，用信号量阻塞等待新数据并逐行匹配。
  * @param  ok_str    成功特征串 (如 "OK"、"CONNECT"、">")
  * @param  err_str   失败特征串 (如 "ERROR"、"FAIL")
  * @param  resp      可选：原始应答捕获缓冲，NULL 表示不捕获
  * @return ESP_OK / ESP_ERR_RESP / ESP_ERR_TIMEOUT
  */
static int esp_wait_resp(const char *ok_str, const char *err_str, uint32_t timeout_ms,
                         uint8_t *resp, uint16_t resp_len)
{
  TickType_t  start = xTaskGetTickCount();
  TickType_t  total = pdMS_TO_TICKS(timeout_ms);
  TickType_t  elapsed;
  Esp_Resp_t  r;
  uint16_t    resp_idx = 0U;
  uint16_t    received = 0U;

  if (resp != NULL)
  {
    resp[0] = '\0';
  }
  esp_line_len = 0U;

  for (;;)
  {
    /* 1) 先消费环形缓冲中已有的字节 */
    r = esp_rx_consume(ok_str, err_str, resp, resp_len, &resp_idx, &received);
    if (r == ESP_RESP_OK)
    {
      return ESP_OK;
    }
    /* err_str 为 NULL 时忽略 CLOSED (如复位过程中的 WIFI DISCONNECT) */
    if ((r == ESP_RESP_ERROR) || ((r == ESP_RESP_CLOSED) && (err_str != NULL)))
    {
      return ESP_ERR_RESP;
    }

    /* 2) 超时判断 */
    elapsed = (TickType_t)((uint32_t)xTaskGetTickCount() - (uint32_t)start);
    if (elapsed >= total)
    {
      break;
    }

    /* 3) 信号量等待新数据 (ISR 每收到一帧 Give 一次)，剩余时间作为超时 */
    if (xSemaphoreTake(esp_rx_sem, total - elapsed) != pdTRUE)
    {
      break;
    }
  }

  /* 超时：最后检查一次未结束的半行 */
  if (esp_line_len > 0U)
  {
    esp_line_buf[esp_line_len] = '\0';
    r = esp_classify_line((const char *)esp_line_buf, ok_str, err_str);
    if (r == ESP_RESP_OK)
    {
      return ESP_OK;
    }
    if ((r == ESP_RESP_ERROR) || ((r == ESP_RESP_CLOSED) && (err_str != NULL)))
    {
      return ESP_ERR_RESP;
    }
  }

  (void)received;
  return ESP_ERR_TIMEOUT;
}

/* ==================== 对外 API ==================== */
void esp_at_init(void)
{
  Esp_RingBuf_Init(&esp_rx_ring);
  esp_line_len = 0U;
  esp_wifi_connected = 0U;
  esp_tcp_connected  = 0U;

  if (esp_rx_sem == NULL)
  {
    /* 计数信号量：一次空闲中断累计一个信号量 */
    esp_rx_sem = xSemaphoreCreateCounting(16U, 0U);
  }
  /* 清空积压的信号量，保证从干净状态开始 */
  while (xSemaphoreTake(esp_rx_sem, 0U) == pdTRUE)
  {
  }

  /* 注册空闲/接收事件回调与错误回调 (需 USE_HAL_UART_REGISTER_CALLBACKS=1) */
  (void)HAL_UART_RegisterRxEventCallback(&huart2, Esp_Uart_RxEventCallback);
  (void)HAL_UART_RegisterCallback(&huart2, HAL_UART_ERROR_CB_ID, Esp_Uart_ErrorCallback);

  /* 启动 DMA + 空闲中断接收 (普通模式，收到一帧或缓冲满都会回调) */
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, esp_dma_buf, ESP_RX_BUF_SIZE) != HAL_OK)
  {
    Error_Handler();
  }
}

int esp_uart_send_byte(uint8_t byte)
{
  if (HAL_UART_Transmit(&huart2, &byte, 1U, ESP_TX_TIMEOUT) != HAL_OK)
  {
    return ESP_ERR_TX;
  }
  return ESP_OK;
}

int esp_at_test(void)
{
  /* AT 链路自检：发送 "AT"，应答 "OK" 表示模块就绪、串口正常 */
  if (esp_send_cmd("AT") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  return esp_wait_resp("OK", "ERROR", ESP_AT_TIMEOUT, NULL, 0U);
}

int esp_reset(void)
{
  int rc;

  /* 若模块卡在透传模式 (AT 不响应，指令被当成 TCP 数据转发)，
     先发 "+++" 强制退出透传。命令模式下 "+++" 被忽略，无副作用。 */
  AT_PRINTF("[AT] try +++ to exit transparent mode...\r\n");
  (void)HAL_UART_Transmit(&huart2, (uint8_t *)"+++", 3U, ESP_TX_TIMEOUT);
  osDelay(1500U);

  /* AT+RST：复位模块，等待 "ready" 就绪。
     应答顺序：AT+RST 回显 -> OK -> (WIFI DISCONNECT) -> ready。
     err_str 传 NULL：复位过程中的 WIFI DISCONNECT 属正常现象，不当作失败。 */
  if (esp_send_cmd("AT+RST") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  rc = esp_wait_resp("ready", NULL, 8000U, NULL, 0U);
  if (rc == ESP_OK)
  {
    AT_PRINTF("[AT] module ready\r\n");
  }
  return rc;
}

int esp_scan_ap(void)
{
  /* 调试辅助：AT+CWLAP 扫描附近 2.4G 热点，每行经 [AT] RX 打印，
     格式：+CWLAP:(加密, "ssid", 信号强度RSSI, mac, 信道)。
     用于确认目标 SSID 可见及信号强度，正常应 >= -70dBm。 */
  if (esp_send_cmd("AT+CWLAP") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  return esp_wait_resp("OK", "ERROR", 10000U, NULL, 0U);
}

int esp_set_wifi_mode(uint8_t mode)
{
  char cmd[ESP_CMD_BUF_SIZE];

  /* AT+CWMODE=1: Station；=2: SoftAP；=3: 双模。应答 OK */
  (void)snprintf(cmd, sizeof(cmd), "AT+CWMODE=%u", (unsigned int)mode);
  if (esp_send_cmd(cmd) != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  return esp_wait_resp("OK", "ERROR", ESP_AT_TIMEOUT, NULL, 0U);
}

int esp_wifi_connect(char *ssid, char *pwd)
{
  char cmd[ESP_CMD_BUF_SIZE];
  uint8_t retry;
  int     rc = ESP_ERR_RESP;

  if ((ssid == NULL) || (pwd == NULL))
  {
    return ESP_ERR_PARAM;
  }
  /* 严格英文双引号/英文逗号：ssid/pwd 内部含 '"' 或 ',' 会破坏指令格式 */
  if ((strchr(ssid, '"') != NULL) || (strchr(ssid, ',') != NULL) ||
      (strchr(pwd, '"') != NULL) || (strchr(pwd, ',') != NULL))
  {
    return ESP_ERR_PARAM;
  }

  /* 1. 设置 STA 模式：AT+CWMODE=1 -> OK
     (模块若处于 AP 模式 CWMODE=2，AT+CWJAP 会直接返回 ERROR) */
  rc = esp_set_wifi_mode(1U);
  if (rc != ESP_OK)
  {
    return rc;
  }

  /* 2. AT+CWJAP="ssid","pwd"：连接 2.4G 路由器。
     必须等到 "WIFI GOT IP" 成功标记才算入网，之后才能继续 TCP 连接；
     失败 (FAIL / WIFI DISCONNECT / 超时) 自动重试 5 次；
     两次之间间隔 3s，避免模块还在 busy p... 处理上一条指令时被重复轰炸。 */
  (void)snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);

  for (retry = 0U; retry < 5U; retry++)
  {
    if (esp_send_cmd(cmd) != ESP_OK)
    {
      return ESP_ERR_TX;
    }
    rc = esp_wait_resp("WIFI GOT IP", "FAIL", ESP_WIFI_TIMEOUT, NULL, 0U);
    if (rc == ESP_OK)
    {
      esp_wifi_connected = 1U;
      return ESP_OK;
    }
    /* 固件兼容回退：部分 AT 固件 CWJAP 成功只回 OK，不打印 "WIFI GOT IP"。
       用 AT+CIPSTA? 验证是否真的拿到非 0.0.0.0 的 IP，拿到即视为入网成功。 */
    if (esp_wifi_check_ip() == ESP_OK)
    {
      esp_wifi_connected = 1U;
      return ESP_OK;
    }
    AT_PRINTF("[AT] CWJAP attempt %u/%u failed, retry...\r\n",
           (unsigned int)(retry + 1U), 5U);
    osDelay(3000U);
  }
  esp_wifi_connected = 0U;
  return rc;
}

int esp_get_ip(char *ip_buf)
{
  int   rc;
  char *ip_start;
  char *ip_end;

  if (ip_buf == NULL)
  {
    return ESP_ERR_PARAM;
  }

  /* AT+CIPSTA?：查询本机 IP，应答形如 +CIPSTA:ip:"192.168.1.100" */
  if (esp_send_cmd("AT+CIPSTA?") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  rc = esp_wait_resp("OK", "ERROR", ESP_AT_TIMEOUT, esp_resp_buf, sizeof(esp_resp_buf));
  if (rc != ESP_OK)
  {
    return rc;
  }

  /* 提取第一个双引号内的 IP 字符串 */
  ip_start = strchr((char *)esp_resp_buf, '"');
  if (ip_start == NULL)
  {
    return ESP_ERR_RESP;
  }
  ip_start++;
  ip_end = strchr(ip_start, '"');
  if (ip_end == NULL)
  {
    return ESP_ERR_RESP;
  }
  *ip_end = '\0';

  (void)strncpy(ip_buf, ip_start, ESP_IP_BUF_SIZE - 1U);
  ip_buf[ESP_IP_BUF_SIZE - 1U] = '\0';
  return ESP_OK;
}

/* 固件兼容回退：AT+CIPSTA? 确认模块是否真的拿到非 0.0.0.0 的 IP */
static int esp_wifi_check_ip(void)
{
  char ip[ESP_IP_BUF_SIZE];

  if (esp_get_ip(ip) != ESP_OK)
  {
    return ESP_ERR_RESP;
  }
  AT_PRINTF("[AT] current IP=%s\r\n", ip);
  if (strcmp(ip, "0.0.0.0") == 0)
  {
    return ESP_ERR_RESP;
  }
  return ESP_OK;
}

int esp_tcp_connect(char *ip, uint16_t port)
{
  char cmd[ESP_CMD_BUF_SIZE];
  uint8_t retry;
  int     rc = ESP_ERR_RESP;

  if ((ip == NULL) || (port == 0U))
  {
    return ESP_ERR_PARAM;
  }

  /* 1. AT+CIPSTART="TCP","ip",port：建立 TCP 连接，等待应答 "CONNECT"。
     【时序要点】本固件必须 TCP 连接成功之后才能设置透传模式，
     禁止在 CIPSTART 之前发送 AT+CIPMODE=1。失败重试 3 次，不复位模块。 */
  (void)snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", ip, (unsigned int)port);

  for (retry = 0U; retry < 3U; retry++)
  {
    if (esp_send_cmd(cmd) != ESP_OK)
    {
      return ESP_ERR_TX;
    }
    rc = esp_wait_resp("CONNECT", "FAIL", ESP_TCP_TIMEOUT, NULL, 0U);
    if (rc == ESP_OK)
    {
      esp_tcp_connected = 1U;
      break;
    }
    AT_PRINTF("[AT] CIPSTART attempt %u/3 failed, retry...\r\n",
           (unsigned int)(retry + 1U));
    osDelay(2000U);
  }
  if (rc != ESP_OK)
  {
    esp_tcp_connected = 0U;
    return ESP_ERR_RESP;
  }

  /* 2. TCP CONNECT 成功之后：AT+CIPMODE=1 开启透传模式 -> OK */
  if (esp_send_cmd("AT+CIPMODE=1") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  rc = esp_wait_resp("OK", "ERROR", ESP_AT_TIMEOUT, NULL, 0U);
  if (rc != ESP_OK)
  {
    return rc;
  }

  /* 3. AT+CIPSEND 进入透传：应答 '>' 提示符后，UART 数据直接进 TCP，
     服务器返回的数据直接出现在串口上 (无 +IPD 帧头) */
  if (esp_send_cmd("AT+CIPSEND") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  return esp_wait_resp(">", "ERROR", 3000U, NULL, 0U);
}

int esp_tcp_close(void)
{
  int rc;

  /* AT+CIPCLOSE：关闭 TCP 连接，应答 OK/CLOSED/ERROR 均可视为已关闭 */
  if (esp_send_cmd("AT+CIPCLOSE") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  rc = esp_wait_resp("OK", "ERROR", ESP_AT_TIMEOUT, NULL, 0U);
  esp_tcp_connected = 0U;
  return rc;
}

int esp_enter_transparent(void)
{
  /* 重新进入透传：首次进入由 esp_tcp_connect() 完成
     (CIPSTART->CONNECT->CIPMODE=1->CIPSEND->'>')。
     本函数用于 "+++" 退出透传后再次进入：此时 CIPMODE 仍为 1，
     只需 AT+CIPSEND 等待 '>' 即可。 */
  if (esp_send_cmd("AT+CIPSEND") != ESP_OK)
  {
    return ESP_ERR_TX;
  }
  return esp_wait_resp(">", "ERROR", 3000U, NULL, 0U);
}

int esp_exit_transparent(void)
{
  /* 透传模式下发送 "+++" (前后保持静默) 退出到命令模式，应答 OK */
  if (HAL_UART_Transmit(&huart2, (uint8_t *)"+++", 3U, ESP_TX_TIMEOUT) != HAL_OK)
  {
    return ESP_ERR_TX;
  }
  return esp_wait_resp("OK", "ERROR", 2000U, NULL, 0U);
}

int esp_tcp_send(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return ESP_ERR_PARAM;
  }
  /* 透传模式下直接发送原始字节 (MQTT 报文) */
  if (HAL_UART_Transmit(&huart2, (uint8_t *)data, len, ESP_TX_TIMEOUT) != HAL_OK)
  {
    return ESP_ERR_TX;
  }
  return (int)len;
}

uint16_t esp_rx_read(uint8_t *data, uint16_t len)
{
  return Esp_RingBuf_Read(&esp_rx_ring, data, len);
}

int esp_rx_wait(uint32_t timeout_ms)
{
  if (xSemaphoreTake(esp_rx_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
  {
    return ESP_OK;
  }
  return ESP_ERR_TIMEOUT;
}

int esp_rx_scan_link(uint8_t *data, uint16_t len)
{
  char   tmp[96];
  uint16_t n = (len < (uint16_t)(sizeof(tmp) - 1U)) ? len : (uint16_t)(sizeof(tmp) - 1U);

  if (n == 0U)
  {
    return 0;
  }
  memcpy(tmp, data, n);
  tmp[n] = '\0';

  /* MQTT 透传阶段：原始数据流中可能混入链路状态字符串 */
  if (strstr(tmp, "WIFI DISCONNECT") != NULL)
  {
    esp_wifi_connected = 0U;
    return 1;
  }
  if (strstr(tmp, "CLOSED") != NULL)
  {
    esp_tcp_connected = 0U;
    return 1;
  }
  return 0;
}

int esp_wifi_is_connected(void)
{
  return (int)esp_wifi_connected;
}

int esp_tcp_is_connected(void)
{
  return (int)esp_tcp_connected;
}

uint16_t esp_rx_overflow_cnt(void)
{
  return esp_rx_overflow;
}
