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
#include "spi.h"
<<<<<<< HEAD
=======
#include "tim.h"
>>>>>>> origin/develop
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
<<<<<<< HEAD
#include "delay.h"
=======
>>>>>>> origin/develop
#include "oledfont.h"
#include "oled_ui.h"
#include "bsp_key.h"
#include "bsp_usart.h"
#include "app_disp.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

<<<<<<< HEAD
=======

>>>>>>> origin/develop
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
<<<<<<< HEAD
 * @brief  The application entry point.
 * @retval int
 */
=======
  * @brief  The application entry point.
  * @retval int
  */
>>>>>>> origin/develop
int main(void)
{

  /* USER CODE BEGIN 1 */
<<<<<<< HEAD
  uint8_t i;
  uint8_t contrast = 40;
=======
>>>>>>> origin/develop
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
  MX_SPI1_Init();
  MX_USART1_UART_Init();
<<<<<<< HEAD
  /* USER CODE BEGIN 2 */
  // LCD_init();
  UI_Init();
  BSP_USART_Init();   /* 初始化 BSP 串口层（回调表） */
  Disp_Init();        /* 注册 USART1 协议接收回调并启动接收 */
=======
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  UI_Init();
  BSP_USART_Init();   /* 初始化 BSP 串口层（回调表） */
  Disp_Init();        /* 注册 USART1 协议接收回调并启动接收 */

  HAL_TIM_Base_Start_IT(&htim3); 

>>>>>>> origin/develop
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
<<<<<<< HEAD
     UI_Task();
=======
     Disp_SendAckPoll();  /* 写参数 ACK（0x07）超时重发与结果判定：非阻塞，必须每轮调用 */
     UI_Task();
     Key_Task();   /* 非阻塞消抖：主循环完成按键确认 */
>>>>>>> origin/develop

    /* 按键检测与处理 */
    if (Key_GetPressed(0)) // KEY3 - 返回键
    {
      UI_KeyK3_Back();
<<<<<<< HEAD
      // LCD_ShowBmp(bmp1);
      // delay_ms(1000);
=======
>>>>>>> origin/develop
    }
    if (Key_GetPressed(1)) // KEY4 - 上键/增加
    {
      UI_KeyK4_Up();
<<<<<<< HEAD
      // LCD_ShowBmp(bmp3);
      // delay_ms(1000);
=======
>>>>>>> origin/develop
    }
    if (Key_GetPressed(2)) // KEY5 - 循环键
    {
      UI_KeyK5_Loop();
<<<<<<< HEAD
      // for (i = (contrast - 5); i < (contrast + 5); i++)
      // {
      //   LCD_WR_REG(0x81);
      //   LCD_WR_REG(0x3F & i);
      //   LCD_ShowNum(2, 0, i);
      //   delay_ms(1000);
      // }
      // LCD_WR_REG(0x81);
      // LCD_WR_REG(contrast);
      // LCD_ShowNum(2, 0, contrast);
      // delay_ms(1000);
=======
>>>>>>> origin/develop
    }
    if (Key_GetPressed(3)) // KEY6 - 确认键
    {
      UI_KeyK6_Enter();
<<<<<<< HEAD
      // LCD_ShowStr(0, 0, "CA12864I2Program");
      // delay_ms(1000);
      // LCD_ShowStr(0, 2, "SunSon ELEC-TECH");
      // delay_ms(1000);
      // LCD_ShowStr(0, 4, "TEL:755-29970110");
      // delay_ms(1000);
      // LCD_ShowStr(0, 6, "By LJ 2009.04.08");
      // delay_ms(1000);
    }

    delay_ms(10);
=======
    }

    HAL_Delay(10);
>>>>>>> origin/develop
  }
  /* USER CODE END 3 */
}

/**
<<<<<<< HEAD
 * @brief System Clock Configuration
 * @retval None
 */
=======
  * @brief System Clock Configuration
  * @retval None
  */
>>>>>>> origin/develop
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
<<<<<<< HEAD
   * in the RCC_OscInitTypeDef structure.
   */
=======
  * in the RCC_OscInitTypeDef structure.
  */
>>>>>>> origin/develop
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
<<<<<<< HEAD
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
=======
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
>>>>>>> origin/develop
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

<<<<<<< HEAD
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
=======
/**
  * @brief  TIM3 更新中断回调（500ms 一次）：翻转 LED_RUN 做 1Hz 心跳
  * @param  htim 触发中断的定时器句柄
  * @retval 无
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* 周期核算：TIM3 挂 APB1，APB1 分频=2 ⇒ 定时器时钟 72MHz；Prescaler=7200-1 ⇒ 10kHz；
   * Period=5000-1 ⇒ 0.5s。⚠ 与 tim.c 的常量必须一致，改一处要同步另一处。
   * ⚠ TIM3 优先级 4（低于 USART1=1，不抢串口）；本回调只翻转一次 GPIO，
   *   日后若在此加耗时操作，须先确认不会拖慢 USART1 接收导致丢帧。 */
  if (htim->Instance == TIM3)
  {
    HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
>>>>>>> origin/develop
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
<<<<<<< HEAD
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
=======
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
>>>>>>> origin/develop
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
