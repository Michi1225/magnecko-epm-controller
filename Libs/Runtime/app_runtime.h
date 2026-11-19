#pragma once

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* All timeouts are milliseconds. I/O has exactly one task owner per bus.
   Transfer helpers suspend only the caller; they do not hold a critical section
   while waiting. Buffers stay live until completion/abort before returning. */
void App_Start(void);
void App_Delay(uint32_t milliseconds);
int App_IsReady(void);
void App_RequestLed(uint32_t animation);
void App_ControlTickISR(void);
void App_ImuReadyISR(void);
void App_TofReadyISR(void);
void App_ButtonHeldISR(void);
void App_DeadTimeISR(void);
void App_SpiCompleteISR(SPI_HandleTypeDef *spi, HAL_StatusTypeDef status);
void App_I2cCompleteISR(I2C_HandleTypeDef *i2c, HAL_StatusTypeDef status);
HAL_StatusTypeDef App_SpiTransfer(SPI_HandleTypeDef *spi, const uint8_t *tx,
    uint8_t *rx, uint16_t length, uint32_t timeout_ms, int use_dma);
HAL_StatusTypeDef App_HallRead(uint16_t address, uint8_t reg, uint8_t *rx,
    uint16_t length, uint32_t timeout_ms);
/* reg_size=0 selects a raw master transfer; otherwise an HAL memory transfer.
   Exactly one of tx/rx must be nonnull. DMA is used only for Hall memory reads. */
HAL_StatusTypeDef App_I2cTransfer(I2C_HandleTypeDef *i2c, uint16_t address,
    uint16_t reg, uint16_t reg_size, const uint8_t *tx, uint8_t *rx,
    uint16_t length, uint32_t timeout_ms, int use_dma);
/* Called by the independent HAL timer: does not call the RTOS. */
void App_CheckControlHeartbeat(void);
void App_Fatal(void);

typedef struct {
    uint32_t control_releases;
    uint32_t missed_control_releases;
    uint32_t control_cycles;
    uint32_t max_control_cycles;
    uint32_t max_wake_latency_cycles;
    uint32_t control_heartbeat_ms;
    uint32_t io_errors;
    uint32_t sensor_stale_mask; /* Hall=1, LDC=2, IMU=4, ToF=8, charger=16 */
    uint32_t stack_free_words[5]; /* control, acquisition, IMU, ToF, services */
} AppDiagnostics;
extern volatile AppDiagnostics app_diagnostics;

#ifdef __cplusplus
}
#endif
