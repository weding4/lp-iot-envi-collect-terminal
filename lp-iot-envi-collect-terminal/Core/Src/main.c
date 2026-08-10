/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "filter.h"
#include "sensor.h"
#include "OLED.h"
#include "servo.h"
#include "motor.h"
#include "buzzer.h"
#include "control.h"  // 提供 atoi
#include "bsp_key.h"

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile uint8_t key_pressed = 0;
volatile uint16_t adc_dma_buf[2];

// 串口中断接收相关
#define RX_BUF_SIZE 64
uint8_t rx_buf[RX_BUF_SIZE];
uint8_t rx_index = 0;
volatile uint8_t rx_cmd_ready = 0;   // 收到完整一行命令

typedef struct {
    float temperature_c;
    float light_percent;
    uint16_t light_lx;
    SensorStatus temp_status;
    SensorStatus light_status;
    bool alarm_active;
    uint8_t alarm_count;
} sensor_data_t;

sensor_data_t sensor_data;
osMutexId_t sensor_data_mutex;
osSemaphoreId_t display_sem;
osSemaphoreId_t data_sem;
osMutexId_t printf_mutex;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */


int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/*void ProcessCommand(uint8_t *cmd)
{
    if (strncmp((char *)cmd, "servo ", 6) == 0) {
        int angle = atoi((char *)cmd + 6);
        if (angle >= 0 && angle <= 180) {
            Servo_SetAngle((uint8_t)angle);
            printf("Servo set to %d\r\n", angle);
        }
    } else if (strcmp((char *)cmd, "motor fwd") == 0) {
        Motor_SetState(MOTOR_FORWARD);
        printf("Motor forward\r\n");
    } else if (strcmp((char *)cmd, "motor stop") == 0) {
        Motor_SetState(MOTOR_STOP);
        printf("Motor stop\r\n");
    } else if (strcmp((char *)cmd, "motor rev") == 0) {
        Motor_SetState(MOTOR_REVERSE);
        printf("Motor reverse\r\n");
    } else if (strcmp((char *)cmd, "buzzer on") == 0) {
        Buzzer_On();
        printf("Buzzer on\r\n");
    } else if (strcmp((char *)cmd, "buzzer off") == 0) {
        Buzzer_Off();
        printf("Buzzer off\r\n");
    } else if (strcmp((char *)cmd, "buzzer beep") == 0) {
        Buzzer_Beep(200);
        printf("Buzzer beep\r\n");
    } else {
        printf("Unknown cmd: %s\r\n", cmd);
    }
}*/

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        if (rx_buf[rx_index] == '\r' || rx_buf[rx_index] == '\n') {
            rx_buf[rx_index] = '\0';          // 终止字符串
            if (rx_index > 0) {
                rx_cmd_ready = 1;             // 通知主循环处理
            }
            rx_index = 0;
        } else {
            rx_index++;
            if (rx_index >= RX_BUF_SIZE - 1) {
                rx_index = 0;                 // 溢出保护
            }
        }
        HAL_UART_Receive_IT(&huart1, &rx_buf[rx_index], 1); // 重新启动接收下一个字节
    }
}

void vTask_Acquisition(void *argument);
void vTask_Control(void *argument);
void vTask_Cloud(void *argument);
void vTask_Display(void *argument);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
	
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  BSP_Key_Init();

 // 启动DMA循环采集
	Sensor_Init(); // 模块内部完成：滤波初始化 + HAL_ADC_Start_DMA
	
	printf("System Start! ADC DMA running...\r\n");
	
	OLED_Init();          // 初始化 OLED
	OLED_Clear();
	OLED_ShowString(0, 0, "RTOS Tasks");  // 标题
	HAL_Delay(1000);
	OLED_Clear();
	Control_Init();      // 初始化舵机、电机、蜂鸣器

     // 启动串口中断接收
   // HAL_UART_Receive_IT(&huart1, &rx_buf[0], 1);
	printf("FreeRTOS Task framework started.\r\n");
	

osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */

 
 
// 同步对象创建

sensor_data_mutex = osMutexNew(NULL);
display_sem = osSemaphoreNew(1, 0, NULL);   // 初始计数 0，显示任务等待第一次采集
data_sem = display_sem;
printf_mutex = osMutexNew(NULL);
	
	
// 创建四个核心任务
	
osThreadAttr_t attr;

// 控制任务 (最高优先级)
attr.name = "Control";
attr.attr_bits = 0;
attr.cb_mem = NULL;
attr.cb_size = 0;
attr.stack_mem = NULL;
attr.stack_size = 512;
attr.priority = osPriorityAboveNormal;
osThreadId_t ctrl_id = osThreadNew(vTask_Control, NULL, &attr);
if (ctrl_id == NULL) {
    printf("Failed to create Control task!\r\n");
}

// 采集任务 (Normal)
attr.name = "Acquisition";
attr.priority = osPriorityNormal;
attr.stack_size = 512;
osThreadNew(vTask_Acquisition, NULL, &attr);

// 云通信任务 (Normal)
attr.name = "Cloud";
attr.stack_size = 512;   // 后续 MQTT 需要较大栈
osThreadNew(vTask_Cloud, NULL, &attr);

// 显示任务 (最低优先级)
attr.name = "Display";
attr.stack_size = 512;
attr.priority = osPriorityLow;
osThreadNew(vTask_Display, NULL, &attr);

 MX_FREERTOS_Init();

	

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
 
 // 裸机循环已被 FreeRTOS 替代，保留空循环防止 main 退出（其实不会执行到这里） 
  while (1)
  {
	/*SensorStatus ts, ls;
        float temp = Sensor_ReadTemperature(&ts);
        float light = Sensor_ReadLightPercent(&ls);

        if (ts == SENSOR_OK && ls == SENSOR_OK)
		{
            OLED_Display_Env(temp, light);
            Control_Execute(temp, light);
            printf("Temp:%.1f C Light:%.1f %%\r\n", temp, light);
        }
		else 
		{
            OLED_ShowString(0, 1, "Sensor Error");
        }

        // 处理串口命令
      if (rx_cmd_ready) 
		{
            rx_cmd_ready = 0;
            ProcessCommand(rx_buf);
        }
		if (BSP_Key_IsPressed())
{
    printf("Key pressed!\r\n");
}
        // 这里可以添加模式切换或唤醒操作
		HAL_Delay(200);   // 5Hz 循环 
  
  }    */
  
  
  
  
   
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != GPIO_PIN_4)
    {
        return;
    }

    /* Software debounce: ignore bounces within 50ms of the last accepted press. */
    static uint32_t last_press_tick = 0U;
    uint32_t now = HAL_GetTick();
    if ((now - last_press_tick) < 50U)
    {
        return;
    }
    last_press_tick = now;

    /* Speed stepping is only valid while the motor is running. */
    if (motor_duty == 0U)
    {
        return;
    }

    /* Cycle: 25% -> 50% -> 75% -> 100% -> 25%. */
    switch (motor_duty)
    {
        case 25U:  motor_duty = 50U;  break;
        case 50U:  motor_duty = 75U;  break;
        case 75U:  motor_duty = 100U; break;
        case 100U: motor_duty = 25U;  break;
        default:   motor_duty = 25U;  break;
    }

    Motor_SetSpeed(motor_duty);
    printf("[KEY] Motor duty -> %u%%\r\n", motor_duty);
}

void vTask_Acquisition(void *argument)
{
    (void)argument;

    // 前期等待传感器稳定
    osDelay(100);

    for(;;)
    {
        // 若 DMA 缓冲区连续 3 次为 0，则强制重启 DMA
        static uint8_t zero_cnt = 0;
        if (adc_dma_buf[0] == 0 && adc_dma_buf[1] == 0) {
            zero_cnt++;
            if (zero_cnt >= 3) {
                // 重新初始化 DMA
                HAL_ADC_Stop_DMA(&hadc1);
                HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buf, 2);
                zero_cnt = 0;
            }
        } else {
            zero_cnt = 0;
        }

        SensorStatus ts, ls;
        float temp = Sensor_ReadTemperature(&ts);
        float light_percent = Sensor_ReadLightPercent(&ls);
        float light_lx_value = Sensor_ReadLightLux(&ls);
        uint16_t light_lx = (uint16_t)light_lx_value;

        // 采集结果写入全局结构体并用信号量通知显示任务
        osMutexAcquire(sensor_data_mutex, osWaitForever);
        sensor_data.temperature_c = temp;
        sensor_data.light_percent = light_percent;
        sensor_data.light_lx = light_lx;
        sensor_data.temp_status = ts;
        sensor_data.light_status = ls;
        sensor_data.alarm_count = 0;
        osMutexRelease(sensor_data_mutex);

        osSemaphoreRelease(display_sem);

        // 500ms 一次把采集结果按要求打印成单行格式
        osMutexAcquire(printf_mutex, osWaitForever);
        printf("Temp: ADC=%u, %.1f C  |  Light: ADC=%u, %.1f %%\r\n",
               adc_dma_buf[0],
               temp,
               adc_dma_buf[1],
               light_percent);
        osMutexRelease(printf_mutex);

        osDelay(200);
    }
}

void vTask_Control(void *argument)
{
    (void)argument;

    for(;;) {
        float temp;
        float light_percent;

        osMutexAcquire(sensor_data_mutex, osWaitForever);
        temp = sensor_data.temperature_c;
        light_percent = sensor_data.light_percent;
        osMutexRelease(sensor_data_mutex);

        if (BSP_Key_IsPressed()) {
            printf("Key pressed!\r\n");
            key_pressed = 0;

            /* First press: force alarm latch active.
               Second press in the same state toggles it back to idle.
               The manual latch is visible to the control task and uses the
               requested actuator semantics: buzzer ON, motor FORWARD, servo 0°;
               the release path shuts all three off and returns servo to mid. */
            static bool manual_alarm = false;
            manual_alarm = !manual_alarm;
            Control_ManualOverride(manual_alarm);
        }

        Control_Execute(temp, light_percent);

        bool manual_alarm_active = Control_IsManualAlarmActive();
        bool threshold_alarm = ((temp > TEMP_THRESHOLD) || (light_percent > LIGHT_THRESHOLD));
        bool combined_alarm = manual_alarm_active || threshold_alarm;

        osMutexAcquire(sensor_data_mutex, osWaitForever);
        sensor_data.alarm_active = combined_alarm;
        sensor_data.alarm_count = (uint8_t)(combined_alarm ? 2 : 0);
        osMutexRelease(sensor_data_mutex);

        osDelay(200);
    }
}

void vTask_Cloud(void *argument)
{
	  
	
    for(;;)
    {
        osMutexAcquire(printf_mutex, osWaitForever);
		printf("[CLOUD] Cloud task running\r\n");
		osMutexRelease(printf_mutex);
		
        osDelay(2000);
    }
}

void vTask_Display(void *argument)
{
    (void)argument;

    for(;;) {
        osSemaphoreAcquire(display_sem, osWaitForever);

        float temp;
        float light_percent;
        bool alarm;
        SensorStatus temp_status;
        SensorStatus light_status;

        osMutexAcquire(sensor_data_mutex, osWaitForever);
        temp = sensor_data.temperature_c;
        light_percent = sensor_data.light_percent;
        alarm = sensor_data.alarm_active;
        temp_status = sensor_data.temp_status;
        light_status = sensor_data.light_status;
        osMutexRelease(sensor_data_mutex);

        char line1[16];
        char line2[16];
        char line3[16];

        if (temp_status == SENSOR_OK && light_status == SENSOR_OK) {
            snprintf(line1, sizeof(line1), "Temp:%.1f C", temp);
            snprintf(line2, sizeof(line2), "Light:%.1f %%", light_percent);
            snprintf(line3, sizeof(line3), "%s", alarm ? "Alarm" : "Normal");
        } else {
            snprintf(line1, sizeof(line1), "Temp:Error");
            snprintf(line2, sizeof(line2), "Light:Error");
            snprintf(line3, sizeof(line3), "Sensor Error");
        }

        /* Keep the OLED memory stable and refresh only the text rows.
           Avoid calling OLED_Clear() here, otherwise the panel blinks. */
        OLED_ShowString(0, 0, line1);
        OLED_ShowString(0, 1, line2);
        OLED_ShowString(0, 2, line3);

        osDelay(500);
    }
}



/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
