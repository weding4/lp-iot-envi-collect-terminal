# 低功耗物联网环境采集终端（前三周版本）
基于 STM32F103C8T6 的物联网环境采集终端，已实现传感器采集、本地显示、执行器联动、FreeRTOS 多任务、ESP-01S AT 指令封装及 MQTT 数据上报。
项目状态：前三周开发完成，低功耗优化与 OTA 升级为后续计划。

## ✨ 功能特性
### 多源环境采集
- NTC 热敏电阻温度采集（ADC + DMA 循环 + 滑动平均滤波）
- 光敏电阻光照采集（输出百分比，可扩展标定为 lux）

### 本地显示
- 0.96 寸 OLED（SSD1306 I2C）实时显示温度、光照、设备状态

### 执行器联动
- 阈值触发：温度 >35℃ 或光照 >80% 时，蜂鸣器报警、舵机动作、电机启动
- 电机转速可按键调节（25% → 50% → 75% → 100% → 25%）

### FreeRTOS 多任务架构
- 采集任务、显示任务、控制任务、云通信任务独立运行
- 使用互斥量保护共享数据、信号量同步采集与显示

### 物联网通信
- USART2 + DMA 空闲中断驱动 ESP-01S
- AT 指令封装（WiFi 连接、TCP 建立、数据收发）
- 轻量 MQTT 客户端：CONNECT / PUBLISH 报文构建，JSON 数据上报至 Broker

## 🧰 硬件清单与接线
| 模块 | 型号/参数 | 连接引脚 |
| --- | --- | --- |
| 主控 | STM32F103C8T6 最小系统 | - |
| 显示 | 0.96 寸 OLED (SSD1306, I2C) | PB8 (SCL), PB9 (SDA) |
| 温度传感器 | NTC 热敏电阻 (10kΩ, B=3950) + 10kΩ 上拉 | PA0 (ADC_IN0) |
| 光照传感器 | 光敏电阻 + 10kΩ 上拉 | PA1 (ADC_IN1) |
| 舵机 | SG90 | PA8 (TIM1_CH1) |
| 电机驱动 | TB6612FNG | PWMA: PB0 (TIM3_CH3), AIN1: PB1, AIN2: GND, STBY: 3.3V |
| 蜂鸣器 | 有源蜂鸣器（3.3V 高电平触发） | PB4（可根据实际修改） |
| WiFi 模块 | ESP-01S (ESP8266) | PA2 (USART2_TX), PA3 (USART2_RX) |
| 按键 | 轻触按键 | PA4 (EXTI4，内部上拉) |
| 调试串口 | USB-TTL | PA9 (USART1_TX), PA10 (USART1_RX) |

> ⚠️ 电源：舵机与电机驱动必须独立 5V 供电，并与 STM32 共地。STM32 及传感器、OLED 使用 3.3V。

## 🏗️ 软件架构
### 任务划分（FreeRTOS，CMSIS-V2）
| 任务名 | 优先级 | 栈大小 (words) | 周期/触发 | 功能 |
| --- | --- | --- | --- | --- |
| vTask_Control | AboveNormal | 256 | 200 ms | 阈值判断、执行器联动、电机调速 |
| vTask_Acquisition | Normal | 256 | 500 ms | 传感器采集、滤波、更新全局数据 |
| vTask_Cloud | Normal | 512 | 5 s | MQTT 连接与周期发布 |
| vTask_Display | Low | 256 | 信号量触发 | OLED 刷新 |

### 同步机制
- 互斥量 `env_mutex`：保护全局环境数据结构 `env_data`
- 信号量 `data_sem`：采集完成 → 通知显示任务刷新
- 互斥量 `printf_mutex`：保护串口输出，避免多任务打印乱码

## 📁 目录结构
```text
Core/
├── Inc/
│   ├── main.h
│   ├── sensor.h
│   ├── oled.h
│   ├── oledfont.h
│   ├── servo.h
│   ├── motor.h
│   ├── buzzer.h
│   ├── control.h
│   ├── filter.h
│   ├── ringbuffer.h
│   ├── at_esp.h
│   ├── mqtt.h
│   └── FreeRTOSConfig.h
└── Src/
    ├── main.c
    ├── sensor.c
    ├── oled.c
    ├── servo.c
    ├── motor.c
    ├── buzzer.c
    ├── control.c
    ├── filter.c
    ├── ringbuffer.c
    ├── at_esp.c
    ├── mqtt.c
    └── stm32f1xx_it.c
```

## 🚀 快速开始
1. **环境准备**
   - STM32CubeMX（6.x）
   - Keil MDK5（勾选 Use MicroLIB）
   - ST-Link 驱动
2. **克隆仓库**
   ```bash
   git clone <你的仓库地址>
   ```
3. **CubeMX 生成代码**
   - 打开 .ioc 文件，检查 SYS → Timebase Source = TIM2（避免与 FreeRTOS SysTick 冲突）
   - 确认外设：ADC1、I2C1、TIM1、TIM3、USART1、USART2、GPIO、EXTI4
   - 生成代码前，在 Project Manager → User Files 中注册所有自定义 .c/.h 文件，防止覆盖丢失
4. **编译与烧录**
   - 打开 Keil 工程，Options for Target → Target 勾选 Use MicroLIB
   - 编译通过后，连接 ST-Link 下载（BOOT0 = 0）
   - 复位后，板载 LED（PC13）应闪烁数次：采集任务 4 次、显示任务 6 次、控制任务 8 次、云通信任务 10 次

## 📝 使用说明
### 串口命令（调试）
在 main.c 中预留了串口命令解析框架，可取消注释后使用：
- `servo <0-180>` ：设置舵机角度
- `motor fwd/stop/rev` ：控制电机方向
- `buzzer on/off/beep` ：控制蜂鸣器

### 按键调速
报警启动电机后，按下 PA4 按键循环调节电机转速（25% → 50% → 75% → 100% → 25%）

### MQTT 数据上报
修改 vTask_Cloud 中的 WiFi 凭据：
```c
AT_ConnectWiFi("你的SSID", "你的密码");
```
默认 Broker：test.mosquitto.org:1883

PC 端使用 MQTT 客户端（如 MQTT.fx）订阅主题 `/device/telemetry`，即可收到 JSON 数据：
```json
{"temp":26.5,"light":60.0,"alarm":0}
```

## ⚠️ 已知问题与后续计划
- 低功耗：尚未实现 STOP 休眠与 RTC 定时唤醒（第四周计划）
- MQTT 保活：当前未发送 PINGREQ，长时间运行需增加心跳
- OTA 升级：预留接口，尚未移植 bootloader
- WiFi 断线重连：框架已搭建，可进一步完善
- 传感器标定：光照值为百分比，如需实际 lux 需标定

## 👤 贡献者
个人项目：基于 STM32 HAL 库开发，参考开源社区驱动与例程

## 📄 许可证
MIT License

> 注：此 README 对应前三周开发进度，后续新增低功耗、OTA 等功能后可在此基础上更新。
