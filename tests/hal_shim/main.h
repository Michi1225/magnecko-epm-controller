#pragma once
#include <stdint.h>
#include <stddef.h>
#define __section(name)
#define __aligned(n) __attribute__((aligned(n)))
#define __NOP() ((void)0)
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { int unused; } DMA_HandleTypeDef;
typedef struct { int unused; } SPI_HandleTypeDef;
typedef struct { DMA_HandleTypeDef *hdmarx, *hdmatx; } I2C_HandleTypeDef;
typedef struct { int unused; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
extern SPI_HandleTypeDef hspi1, hspi2, hspi3, hspi4, hspi6;
extern I2C_HandleTypeDef hi2c1, hi2c2, hi2c4;
extern GPIO_TypeDef test_gpio;
#define IMU_NCS_GPIO_Port (&test_gpio)
#define IMU_NRST_GPIO_Port (&test_gpio)
#define IMU_INT_GPIO_Port (&test_gpio)
#define IMU_WAKE_GPIO_Port (&test_gpio)
#define IMU_BOOTN_GPIO_Port (&test_gpio)
#define IMU_NCS_Pin 1
#define IMU_NRST_Pin 2
#define IMU_INT_Pin 4
#define IMU_WAKE_Pin 8
#define IMU_BOOTN_Pin 16
#define I2C_MEMADD_SIZE_8BIT 1
#define I2C_MEMADD_SIZE_16BIT 2
#define __HAL_I2C_DISABLE(h) ((void)(h))
#ifdef __cplusplus
extern "C" {
#endif
uint32_t __get_IPSR(void);
uint32_t HAL_GetTick(void);
void Error_Handler(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *, uint16_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef *, const uint8_t *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *, const uint8_t *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT(SPI_HandleTypeDef *, const uint8_t *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *, const uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef *, const uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_SPI_Receive_DMA(SPI_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *);
HAL_StatusTypeDef HAL_I2C_Mem_Read_DMA(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *);
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *);
void MX_I2C2_Init(void);
void MX_I2C1_Init(void);
void MX_I2C4_Init(void);
#ifdef __cplusplus
}
#endif
