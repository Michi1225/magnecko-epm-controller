#include "app_runtime.h"
#include "FreeRTOS.h"
#include "task.h"
#include "i2c.h"

namespace {
constexpr UBaseType_t io_notification = 1;
struct Transfer {
    TaskHandle_t owner = nullptr;
    volatile HAL_StatusTypeDef status = HAL_BUSY;
};
Transfer spi_transfers[5]; /* SPI1, SPI2, SPI3, SPI4, SPI6 */
Transfer i2c_transfers[3]; /* I2C1, I2C2, I2C4 */

void record_error() {
    taskENTER_CRITICAL();
    ++app_diagnostics.io_errors;
    taskEXIT_CRITICAL();
}

Transfer *slot(SPI_HandleTypeDef *spi) {
    if (spi == &hspi1) return &spi_transfers[0];
    if (spi == &hspi2) return &spi_transfers[1];
    if (spi == &hspi3) return &spi_transfers[2];
    if (spi == &hspi4) return &spi_transfers[3];
    if (spi == &hspi6) return &spi_transfers[4];
    return nullptr;
}

Transfer *slot(I2C_HandleTypeDef *i2c) {
    if (i2c == &hi2c1) return &i2c_transfers[0];
    if (i2c == &hi2c2) return &i2c_transfers[1];
    if (i2c == &hi2c4) return &i2c_transfers[2];
    return nullptr;
}

bool begin(Transfer &transfer) {
    configASSERT(__get_IPSR() == 0);
    taskENTER_CRITICAL();
    if (transfer.owner != nullptr) {
        taskEXIT_CRITICAL();
        return false;
    }
    (void)ulTaskNotifyTakeIndexed(io_notification, pdTRUE, 0);
    transfer.status = HAL_BUSY;
    transfer.owner = xTaskGetCurrentTaskHandle();
    taskEXIT_CRITICAL();
    return true;
}

void finish(Transfer &transfer) {
    taskENTER_CRITICAL();
    transfer.owner = nullptr;
    (void)ulTaskNotifyTakeIndexed(io_notification, pdTRUE, 0);
    taskEXIT_CRITICAL();
}

void complete(Transfer &transfer, HAL_StatusTypeDef status) {
    if (transfer.owner == nullptr) return;
    BaseType_t wake = pdFALSE;
    transfer.status = status;
    vTaskNotifyGiveIndexedFromISR(transfer.owner, io_notification, &wake);
    portYIELD_FROM_ISR(wake);
}

HAL_StatusTypeDef wait(Transfer &transfer, HAL_StatusTypeDef started, uint32_t timeout_ms) {
    if (started != HAL_OK) return started;
    if (ulTaskNotifyTakeIndexed(io_notification, pdTRUE,
            pdMS_TO_TICKS(timeout_ms) + 1U) == 0) return HAL_TIMEOUT;
    return transfer.status;
}
}

extern "C" HAL_StatusTypeDef App_SpiTransfer(SPI_HandleTypeDef *spi,
    const uint8_t *tx, uint8_t *rx, uint16_t length, uint32_t timeout_ms, int use_dma) {
    Transfer *transfer = slot(spi);
    configASSERT(transfer && length && (tx || rx));
    if (!begin(*transfer)) return HAL_BUSY;
    HAL_StatusTypeDef started;
    if (tx && rx) {
        started = use_dma ? HAL_SPI_TransmitReceive_DMA(spi, tx, rx, length)
                          : HAL_SPI_TransmitReceive_IT(spi, tx, rx, length);
    } else if (tx) {
        started = use_dma ? HAL_SPI_Transmit_DMA(spi, tx, length)
                          : HAL_SPI_Transmit_IT(spi, tx, length);
    } else {
        started = use_dma ? HAL_SPI_Receive_DMA(spi, rx, length)
                          : HAL_SPI_Receive_IT(spi, rx, length);
    }
    HAL_StatusTypeDef result = wait(*transfer, started, timeout_ms);
    if (result != HAL_OK) {
        /* Quiesce DMA/IRQs before a caller may reuse or release its buffers. */
        if (HAL_SPI_Abort(spi) != HAL_OK) App_Fatal();
        record_error();
    }
    finish(*transfer);
    return result;
}

extern "C" HAL_StatusTypeDef App_HallRead(uint16_t address, uint8_t reg,
    uint8_t *rx, uint16_t length, uint32_t timeout_ms) {
    return App_I2cTransfer(&hi2c2, address, reg, I2C_MEMADD_SIZE_8BIT,
        nullptr, rx, length, timeout_ms, 1);
}

extern "C" HAL_StatusTypeDef App_I2cTransfer(I2C_HandleTypeDef *i2c,
    uint16_t address, uint16_t reg, uint16_t reg_size, const uint8_t *tx,
    uint8_t *rx, uint16_t length, uint32_t timeout_ms, int use_dma) {
    Transfer *transfer = slot(i2c);
    configASSERT(transfer && length && ((tx != nullptr) != (rx != nullptr)));
    configASSERT(!use_dma || (i2c == &hi2c2 && reg_size && rx));
    if (!begin(*transfer)) return HAL_BUSY;
    HAL_StatusTypeDef started;
    // HAL's I2C TX signature is non-const; the driver only reads these bytes.
    auto *tx_bytes = const_cast<uint8_t*>(tx);
    if (!reg_size) {
        started = tx ? HAL_I2C_Master_Transmit_IT(i2c, address, tx_bytes, length)
                     : HAL_I2C_Master_Receive_IT(i2c, address, rx, length);
    } else if (tx) {
        started = HAL_I2C_Mem_Write_IT(i2c, address, reg, reg_size, tx_bytes, length);
    } else {
        started = use_dma ? HAL_I2C_Mem_Read_DMA(i2c, address, reg, reg_size, rx, length)
                          : HAL_I2C_Mem_Read_IT(i2c, address, reg, reg_size, rx, length);
    }
    const auto result = wait(*transfer, started, timeout_ms);
    if (result != HAL_OK) {
        // Fault cleanup must quiesce hardware before returning a caller's buffer.
        __HAL_I2C_DISABLE(i2c);
        if (use_dma && i2c->hdmarx && HAL_DMA_Abort(i2c->hdmarx) == HAL_TIMEOUT) App_Fatal();
        if (use_dma && i2c->hdmatx && HAL_DMA_Abort(i2c->hdmatx) == HAL_TIMEOUT) App_Fatal();
        if (HAL_I2C_DeInit(i2c) != HAL_OK) App_Fatal();
        if (i2c == &hi2c1) MX_I2C1_Init();
        else if (i2c == &hi2c2) MX_I2C2_Init();
        else MX_I2C4_Init(); // Restore timing, filters, IRQs and DMA links.
        record_error();
    }
    finish(*transfer);
    return result;
}

extern "C" void App_SpiCompleteISR(SPI_HandleTypeDef *spi, HAL_StatusTypeDef status) {
    if (Transfer *transfer = slot(spi)) complete(*transfer, status);
}

extern "C" void App_I2cCompleteISR(I2C_HandleTypeDef *i2c, HAL_StatusTypeDef status) {
    if (Transfer *transfer = slot(i2c)) complete(*transfer, status);
}
