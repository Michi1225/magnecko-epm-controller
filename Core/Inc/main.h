/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "utypes.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern _Objects Obj;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
extern SPI_HandleTypeDef hspi4;
extern SPI_HandleTypeDef hspi6;

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c4;

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim6;

extern ADC_HandleTypeDef hadc1;

extern CRC_HandleTypeDef hcrc;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ECAT_MOSI_Pin GPIO_PIN_6
#define ECAT_MOSI_GPIO_Port GPIOE
#define ECAT_MISO_Pin GPIO_PIN_5
#define ECAT_MISO_GPIO_Port GPIOE
#define ECAT_SCK_Pin GPIO_PIN_2
#define ECAT_SCK_GPIO_Port GPIOE
#define EEPROM_SCL_Pin GPIO_PIN_8
#define EEPROM_SCL_GPIO_Port GPIOB
#define CHARGER_MOSI_Pin GPIO_PIN_5
#define CHARGER_MOSI_GPIO_Port GPIOB
#define IMU_MOSI_Pin GPIO_PIN_6
#define IMU_MOSI_GPIO_Port GPIOD
#define IMU_NRST_Pin GPIO_PIN_3
#define IMU_NRST_GPIO_Port GPIOD
#define IMU_BOOTN_Pin GPIO_PIN_2
#define IMU_BOOTN_GPIO_Port GPIOD
#define CHARGER_SCK_Pin GPIO_PIN_12
#define CHARGER_SCK_GPIO_Port GPIOC
#define EEPROM_LOADED_Pin GPIO_PIN_14
#define EEPROM_LOADED_GPIO_Port GPIOC
#define ECAT_IRQ_Pin GPIO_PIN_15
#define ECAT_IRQ_GPIO_Port GPIOC
#define SYNC0_Pin GPIO_PIN_3
#define SYNC0_GPIO_Port GPIOE
#define SYNC1_Pin GPIO_PIN_0
#define SYNC1_GPIO_Port GPIOE
#define EEPROM_SDA_Pin GPIO_PIN_7
#define EEPROM_SDA_GPIO_Port GPIOB
#define IMU_WAKE_Pin GPIO_PIN_4
#define IMU_WAKE_GPIO_Port GPIOD
#define IMU_INT_Pin GPIO_PIN_1
#define IMU_INT_GPIO_Port GPIOD
#define IMU_INT_EXTI_IRQn EXTI1_IRQn
#define IMU_MISO_Pin GPIO_PIN_11
#define IMU_MISO_GPIO_Port GPIOC
#define IMU_SCK_Pin GPIO_PIN_10
#define IMU_SCK_GPIO_Port GPIOC
#define ECAT_NCS_Pin GPIO_PIN_4
#define ECAT_NCS_GPIO_Port GPIOE
#define nRST_ECAT_Pin GPIO_PIN_1
#define nRST_ECAT_GPIO_Port GPIOE
#define CHARGER_MISO_Pin GPIO_PIN_4
#define CHARGER_MISO_GPIO_Port GPIOB
#define IMU_NCS_Pin GPIO_PIN_15
#define IMU_NCS_GPIO_Port GPIOA
#define LED_SCL_Pin GPIO_PIN_6
#define LED_SCL_GPIO_Port GPIOB
#define LED_SDA_Pin GPIO_PIN_9
#define LED_SDA_GPIO_Port GPIOB
#define DISCHARGE_Pin GPIO_PIN_10
#define DISCHARGE_GPIO_Port GPIOA
#define DRV2_Pin GPIO_PIN_9
#define DRV2_GPIO_Port GPIOC
#define DRV1_Pin GPIO_PIN_8
#define DRV1_GPIO_Port GPIOA
#define CHARGER_NCS_Pin GPIO_PIN_0
#define CHARGER_NCS_GPIO_Port GPIOA
#define GD_nFLT_Pin GPIO_PIN_7
#define GD_nFLT_GPIO_Port GPIOC
#define GD_nEN_Pin GPIO_PIN_8
#define GD_nEN_GPIO_Port GPIOC
#define LDC3_NCS_Pin GPIO_PIN_13
#define LDC3_NCS_GPIO_Port GPIOD
#define TOF_MISO_Pin GPIO_PIN_6
#define TOF_MISO_GPIO_Port GPIOA
#define TOF_INT_Pin GPIO_PIN_5
#define TOF_INT_GPIO_Port GPIOC
#define TOF_INT_EXTI_IRQn EXTI9_5_IRQn
#define BUTTON_Pin GPIO_PIN_2
#define BUTTON_GPIO_Port GPIOB
#define BUTTON_EXTI_IRQn EXTI2_IRQn
#define HALL_SDA_Pin GPIO_PIN_11
#define HALL_SDA_GPIO_Port GPIOB
#define LDC_SCK_Pin GPIO_PIN_13
#define LDC_SCK_GPIO_Port GPIOB
#define LDC1_NCS_Pin GPIO_PIN_8
#define LDC1_NCS_GPIO_Port GPIOD
#define LDC2_NCS_Pin GPIO_PIN_10
#define LDC2_NCS_GPIO_Port GPIOD
#define TOF_NCS_Pin GPIO_PIN_4
#define TOF_NCS_GPIO_Port GPIOA
#define TOF_SCK_Pin GPIO_PIN_5
#define TOF_SCK_GPIO_Port GPIOA
#define TOF_MOSI_Pin GPIO_PIN_7
#define TOF_MOSI_GPIO_Port GPIOA
#define MAG_STAT_Pin GPIO_PIN_0
#define MAG_STAT_GPIO_Port GPIOB
#define HALL_SCL_Pin GPIO_PIN_10
#define HALL_SCL_GPIO_Port GPIOB
#define LDC0_NCS_Pin GPIO_PIN_12
#define LDC0_NCS_GPIO_Port GPIOB
#define LDC_MISO_Pin GPIO_PIN_14
#define LDC_MISO_GPIO_Port GPIOB
#define LDC_MOSI_Pin GPIO_PIN_15
#define LDC_MOSI_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define TIM_CONTROL (&htim2)
#define TIM_IMU (&htim4)
#define TIM_BUTTON (&htim5)
#define TIM_DRV1 (&htim1)
#define CHANNEL_DRV1 TIM_CHANNEL_1
#define TIM_DRV2 (&htim8)
#define CHANNEL_DRV2 TIM_CHANNEL_4
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
