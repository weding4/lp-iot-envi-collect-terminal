/**
  ******************************************************************************
  * @file    esp_at.h
  * @brief   ESP-01S AT 指令封装模块头文件 (USART2 + DMA + IDLE + FreeRTOS)
  * @note    STM32F103C8T6, HAL + FreeRTOS (CMSIS-RTOS v2)
  *          所有函数返回值约定：0 成功，负数失败
  ******************************************************************************
  */
#ifndef __ESP_AT_H
#define __ESP_AT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* 返回值定义：0 成功，负数失败 ----------------------------------------------*/
#define ESP_OK            0
#define ESP_ERR_TIMEOUT  (-1)  /* 等待 AT 应答超时 / 无应答 */
#define ESP_ERR_RESP     (-2)  /* 收到 ERROR / FAIL / CLOSED 等失败应答 */
#define ESP_ERR_PARAM    (-3)  /* 参数错误 */
#define ESP_ERR_TX       (-4)  /* USART2 发送失败 */

/* 可配置参数 ----------------------------------------------------------------*/
#define ESP_RX_BUF_SIZE    1024U   /* DMA/环形接收缓冲大小 (字节) */
#define ESP_AT_TIMEOUT     1000U   /* 常规 AT 应答超时 (ms) */
#define ESP_WIFI_TIMEOUT   15000U  /* AT+CWJAP 连接路由超时 (ms) */
#define ESP_TCP_TIMEOUT    10000U  /* AT+CIPSTART 建连超时 (ms) */
#define ESP_IP_BUF_SIZE    32U     /* IP 字符串缓冲大小 */

/* USART2 底层 ---------------------------------------------------------------*/
void esp_at_init(void);                /* 注册空闲中断回调 + 启动 DMA 接收 + 创建信号量 */
int  esp_uart_send_byte(uint8_t byte); /* USART2 单字节发送 (PA2 -> ESP-01S RX) */
int  esp_at_test(void);                /* AT 链路自检：发送 "AT" 等待 "OK" */
int  esp_reset(void);                  /* AT+RST 复位模块并等待 "ready" 就绪 */
int  esp_scan_ap(void);                /* AT+CWLAP 扫描热点，调试用 (打印 RSSI) */

/* AT 指令接口 (Day15) --------------------------------------------------------*/
int esp_set_wifi_mode(uint8_t mode);   /* AT+CWMODE=<mode> 设置工作模式 */
int esp_wifi_connect(char *ssid, char *pwd); /* AT+CWJAP 连接 WiFi 热点 */
int esp_get_ip(char *ip_buf);          /* AT+CIPSTA? 查询本机 IP */
int esp_tcp_connect(char *ip, uint16_t port); /* AT+CIPMODE=1 + AT+CIPSTART 建 TCP */
int esp_tcp_close(void);               /* AT+CIPCLOSE 关闭 TCP */
int esp_enter_transparent(void);       /* AT+CIPSEND 进入透传 (应答 '>') */
int esp_exit_transparent(void);        /* 发送 "+++" 退出透传 */
int esp_tcp_send(const uint8_t *data, uint16_t len); /* 透传模式下原始发送 */

/* 接收/状态辅助 (供 MQTT 移植层使用) ------------------------------------------*/
uint16_t esp_rx_read(uint8_t *data, uint16_t len);   /* 非阻塞读环形缓冲 */
int      esp_rx_wait(uint32_t timeout_ms);           /* 信号量等待新数据 (0 成功/-1 超时) */
int      esp_rx_scan_link(uint8_t *data, uint16_t len); /* 扫描 CLOSED/WIFI DISCONNECT */
int      esp_wifi_is_connected(void);
int      esp_tcp_is_connected(void);
uint16_t esp_rx_overflow_cnt(void);                  /* 环形缓冲溢出计数 */

#ifdef __cplusplus
}
#endif

#endif /* __ESP_AT_H */
