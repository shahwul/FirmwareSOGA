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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PORT_VALVE GPIOA
#define VALVE_AIR GPIO_PIN_4
#define VALVE_NUTRISI GPIO_PIN_5
#define VALVE_MIX GPIO_PIN_6
#define VALVE_IRIGASI GPIO_PIN_7

#define POMPA_ON(speed) __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed)
#define POMPA_OFF() __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0)
#define VALVE_ON(pin) HAL_GPIO_WritePin(PORT_VALVE, pin, GPIO_PIN_SET)
#define VALVE_OFF(pin) HAL_GPIO_WritePin(PORT_VALVE, pin, GPIO_PIN_RESET)

#define VALVE_OFF_ALL()                                                        \
  do {                                                                         \
    VALVE_OFF(VALVE_AIR);                                                      \
    VALVE_OFF(VALVE_NUTRISI);                                                  \
    VALVE_OFF(VALVE_MIX);                                                      \
    VALVE_OFF(VALVE_IRIGASI);                                                  \
  } while (0)

#define CAIRAN_AIR 0
#define CAIRAN_NUTRISI 1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;
DMA_HandleTypeDef hdma_tim4_ch3;

UART_HandleTypeDef huart1;

osThreadId defaultTaskHandle;
osThreadId TaskTelemetryHandle;
osThreadId TaskIrrigationHandle;
/* USER CODE BEGIN PV */
volatile uint32_t water_pulse_count = 0;
volatile uint32_t nutrisi_pulse_count = 0;

volatile float current_vpd = 1.0f;
volatile float current_moisture = 100.0f;
volatile uint8_t flag_manual_air = 0;
volatile uint8_t flag_manual_nutrisi = 0;
char status_mesin[32] = "STANDBY"; // Buat laporan ke aplikasi

volatile uint8_t status_data_valid = 0;

volatile uint32_t total_siklus_irigasi = 0;

volatile uint32_t tick_siram_berikutnya = 0;
volatile uint8_t mesin_sedang_menyiram = 0;

uint8_t rxByte;
char rxBuffer[512];
volatile uint16_t rxIndex = 0;

uint32_t dma_capture_data[42];

volatile uint8_t latch_dosing_event = 0;
volatile uint8_t is_auto_mode = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_RTC_Init(void);
void StartDefaultTask(void const *argument);
void StartTaskTelemetry(void const *argument);
void StartTaskIrrigation(void const *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len) {
  // Tembak data printf langsung ke port USB Type-C
  CDC_Transmit_FS((uint8_t *)ptr, len);

  // Kasih delay super tipis biar buffer USB ga jebol kalau datanya kepanjangan
  osDelay(2);
  return len;
}

typedef struct {
  uint8_t valve_1;  // Valve Air
  uint8_t valve_2;  // Valve Nutrisi
  uint8_t valve_3;  // Valve Mix
  uint8_t valve_4;  // Valve Irigasi
  uint8_t pump_pwm; // Nilai 0 - 100
} SogaActuator_t;

SogaActuator_t FarmActuators = {0, 0, 0, 0, 0}; // Default mati semua

void DWT_Init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint16_t us) {
  //    __HAL_TIM_SET_COUNTER(&htim3, 0);
  //    while (__HAL_TIM_GET_COUNTER(&htim3) < us) {
  //    }
}

// Helper untuk mengubah pin PB2 menjadi Output
void DHT_Set_Pin_Output(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

// Helper untuk mengubah pin PB2 menjadi Input (Pull-Up)
void DHT_Set_Pin_Input(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void ParseActuatorJSON(char *jsonBuffer, SogaActuator_t *actuators) {
  char *ptr;
  int temp_val;

  // Pakai int sementara biar sscanf newlib-nano GCC nggak error
  if ((ptr = strstr(jsonBuffer, "\"v1\":")) != NULL) {
    sscanf(ptr, "\"v1\":%d", &temp_val);
    actuators->valve_1 = temp_val;
  }
  if ((ptr = strstr(jsonBuffer, "\"v2\":")) != NULL) {
    sscanf(ptr, "\"v2\":%d", &temp_val);
    actuators->valve_2 = temp_val;
  }
  if ((ptr = strstr(jsonBuffer, "\"v3\":")) != NULL) {
    sscanf(ptr, "\"v3\":%d", &temp_val);
    actuators->valve_3 = temp_val;
  }
  if ((ptr = strstr(jsonBuffer, "\"v4\":")) != NULL) {
    sscanf(ptr, "\"v4\":%d", &temp_val);
    actuators->valve_4 = temp_val;
  }

  // Parse PWM Pompa (Batasi maksimal 100 biar hardware ga jebol)
  if ((ptr = strstr(jsonBuffer, "\"pwm\":")) != NULL) {
    sscanf(ptr, "\"pwm\":%d", &temp_val);
    actuators->pump_pwm = (temp_val > 100) ? 100 : temp_val;
  }
}

void ApplyHardwareState(SogaActuator_t *actuators) {
  // Eksekusi Relay Valve
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4,
                    actuators->valve_1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5,
                    actuators->valve_2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6,
                    actuators->valve_3 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7,
                    actuators->valve_4 ? GPIO_PIN_SET : GPIO_PIN_RESET);

  // Eksekusi PWM Pompa
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, actuators->pump_pwm);
}

void TriggerEmergencyStop() {
  FarmActuators.valve_1 = 0;
  FarmActuators.valve_2 = 0;
  FarmActuators.valve_3 = 0;
  FarmActuators.valve_4 = 0;
  FarmActuators.pump_pwm = 0;
  ApplyHardwareState(&FarmActuators);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    // Jika mendeteksi karakter Enter (\n), paket JSON selesai dikirim
    if (rxByte == '\n') {
      rxBuffer[rxIndex] = '\0'; // Kunci string
      rxIndex = 0;              // Reset index untuk paket selanjutnya

      // BANGUNKAN TASK 1 SEKETIKA DARI DALAM INTERUPSI!
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      vTaskNotifyGiveFromISR(TaskTelemetryHandle, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
      // Amankan dari Over-Buffer yang bisa bikin STM32 crash
      if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = rxByte;
      } else {
        rxIndex = 0;
      }
    }

    // RE-ARM: Aktifkan kembali interupsi untuk menunggu byte berikutnya
    // BUG HAL: Buka paksa lock agar tidak bertabrakan dengan HAL_UART_Transmit
    huart->Lock = HAL_UNLOCKED;
    HAL_UART_Receive_IT(huart, &rxByte, 1);
  }
}

// FUNGSI PENYELAMAT: Jika terjadi Overrun Error (interrupt telat merespon
// karena CPU sibuk), HAL akan mematikan receiver selamanya. Fungsi ini akan
// membangkitkan receiver kembali!
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    // Bersihkan semua bendera error (Overrun, Noise, Frame Error)
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);

    // Buka paksa lock dan nyalakan lagi interupsi RX!
    huart->Lock = HAL_UNLOCKED;
    HAL_UART_Receive_IT(huart, &rxByte, 1);
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_0) // PB0 (Flow Sensor Air)
  {
    water_pulse_count++;
  } else if (GPIO_Pin == GPIO_PIN_1) // PB1 (Flow Sensor Nutrisi)
  {
    nutrisi_pulse_count++;
  } else if (GPIO_Pin == GPIO_PIN_5) // PB5 (Tombol Manual / Bypass)
  {
    static uint32_t last_press_time = 0;
    if (osKernelSysTick() - last_press_time > 300) // Jeda 300ms
    {
      last_press_time = osKernelSysTick();

      // COBA BANGUNKAN TASK IRIGASI SECARA PAKSA!
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      vTaskNotifyGiveFromISR(TaskIrrigationHandle, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

uint8_t TakarCairan(uint8_t tipe_cairan, float target_liter) {
  uint32_t target_pulse = 0;
  volatile uint32_t *p_pulse_count = NULL; // Pointer penunjuk alamat RAM

  // Pemilihan Sensor & Kalibrasi Dinamis Berdasarkan Karakteristik Cairan
  if (tipe_cairan == CAIRAN_AIR) {
    target_pulse =
        (uint32_t)(target_liter * 450.0f); // Kalibrasi Air (1L = 450 pulse)
    p_pulse_count = &water_pulse_count;    // Arahkan pointer ke RAM Air
    printf("[DOSING] Menakar %.2f L Air Murni...\n", target_liter);
  } else if (tipe_cairan == CAIRAN_NUTRISI) {
    // Info: Cairan nutrisi pekat biasanya lebih kental,
    // nilai kalibrasi (misal 465.0f) bisa disesuaikan dengan massa jenisnya
    // nanti
    target_pulse = (uint32_t)(target_liter * 465.0f);
    p_pulse_count = &nutrisi_pulse_count; // Arahkan pointer ke RAM Nutrisi
    printf("[DOSING] Menakar %.2f L Nutrisi Pekat...\n", target_liter);
  }

  if (p_pulse_count == NULL)
    return 0; // Guard clause keselamatan memori

  *p_pulse_count = 0; // Reset counter sensor terpilih ke angka 0 lewat pointer
  uint32_t safety_timer = 0;

  // Looping Tunggu Cairan Sampai Target Tercapai
  while (*p_pulse_count < target_pulse) {
    uint32_t pulse_awal = *p_pulse_count;

    osDelay(100);
    safety_timer += 100;

    // Timeout Safety System (Jika air/nutrisi mampet 5 detik, matikan sekering)
    if ((*p_pulse_count == pulse_awal) && (safety_timer > 5000)) {
      printf("[FATAL ERROR] Aliran Macet pada Saluran Tipe: %d!\n",
             tipe_cairan);
      return 0; // Fail
    }

    if (*p_pulse_count > pulse_awal) {
      safety_timer = 0; // Reset safety timer karena cairan mengalir deras
    }
  }

  printf("[DOSING] Sukses! Cairan tipe %d tertakar sempurna.\n", tipe_cairan);
  return 1; // Success
}

float Read_MixingTank_Distance(void) {
  // Pastikan Trig LOW dulu
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
  delay_us(2);

  // Tembak pulsa 10 mikrodetik ke pin TRIG AJ-SR04M
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
  delay_us(10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

  uint32_t timeout = 500000;
  // Tunggu sampai pin ECHO bernilai HIGH
  while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
    if (--timeout == 0)
      return -1.0f; // Batalkan jika sensor putus
  }

  uint32_t startTime = DWT->CYCCNT;
  timeout = 500000;
  // Hitung berapa lama pin ECHO bernilai HIGH
  while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_SET) {
    if (--timeout == 0)
      return -1.0f;
  }
  uint32_t durationTicks = DWT->CYCCNT - startTime;

  // Konversi tick prosessor ke mikrodetik, lalu hitung jarak (cm)
  float durationUs =
      (float)durationTicks / ((float)HAL_RCC_GetHCLKFreq() / 1000000.0f);
  return (durationUs * 0.0343f) / 2.0f;
}

uint8_t IsTankEmpty(void) {
  float distance = Read_MixingTank_Distance();
  if (distance > 27.0f || distance < 0) {
    return 1;
  }
  return 0;
}

void Get_SystemUptime(char *timeBuffer) {
  uint32_t total_seconds = osKernelSysTick() / 1000;
  uint32_t hours = total_seconds / 3600;
  total_seconds %= 3600;
  uint32_t minutes = total_seconds / 60;
  uint32_t seconds = total_seconds % 60;

  sprintf(timeBuffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
}

/* ============= IRRIGATION CONTROL SYSTEM ============= */
#define DWELL_TIME_MS 900000 // 15 min (Kompensasi pure dead-time)
#define DEADBAND_PCT 5.0f    // Histeresis: Jangan aktuasi jika error < 5%
#define SP_MIN 70.0f         // Batas bawah setpoint (Kondisi sejuk)
#define SP_MAX 80.0f // Batas atas setpoint (Kondisi panas/kering ekstrem)

volatile float dynamic_sp = 70.0f;         // Inisialisasi Dynamic setpoint
volatile uint8_t latch_nutrient_event = 0; // Latch untuk event nutrisi

/**
 * @brief Menghitung Dynamic Setpoint berbasis Feed-Forward VPD
 * @param vpd Nilai Vapor Pressure Deficit (kPa) dari sensor udara
 * @retval Target persentase kelembapan tanah (%)
 */
float compute_dynamic_sp(float vpd) {
  // Persamaan Linier Feed-Forward: Base SP = 70%, Gain = 5.0 per satuan VPD
  // Contoh: VPD 1.0 kPa -> SP 75.0% | VPD 2.0 kPa -> SP 80.0%
  float sp = 70.0f + (5.0f * vpd);

  // Anti-Windup / Saturasi Proteksi
  if (sp < SP_MIN) {
    sp = SP_MIN;
  }
  if (sp > SP_MAX) {
    sp = SP_MAX;
  }

  return sp;
}
void report_control_state(float vpd, float sp, float pv, float error,
                          const char *action) {
  char buf[200];
  snprintf(buf, sizeof(buf),
           "{\"type\":\"control_state\",\"vpd\":%.2f,\"sp\":%.1f,\"pv\":%.1f,"
           "\"error\":%.1f,\"action\":\"%s\"}\n",
           vpd, sp, pv, error, action);
  HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
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
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  //  HAL_TIM_Base_Start(&htim3);
  //    printf("[SYSTEM] Hardware Timer 3 Mikrodetik Aktif!\n");
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
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
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of TaskTelemetry */
  osThreadDef(TaskTelemetry, StartTaskTelemetry, osPriorityNormal, 0, 512);
  TaskTelemetryHandle = osThreadCreate(osThread(TaskTelemetry), NULL);

  /* definition and creation of TaskIrrigation */
  osThreadDef(TaskIrrigation, StartTaskIrrigation, osPriorityNormal, 0, 256);
  TaskIrrigationHandle = osThreadCreate(osThread(TaskIrrigation), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief RTC Initialization Function
 * @param None
 * @retval None
 */
static void MX_RTC_Init(void) {

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
   */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 41;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 100;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);
}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void) {

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 83;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim4) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_3) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

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
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, v1_Pin | v2_Pin | v3_Pin | v4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DHT22_DATA_Pin | US_TRIG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : v1_Pin v2_Pin v3_Pin v4_Pin */
  GPIO_InitStruct.Pin = v1_Pin | v2_Pin | v3_Pin | v4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : FLOW_AIR_Pin FLOW_NUTRISI_Pin */
  GPIO_InitStruct.Pin = FLOW_AIR_Pin | FLOW_NUTRISI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : DHT22_DATA_Pin */
  GPIO_InitStruct.Pin = DHT22_DATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT22_DATA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : US_TRIG_Pin */
  GPIO_InitStruct.Pin = US_TRIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(US_TRIG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : US_ECHO_Pin */
  GPIO_InitStruct.Pin = US_ECHO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(US_ECHO_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const *argument) {
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for (;;) {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTaskTelemetry */
/**
 * @brief Function implementing the TaskTelemetry thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskTelemetry */
void StartTaskTelemetry(void const *argument) {
  /* USER CODE BEGIN StartTaskTelemetry */
  /* Infinite loop */
  char jsonTxBuffer[256];
  uint32_t last_tx_time = osKernelSysTick();

  HAL_UART_Receive_IT(&huart1, &rxByte, 1);
  printf("[SYSTEM] Task Telemetri Tank Siap Tempur!\n");

  for (;;) {
    // ========================================================
    // 1. INBOUND (RX) - BACA DATA MONITOR ATAU PERINTAH
    // ========================================================
    uint32_t rx_notif = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

    if (rx_notif > 0) {
      if (strstr(rxBuffer, "\"cmd\":\"sync_rtc\"") != NULL) {
        int jam = 0, menit = 0, detik = 0;

        char *ptr_h = strstr(rxBuffer, "\"h\":");
        char *ptr_m = strstr(rxBuffer, "\"m\":");
        char *ptr_s = strstr(rxBuffer, "\"s\":");

        if (ptr_h)
          sscanf(ptr_h, "\"h\":%d", &jam);
        if (ptr_m)
          sscanf(ptr_m, "\"m\":%d", &menit);
        if (ptr_s)
          sscanf(ptr_s, "\"s\":%d", &detik);

        RTC_TimeTypeDef sTime = {0};
        sTime.Hours = (uint8_t)jam;
        sTime.Minutes = (uint8_t)menit;
        sTime.Seconds = (uint8_t)detik;
        sTime.TimeFormat = RTC_HOURFORMAT_24;

        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        printf("\n[RTC] Waktu berhasil disinkronkan dari ESP32 -> "
               "%02d:%02d:%02d\n",
               jam, menit, detik);
      }

      char *ptr_vpd = strstr(rxBuffer, "\"vpd\":");
      if (ptr_vpd != NULL)
        sscanf(ptr_vpd, "\"vpd\":%f", &current_vpd);

      char *ptr_soil = strstr(rxBuffer, "\"soil_moisture\":[");
      if (ptr_soil != NULL) {
        sscanf(ptr_soil, "\"soil_moisture\":[%f", &current_moisture);
      }

      if (ptr_vpd != NULL || ptr_soil != NULL) {
        dynamic_sp = compute_dynamic_sp(current_vpd);

        float error = dynamic_sp - current_moisture;
        const char *action = "idle";

        if (latch_dosing_event == 1 ||
            strcmp(status_mesin, "DOSING_AIR") == 0) {
          action = "watering";
          latch_dosing_event = 0;
        } else if (latch_nutrient_event == 1 ||
                   strcmp(status_mesin, "DISTRIBUSI_LAHAN") == 0) {
          action = "nutrient_dosing";
          latch_nutrient_event = 0;
        } else if (strcmp(status_mesin, "WAIT_ABSORPTION") == 0) {
          action = "within_deadband";
        } else if (error > DEADBAND_PCT) {
          action = "ready_to_water";
        } else {
          action = "within_deadband";
        }

        report_control_state(current_vpd, dynamic_sp, current_moisture, error,
                             action);
      }

      if (strstr(rxBuffer, "\"siram\":1") != NULL)
        flag_manual_air = 1;
      if (strstr(rxBuffer, "\"siram_nutrisi\":1") != NULL)
        flag_manual_nutrisi = 1;

      if (strstr(rxBuffer, "\"auto_mode\":1") != NULL) {
        is_auto_mode = 1;
        printf("\n[MODE] Beralih ke mode OTOMATIS\n");
      } else if (strstr(rxBuffer, "\"auto_mode\":0") != NULL) {
        is_auto_mode = 0;
        printf("\n[MODE] Beralih ke mode MANUAL\n");
      }

      memset(rxBuffer, 0, sizeof(rxBuffer));
      rxIndex = 0;
    }

    // ========================================================
    // 2. OUTBOUND (TX) - LAPOR STATUS MESIN & HEARTBEAT
    // ========================================================
    if ((osKernelSysTick() - last_tx_time) >= 5000) {
      uint32_t uptime_detik = osKernelSysTick() / 1000;
      uint32_t sisa_ram = xPortGetFreeHeapSize();

      sprintf(jsonTxBuffer,
              "{\"type\":\"heartbeat\",\"node\":\"SOGA-TANK-01\",\"status\":\"%"
              "s\",\"is_tank_empty\":%d,\"is_auto_mode\":%d,\"uptime\":%lu,"
              "\"free_heap\":%lu}\n",
              status_mesin, IsTankEmpty(), is_auto_mode, uptime_detik,
              sisa_ram);

      HAL_UART_Transmit(&huart1, (uint8_t *)jsonTxBuffer, strlen(jsonTxBuffer),
                        100);
      last_tx_time = osKernelSysTick();
    }

    osDelay(10);
  }
  /* USER CODE END StartTaskTelemetry */
}

/* USER CODE BEGIN Header_StartTaskIrrigation */
/**
 * @brief Function implementing the TaskIrrigation thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskIrrigation */
void StartTaskIrrigation(void const *argument) {
  /* USER CODE BEGIN StartTaskIrrigation */
  /* Infinite loop */
  POMPA_OFF();
  VALVE_OFF_ALL();

  static uint8_t last_minute_triggered = 60;

  // ========================================================
  // [NEW] VARIABEL STATE MACHINE (NON-BLOCKING)
  // ========================================================
  static uint8_t is_waiting_absorption = 0;  // Flag status lagi nunggu resapan
  static uint32_t time_start_absorption = 0; // Catatan waktu mulai nunggu

  for (;;) {
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // ========================================================
    // RULE 2: TRIGGER NUTRISI (Jalan Terus Tanpa Terblokir)
    // ========================================================
    if (((is_auto_mode && sTime.Minutes % 5 == 0) &&
         (sTime.Minutes != last_minute_triggered)) ||
        flag_manual_nutrisi) {

      last_minute_triggered = sTime.Minutes;

      strcpy(status_mesin, "DISTRIBUSI_LAHAN");
      latch_nutrient_event = 1; // Aktifkan latch agar telemetry mencatat
      printf(
          "\n[IRIGASI %02d:%02d:%02d] Waktunya Makan! Mulai Sekuens NUTRISI!\n",
          sTime.Hours, sTime.Minutes, sTime.Seconds);

      VALVE_ON(VALVE_AIR);
      VALVE_ON(VALVE_NUTRISI);
      printf("[IRIGASI] Nutrisi");
      osDelay(50); // Delay aktuasi 50ms sangat aman, tidak memblokir sistem
      VALVE_OFF(VALVE_AIR);
      VALVE_OFF(VALVE_NUTRISI);

      // LOGIKA ACK
      if (flag_manual_nutrisi == 1) {
        char ackBuffer[128];
        sprintf(ackBuffer,
                "{\"type\":\"ack\",\"node\":\"SOGA-TANK-01\",\"status\":"
                "\"DONE\",\"keterangan\":\"Siram Nutrisi Sukses\"}\n");
        HAL_UART_Transmit(&huart1, (uint8_t *)ackBuffer, strlen(ackBuffer),
                          100);
      }

      flag_manual_nutrisi = 0;

      // [PENTING] Kembalikan status mesin ke posisi semula
      if (is_waiting_absorption) {
        strcpy(status_mesin, "WAIT_ABSORPTION");
      } else {
        strcpy(status_mesin, "STANDBY");
      }
    }

    // ========================================================
    // RULE 1: TRIGGER AIR (NON-BLOCKING DWELL TIME)
    // ========================================================

    // CEK DULU: Apakah kita sedang dalam masa tunggu resapan air?
    if (is_waiting_absorption) {

      // Apakah DWELL_TIME (misal 15 menit) sudah terlewati?
      if ((osKernelSysTick() - time_start_absorption) >= DWELL_TIME_MS) {
        is_waiting_absorption = 0; // Lepas gemboknya
        strcpy(status_mesin, "STANDBY");
        mesin_sedang_menyiram = 0;
        printf(
            "\n[IRIGASI] Masa resapan selesai! Sensor siap membaca ulang.\n");
      }

    }
    // JIKA TIDAK SEDANG NUNGGU, evaluasi kelembapan tanah
    else {

      if ((is_auto_mode && current_moisture < (dynamic_sp - DEADBAND_PCT)) ||
          flag_manual_air) {
        strcpy(status_mesin, "DOSING_AIR");
        latch_dosing_event = 1;
        mesin_sedang_menyiram = 1;

        printf("\n[IRIGASI] Tanah Kering (%.1f%%). SP: %.1f%%. Mulai Sekuens "
               "AIR!\n",
               current_moisture, dynamic_sp);

        float error = dynamic_sp - current_moisture;
        if (error < 0)
          error = 0.0f;

        uint32_t durasi_siram_ms = (uint32_t)(error * 50.0f);
        if (durasi_siram_ms < 50)
          durasi_siram_ms = 50;
        if (durasi_siram_ms > 200)
          durasi_siram_ms = 200;

        VALVE_ON(VALVE_AIR);
        VALVE_ON(VALVE_NUTRISI);
        printf("[IRIGASI] Durasi time-proportional: %lu ms\n", durasi_siram_ms);
        osDelay(durasi_siram_ms); // Delay hitungan milidetik masih aman
        VALVE_OFF(VALVE_AIR);
        VALVE_OFF(VALVE_NUTRISI);

        if (flag_manual_air == 1) {
          char ackBuffer[128];
          sprintf(ackBuffer,
                  "{\"type\":\"ack\",\"node\":\"SOGA-TANK-01\",\"status\":"
                  "\"DONE\",\"keterangan\":\"Siram Air Sukses\"}\n");
          HAL_UART_Transmit(&huart1, (uint8_t *)ackBuffer, strlen(ackBuffer),
                            100);
        }

        flag_manual_air = 0;

        // =====================================================
        // INI PENGGANTI osDelay PANJANG YANG BIKIN HANG:
        // =====================================================
        is_waiting_absorption = 1;                 // Nyalakan penanda
        time_start_absorption = osKernelSysTick(); // Catat waktu mulainya
        strcpy(status_mesin, "WAIT_ABSORPTION");
      }
    }

    osDelay(1000); // 1 detik per tick (Loop bernapas, tidak memblokir)
  }
  /* USER CODE END StartTaskIrrigation */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM5 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM5) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
