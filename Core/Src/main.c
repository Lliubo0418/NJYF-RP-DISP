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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
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
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint8_t i;
  uint8_t contrast = 40;
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
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  UI_Init();
  BSP_USART_Init();   /* 初始化 BSP 串口层（回调表） */
  Disp_Init();        /* 注册 USART1 协议接收回调并启动接收 */

  HAL_TIM_Base_Start_IT(&htim3); 

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
     UI_Task();
     Key_Task();   /* 非阻塞消抖：主循环完成按键确认 */

    /* 按键检测与处理 */
    if (Key_GetPressed(0)) // KEY3 - 返回键
    {
      UI_KeyK3_Back();
      // LCD_ShowBmp(bmp1);
      // delay_ms(1000);
    }
    if (Key_GetPressed(1)) // KEY4 - 上键/增加
    {
      UI_KeyK4_Up();
      // LCD_ShowBmp(bmp3);
      // delay_ms(1000);
    }
    if (Key_GetPressed(2)) // KEY5 - 循环键
    {
      UI_KeyK5_Loop();
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
    }
    if (Key_GetPressed(3)) // KEY6 - 确认键
    {
      UI_KeyK6_Enter();
      // LCD_ShowStr(0, 0, "CA12864I2Program");
      // delay_ms(1000);
      // LCD_ShowStr(0, 2, "SunSon ELEC-TECH");
      // delay_ms(1000);
      // LCD_ShowStr(0, 4, "TEL:755-29970110");
      // delay_ms(1000);
      // LCD_ShowStr(0, 6, "By LJ 2009.04.08");
      // delay_ms(1000);
    }

    /* 主循环节流：用 HAL 自带的 HAL_Delay（基于 SysTick 的 uwTick，见 stm32f1xx_it.c
     * SysTick_Handler → HAL_IncTick）。
     * ★不要用自定义的 delay_ms：原 Drivers/SYSTEM/delay 的 delay_init() 从未被调用，
     *   g_fac_us 恒为 0，导致 delay_ms() 实际是空操作（约 14ns），
     *   并且它还用强符号覆盖了 HAL_Delay，将来任何 HAL_Delay 调用都会空转。
     *   该模块已于 2026-09-20 删除。
     * ⚠ HAL_Delay 依赖 SysTick 中断（本工程 TICK_INT_PRIORITY=15，最低优先级），
     *   因此不能在优先级数字 <15 的中断服务程序里调用（会死等）。本处是主循环，安全。*/
    HAL_Delay(10);
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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
}

/* USER CODE BEGIN 4 */

/**
  * @brief  TIM3 更新中断（每 500ms 一次）回调
  * @note   由 HAL_TIM_IRQHandler(&htim3) → 本函数（覆盖 HAL 的 __weak 版本）。
  *
  *   定时周期核算（与 tim.c 的常量必须一致，改一处要同步另一处）：
  *     TIM3 挂在 APB1；APB1 预分频=2 ⇒ 时钟为 HCLK 的 2 倍 = 72MHz
  *     （STM32F1 的倍频规则：APBx 预分频 >1 时定时器时钟 = PCLKx × 2）
  *     Prescaler = 7200-1 ⇒ 计数频率 = 72MHz / 7200 = 10kHz
  *     Period    = 5000-1 ⇒ 溢出周期 = 5000 / 10kHz = 0.5s = 500ms ✓
  *
  *   LED 行为：PB1(LED_RUN) 每 500ms 翻转一次 ⇒ 亮/灭各 500ms，
  *     完整闪烁周期 1s（1Hz 心跳灯）。
  *
  *   ★中断优先级：TIM3 在 tim.c 里设为 4（比 USART1=1 低，不会抢串口）。
  *     本回调只做一次 GPIO 翻转，耗时纳秒级。若日后要在此回调里加耗时操作
  *     （打印、SPI 写屏等），请先确认它仍不会拖慢 USART1 接收导致丢帧。
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
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
