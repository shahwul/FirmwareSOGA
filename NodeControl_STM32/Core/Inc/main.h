/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define US2_TRIG_Pin GPIO_PIN_13
#define US2_TRIG_GPIO_Port GPIOC
#define US1_ECHO_Pin GPIO_PIN_0
#define US1_ECHO_GPIO_Port GPIOA
#define TFT_LED_Pin GPIO_PIN_1
#define TFT_LED_GPIO_Port GPIOA
#define TFT_CS_Pin GPIO_PIN_4
#define TFT_CS_GPIO_Port GPIOA
#define TFT_DC_Pin GPIO_PIN_0
#define TFT_DC_GPIO_Port GPIOB
#define TFT_RST_Pin GPIO_PIN_1
#define TFT_RST_GPIO_Port GPIOB
#define US1_TRIG_Pin GPIO_PIN_2
#define US1_TRIG_GPIO_Port GPIOB
#define FLOW2_Pin GPIO_PIN_10
#define FLOW2_GPIO_Port GPIOB
#define FLOW2_EXTI_IRQn EXTI15_10_IRQn
#define RLY1_Pin GPIO_PIN_12
#define RLY1_GPIO_Port GPIOB
#define RLY2_Pin GPIO_PIN_13
#define RLY2_GPIO_Port GPIOB
#define RLY3_Pin GPIO_PIN_14
#define RLY3_GPIO_Port GPIOB
#define RLY4_Pin GPIO_PIN_15
#define RLY4_GPIO_Port GPIOB
#define RLY5_Pin GPIO_PIN_8
#define RLY5_GPIO_Port GPIOA
#define US2_ECHO_Pin GPIO_PIN_9
#define US2_ECHO_GPIO_Port GPIOA
#define US3_ECHO_Pin GPIO_PIN_10
#define US3_ECHO_GPIO_Port GPIOA
#define US3_TRIG_Pin GPIO_PIN_15
#define US3_TRIG_GPIO_Port GPIOA
#define T_CS_Pin GPIO_PIN_3
#define T_CS_GPIO_Port GPIOB
#define T_IRQ_Pin GPIO_PIN_4
#define T_IRQ_GPIO_Port GPIOB
#define RLY6_Pin GPIO_PIN_5
#define RLY6_GPIO_Port GPIOB
#define LED_R_Pin GPIO_PIN_6
#define LED_R_GPIO_Port GPIOB
#define LED_G_Pin GPIO_PIN_7
#define LED_G_GPIO_Port GPIOB
#define LED_B_Pin GPIO_PIN_8
#define LED_B_GPIO_Port GPIOB
#define FLOW1_Pin GPIO_PIN_9
#define FLOW1_GPIO_Port GPIOB
#define FLOW1_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
