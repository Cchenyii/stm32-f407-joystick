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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ssd1306.h"
#include "sensor_protocol.h"
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
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* Definitions for TaskA */
osThreadId_t TaskAHandle;
const osThreadAttr_t TaskA_attributes = {
  .name = "TaskA",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskB */
osThreadId_t TaskBHandle;
const osThreadAttr_t TaskB_attributes = {
  .name = "TaskB",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2;
osMutexId_t uartMutexHandle;
const osMutexAttr_t uartMutex_attributes = {
  .name = "uartMutex"
};
/* adc samples filled by polling (no DMA) */
volatile uint16_t adc_dma_buf[2];
typedef struct {
  uint16_t x;
  uint16_t y;
  uint8_t direction;
  uint8_t sw;
} JoySample;
volatile JoySample g_joy = {2048, 2048, SP_JOY_CENTER, 0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void *argument);
void StartTask02(void *argument);

/* USER CODE BEGIN PFP */
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static uint16_t ADC_ReadChannel(uint32_t channel);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UartPrint(const char *s)
{
  if (osMutexAcquire(uartMutexHandle, osWaitForever) == osOK)
  {
    const uint16_t len = (uint16_t)strlen(s);
    HAL_UART_Transmit(&huart1, (uint8_t *)s, len, 100);
    osMutexRelease(uartMutexHandle);
  }
}
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
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  {
    const char boot[] = "boot\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)boot, sizeof(boot) - 1, 100);
  }
  if (SSD1306_Init(&hi2c1) != HAL_OK)
  {
    const char fail[] = "oled fail\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)fail, sizeof(fail) - 1, 100);
  }
  else
  {
    const char ok[] = "oled ok\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)ok, sizeof(ok) - 1, 100);
    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString("F407 JOYSTICK");
    SSD1306_Update();
  }
  {
    const char link[] = "USART2->ESP32B V1\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)link, sizeof(link) - 1, 100);
  }
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  uartMutexHandle = osMutexNew(&uartMutex_attributes);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of TaskA */
  TaskAHandle = osThreadNew(StartDefaultTask, NULL, &TaskA_attributes);

  /* creation of TaskB */
  TaskBHandle = osThreadNew(StartTask02, NULL, &TaskB_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = JOY_SW_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(JOY_SW_GPIO_Port, &GPIO_InitStruct);
  }
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static uint16_t ADC_ReadChannel(uint32_t channel)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = channel;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    return 0;
  }
  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    return 0;
  }
  if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
  {
    return 0;
  }
  return (uint16_t)HAL_ADC_GetValue(&hadc1);
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

#define JOY_CENTER   2048
#define JOY_DEADZONE 400   /* 中心附近晃动忽略 */

static uint16_t JoyFilter(uint16_t prev, uint16_t sample)
{
  /* 简单低通：新值占 1/4，旧值占 3/4，减少抖动 */
  return (uint16_t)((prev * 3U + sample) / 4U);
}

static uint8_t JoyDirCode(uint16_t x, uint16_t y)
{
  const int dx = (int)x - JOY_CENTER;
  const int dy = (int)y - JOY_CENTER;
  const int ax = (dx < 0) ? -dx : dx;
  const int ay = (dy < 0) ? -dy : dy;

  if (ax < JOY_DEADZONE && ay < JOY_DEADZONE)
  {
    return SP_JOY_CENTER;
  }

  if (ax >= ay)
  {
    return (dx > 0) ? SP_JOY_RIGHT : SP_JOY_LEFT;
  }
  return (dy > 0) ? SP_JOY_DOWN : SP_JOY_UP;
}

static const char *JoyDirName(uint8_t code)
{
  switch (code)
  {
    case SP_JOY_LEFT: return "LEFT";
    case SP_JOY_RIGHT: return "RIGHT";
    case SP_JOY_UP: return "UP";
    case SP_JOY_DOWN: return "DOWN";
    default: return "CENTER";
  }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the TaskA thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  char line[48];
  uint16_t fx = JOY_CENTER;
  uint16_t fy = JOY_CENTER;

  for (;;)
  {
    adc_dma_buf[0] = ADC_ReadChannel(ADC_CHANNEL_0);
    adc_dma_buf[1] = ADC_ReadChannel(ADC_CHANNEL_1);
    fx = JoyFilter(fx, adc_dma_buf[0]);
    fy = JoyFilter(fy, adc_dma_buf[1]);
    const unsigned sw =
        (HAL_GPIO_ReadPin(JOY_SW_GPIO_Port, JOY_SW_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    const uint8_t dir = JoyDirCode(fx, fy);

    g_joy.x = fx;
    g_joy.y = fy;
    g_joy.direction = dir;
    g_joy.sw = (uint8_t)sw;

    snprintf(line, sizeof(line), "X=%u Y=%u %s SW=%u\r\n",
             (unsigned)fx, (unsigned)fy, JoyDirName(dir), sw);
    UartPrint(line);

    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString("F407 JOYSTICK");
    snprintf(line, sizeof(line), "X=%u", (unsigned)fx);
    SSD1306_SetCursor(0, 2);
    SSD1306_WriteString(line);
    snprintf(line, sizeof(line), "Y=%u", (unsigned)fy);
    SSD1306_SetCursor(0, 3);
    SSD1306_WriteString(line);
    SSD1306_SetCursor(0, 5);
    SSD1306_WriteString(JoyDirName(dir));
    snprintf(line, sizeof(line), "SW=%u", sw);
    SSD1306_SetCursor(0, 6);
    SSD1306_WriteString(line);
    SSD1306_Update();

    osDelay(200);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the TaskB thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
  uint32_t sequence = 1;
  uint8_t frame[SP_MAX_FRAME_SIZE];
  uint8_t rx[16];
  char line[40];

  for (;;)
  {
    const JoySample sample = g_joy;
    const size_t len = Sp_EncodeJoystickFrame(
        sequence, sample.x, sample.y, sample.direction, sample.sw, frame,
        sizeof(frame));
    int acked = 0;
    if (len > 0)
    {
      for (int attempt = 0; attempt < 3 && !acked; ++attempt)
      {
        HAL_UART_Transmit(&huart2, frame, (uint16_t)len, 50);
        /* drain / wait ACK ~80ms */
        uint32_t got = 0;
        const uint32_t t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < 80u && got < sizeof(rx))
        {
          uint8_t b;
          if (HAL_UART_Receive(&huart2, &b, 1, 5) == HAL_OK)
          {
            rx[got++] = b;
            if (got >= 13u && Sp_IsAckOk(rx, got, sequence))
            {
              acked = 1;
              break;
            }
          }
        }
      }
    }

    snprintf(line, sizeof(line), "TX seq=%lu %s\r\n",
             (unsigned long)sequence, acked ? "ACK" : "NOACK");
    UartPrint(line);
    sequence++;
    osDelay(200);
  }
  /* USER CODE END StartTask02 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
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
  const char msg[] = "ERROR_HANDLER\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t *)msg, sizeof(msg) - 1, 100);
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
