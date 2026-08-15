# lp‑iot‑envi‑collect‑terminal
> V2.0 物联网通信实现版 | STM32F103C8T6 + FreeRTOS + ESP‑01S(AT指令) + MQTT

## 📋项目简介
基于STM32F103的物联网环境采集终端。
- 使用ESP‑01S WiFi模块AT指令完成网络通信
- FreeRTOS多任务架构，外设驱动、AT解析、MQTT、传感器采集任务分离
- MQTT实现设备上报环境数据，云端下发JSON指令控制执行机构
- 硬件：光照ADC采集、直流电机、蜂鸣器、SG90舵机

## 🧰硬件平台
- MCU：STM32F103C8T6
- WiFi模块：ESP‑01S（USART2 PA2‑TX PA3‑RX，DMA+空闲中断接收）
- 执行器件
  - 直流电机：PB0(TIM3_CH3 PWM调速)，PB1(GPIO输出控制启停)
  - 蜂鸣器：PB5
  - SG90舵机：PA0(TIM2_CH1 50Hz PWM输出)
- 传感器：光照ADC采集

## ✨V2.0版本新增功能
1. ESP‑01S AT指令驱动，DMA+空闲中断 + 环形缓冲区接收解析AT响应
2. paho‑mqtt‑embedded‑c嵌入式MQTT客户端移植
3. MQTT连接Broker，主题定义
   - 上报遥测：`esp01s/device/telemetry`
   - 下发控制：`esp01s/device/cmd`
4. JSON报文解析，云端下发指令控制电机启停、蜂鸣器开关、舵机角度
5. 光照原始ADC转换为百分比0‑100进行上报
6. FreeRTOS多任务：AT任务、传感器采集上报任务，高优先级AT处理

### MQTT下发控制指令示例(JSON)
主题：`esp01s/device/cmd`
```json
{"motor":1,"buzzer":1,"servo_angle":90}