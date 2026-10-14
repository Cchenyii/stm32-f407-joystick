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
#include "uart_session.h"
#include "FreeRTOS.h"
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
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

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
osMutexId_t uartMutexHandle;
const osMutexAttr_t uartMutex_attributes = {
  .name = "uartMutex"
};
osMutexId_t joyMutexHandle;
const osMutexAttr_t joyMutex_attributes = {
  .name = "joyMutex"
};
typedef struct {
  uint16_t x;
  uint16_t y;
  uint8_t direction;
  uint8_t sw;
} JoySample;
JoySample g_joy = {2048, 2048, SP_JOY_CENTER, 0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void StartDefaultTask(void *argument);
void StartTask02(void *argument);

/* USER CODE BEGIN PFP */
static int ADC_ReadJoystick(uint16_t samples[2]);
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
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
    SSD1306_Fill(SSD1306_WHITE);
    SSD1306_FillRect(0, 0, 128, 11, SSD1306_BLACK);
    SSD1306_DrawText(3, 2, "ENV SENSOR", 1, SSD1306_WHITE);
    SSD1306_DrawText(14, 27, "STM32F407 READY", 1, SSD1306_BLACK);
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
  joyMutexHandle = osMutexNew(&joyMutex_attributes);
  if (uartMutexHandle == NULL || joyMutexHandle == NULL)
  {
    Error_Handler();
  }
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
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();
  /* Same preemption priority as USART2: serialize receive cursor updates. */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data
  * Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank
  * in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank
  * in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : JOY_SW_Pin */
  GPIO_InitStruct.Pin = JOY_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(JOY_SW_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static int ADC_ReadJoystick(uint16_t samples[2])
{
  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    return 0;
  }

  for (uint32_t i = 0; i < 2U; ++i)
  {
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
    {
      HAL_ADC_Stop(&hadc1);
      return 0;
    }
    samples[i] = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }

  HAL_ADC_Stop(&hadc1);
  return 1;
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
static void OledCenteredText(int16_t left, int16_t width, int16_t y, const char *text)
{
  const int16_t textWidth = (int16_t)SSD1306_TextWidth(text, 1);
  SSD1306_DrawText((int16_t)(left + (width - textWidth) / 2), y, text, 1, SSD1306_BLACK);
}

static void OledSensorValue(int16_t left, const char *text)
{
  const int16_t width = (int16_t)SSD1306_NumberWidth(text);
  if (width <= 60)
    SSD1306_DrawNumber((int16_t)(left + (64 - width) / 2), 24, text, SSD1306_BLACK);
  else
    OledCenteredText(left, 64, 27, text);
}

static void OledJoystickIcon(uint8_t direction)
{
  const int16_t cx = 9;
  const int16_t cy = 60;
  if (direction == SP_JOY_CENTER)
  {
    SSD1306_FillRect(cx - 1, cy - 3, 3, 1, SSD1306_WHITE);
    SSD1306_FillRect(cx - 1, cy + 3, 3, 1, SSD1306_WHITE);
    SSD1306_FillRect(cx - 3, cy - 1, 1, 3, SSD1306_WHITE);
    SSD1306_FillRect(cx + 3, cy - 1, 1, 3, SSD1306_WHITE);
    SSD1306_DrawPixel(cx - 2, cy - 2, SSD1306_WHITE);
    SSD1306_DrawPixel(cx + 2, cy - 2, SSD1306_WHITE);
    SSD1306_DrawPixel(cx - 2, cy + 2, SSD1306_WHITE);
    SSD1306_DrawPixel(cx + 2, cy + 2, SSD1306_WHITE);
    SSD1306_DrawPixel(cx, cy, SSD1306_WHITE);
    return;
  }
  const int16_t dx = direction == SP_JOY_LEFT ? -1 : direction == SP_JOY_RIGHT ? 1 : 0;
  const int16_t dy = direction == SP_JOY_UP ? -1 : direction == SP_JOY_DOWN ? 1 : 0;
  for (int16_t step = -3; step <= 3; ++step)
    SSD1306_DrawPixel((int16_t)(cx + step * dx), (int16_t)(cy + step * dy), SSD1306_WHITE);
  for (int16_t step = 1; step <= 3; ++step)
  {
    const int16_t tipX = (int16_t)(cx + (3 - step) * dx);
    const int16_t tipY = (int16_t)(cy + (3 - step) * dy);
    SSD1306_DrawPixel((int16_t)(tipX + step * dy), (int16_t)(tipY - step * dx), SSD1306_WHITE);
    SSD1306_DrawPixel((int16_t)(tipX - step * dy), (int16_t)(tipY + step * dx), SSD1306_WHITE);
  }
}

static void DrawSensorAndJoystick(const JoySample *joy, const SpLink_SensorSnapshot *sensor)
{
  char line[48];
  const char *status = "ONLINE";
  if (!sensor->has_session) status = "WAIT LINK";
  else if (!sensor->link_alive) status = "LINK LOST";
  else if (!sensor->wifi_connected) status = "WIFI OFF";
  else if (!sensor->tcp_connected) status = "WAIT TCP";
  else if (!sensor->valid) status = sensor->has_data ? "STALE" : "WAIT DATA";

  SSD1306_Fill(SSD1306_WHITE);
  SSD1306_FillRect(0, 0, 128, 11, SSD1306_BLACK);
  SSD1306_DrawText(3, 2, "ENV SENSOR", 1, SSD1306_WHITE);
  SSD1306_FillRect(66, 4, 3, 3, SSD1306_WHITE);
  if (!sensor->valid) SSD1306_DrawPixel(67, 5, SSD1306_BLACK);
  SSD1306_DrawText((int16_t)(124 - SSD1306_TextWidth(status, 1)), 2,
                   status, 1, SSD1306_WHITE);

  OledCenteredText(0, 64, 15, "TEMP C");
  OledCenteredText(64, 64, 15, "HUM %");
  SSD1306_FillRect(64, 14, 1, 23, SSD1306_BLACK);
  SSD1306_FillRect(4, 40, 120, 1, SSD1306_BLACK);
  SSD1306_DrawText(4, 46, "DISTANCE", 1, SSD1306_BLACK);
  SSD1306_DrawText(114, 48, "CM", 1, SSD1306_BLACK);
  if (sensor->valid)
  {
    const int32_t temperature = sensor->temperature_centi_c;
    const uint32_t magnitude = (uint32_t)(temperature < 0 ? -temperature : temperature);
    snprintf(line, sizeof(line), "%s%lu.%02lu", temperature < 0 ? "-" : "",
             (unsigned long)(magnitude / 100u), (unsigned long)(magnitude % 100u));
    OledSensorValue(0, line);
    snprintf(line, sizeof(line), "%u.%02u", (unsigned)(sensor->humidity_centi_percent / 100u),
             (unsigned)(sensor->humidity_centi_percent % 100u));
    OledSensorValue(64, line);
    snprintf(line, sizeof(line), "%u", (unsigned)sensor->distance_cm);
  }
  else
  {
    OledSensorValue(0, "--");
    OledSensorValue(64, "--");
    snprintf(line, sizeof(line), "--");
  }
  SSD1306_DrawNumber((int16_t)(110 - SSD1306_NumberWidth(line)), 43, line, SSD1306_BLACK);

  SSD1306_FillRect(0, 56, 128, 8, SSD1306_BLACK);
  OledJoystickIcon(joy->direction);
  snprintf(line, sizeof(line), "X%u Y%u", (unsigned)joy->x, (unsigned)joy->y);
  SSD1306_DrawText(20, 57, line, 1, SSD1306_WHITE);
  if (joy->sw) SSD1306_FillRect(99, 56, 29, 8, SSD1306_WHITE);
  snprintf(line, sizeof(line), "SW:%u", (unsigned)joy->sw);
  SSD1306_DrawText(102, 57, line, 1, joy->sw ? SSD1306_BLACK : SSD1306_WHITE);
  SSD1306_Update();
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
  uint16_t adcSamples[2] = {JOY_CENTER, JOY_CENTER};

  for (;;)
  {
    if (ADC_ReadJoystick(adcSamples))
    {
      fx = JoyFilter(fx, adcSamples[0]);
      fy = JoyFilter(fy, adcSamples[1]);
    }
    const unsigned sw =
        (HAL_GPIO_ReadPin(JOY_SW_GPIO_Port, JOY_SW_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    const uint8_t dir = JoyDirCode(fx, fy);

    const JoySample sample = {fx, fy, dir, (uint8_t)sw};
    if (osMutexAcquire(joyMutexHandle, osWaitForever) == osOK)
    {
      g_joy = sample;
      osMutexRelease(joyMutexHandle);
    }

    snprintf(line, sizeof(line), "X=%u Y=%u %s SW=%u\r\n",
             (unsigned)fx, (unsigned)fy, JoyDirName(dir), sw);
    UartPrint(line);

    SpLink_SensorSnapshot sensor;
    SpLink_GetSensorSnapshot(&sensor);
    DrawSensorAndJoystick(&sample, &sensor);

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
  char line[96];
  if (!SpLink_Init(&huart2))
  {
    UartPrint("UART session init/self-test FAILED\r\n");
    for (;;) osDelay(1000);
  }
  JoySample sample = {JOY_CENTER, JOY_CENTER, SP_JOY_CENTER, 0};
  UartPrint("USART2 DMA circular RX + IDLE; receive self-test PASS (8/8)\r\n");
  for (;;)
  {
    if (osMutexAcquire(joyMutexHandle, osWaitForever) == osOK)
    {
      sample = g_joy;
      osMutexRelease(joyMutexHandle);
    }
    const int acked = SpLink_SendJoystick(sample.x, sample.y,
                                          sample.direction, sample.sw);
    const uint64_t current_session = SpLink_GetSession();
    snprintf(line, sizeof(line), "TX session=%08lx%08lx seq=%lu %s\r\n",
             (unsigned long)(current_session >> 32), (unsigned long)current_session,
             (unsigned long)SpLink_GetLastSequence(), acked ? "ACK" : "NOACK");
    UartPrint(line);
    g_spLinkRxStats.task_a_stack_free_bytes = osThreadGetStackSpace(TaskAHandle);
    g_spLinkRxStats.free_heap_bytes = xPortGetFreeHeapSize();
    g_spLinkRxStats.minimum_free_heap_bytes = xPortGetMinimumEverFreeHeapSize();
    for (unsigned idle = 0; idle < 20; ++idle)
    {
      SpLink_Poll();
      osDelay(10);
    }
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
