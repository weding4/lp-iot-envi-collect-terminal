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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include "filter.h"
#include "sensor.h"
#include "OLED.h"
#include "servo.h"
#include "motor.h"
#include "buzzer.h"
#include "control.h"
#include <string.h>   // 提供 strcmp, strncmp
#include <stdlib.h>   // 提供 atoi

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

volatile uint16_t adc_dma_buf[2];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

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
  /* USER CODE BEGIN 2 */

 // 启动DMA循环采集
	Sensor_Init(); // 模块内部完成：滤波初始化 + HAL_ADC_Start_DMA
	printf("System Start! ADC DMA running...\r\n");
	OLED_Init();          // 初始化 OLED
	OLED_Clear();
	OLED_ShowString(0, 0, "Env Monitor");  // 标题
	HAL_Delay(1000);
	OLED_Clear();
	
	Control_Init();      // 初始化舵机、电机、蜂鸣器

    // 串口命令提示（可选）
    printf("System ready. Commands:\r\n");
    printf("  servo <angle>  - set servo angle 0~180\r\n");
    printf("  motor fwd/stop/rev\r\n");
    printf("  buzzer on/off/beep\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
 
  
  while (1)
  {
	SensorStatus ts, ls;
        float temp = Sensor_ReadTemperature(&ts);
        float light = Sensor_ReadLightPercent(&ls);

        if (ts == SENSOR_OK && ls == SENSOR_OK) {
            OLED_Display_Env(temp, light);
            Control_Execute(temp, light);   // 阈值联动
            printf("Temp:%.1f C Light:%.1f %%\r\n", temp, light);
        } else {
            OLED_ShowString(0, 1, "Sensor Error");
        }

        // 串口命令解析（简单示例，后续可移至专门任务）
        static char rx_buf[32];
        if (HAL_UART_Receive(&huart1, (uint8_t*)rx_buf, sizeof(rx_buf)-1, 10) == HAL_OK) {
            rx_buf[sizeof(rx_buf)-1] = '\0';
            // 这里简单处理：判断开头字符串
            if (strncmp(rx_buf, "servo ", 6) == 0) {
                uint8_t angle = atoi(rx_buf+6);
                if (angle <= 180) {
                    Servo_SetAngle(angle);
                    printf("Servo set to %d\r\n", angle);
                }
            } else if (strcmp(rx_buf, "motor fwd") == 0) {
                Motor_SetState(MOTOR_FORWARD);
                printf("Motor forward\r\n");
            } else if (strcmp(rx_buf, "motor stop") == 0) {
                Motor_SetState(MOTOR_STOP);
                printf("Motor stop\r\n");
            } else if (strcmp(rx_buf, "buzzer on") == 0) {
                Buzzer_On();
                printf("Buzzer on\r\n");
            } else if (strcmp(rx_buf, "buzzer off") == 0) {
                Buzzer_Off();
                printf("Buzzer off\r\n");
            } else if (strcmp(rx_buf, "buzzer beep") == 0) {
                Buzzer_Beep(200);
                printf("Buzzer beep\r\n");
            }
            // 清空缓冲区（简化处理，实际应使用环形缓冲+DMA）
        }

        HAL_Delay(500);   // 500ms 周期   
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

/* USER CODE END 4 */

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
