#include "app_runtime.h"
#include "task.h"
#include <cassert>
#include <cstdlib>

SPI_HandleTypeDef hspi1{}, hspi2{}, hspi3{}, hspi4{}, hspi6{};
I2C_HandleTypeDef hi2c1{}, hi2c2{}, hi2c4{};
unsigned mock_critical_depth = 0;
volatile AppDiagnostics app_diagnostics{};
static unsigned notifications = 0, aborts = 0, i2c_resets = 0;
static HAL_StatusTypeDef started = HAL_OK, completed = HAL_OK;
static bool complete_early = true, test_contention = false;
static bool complete_on_wait = false;
static unsigned other_task_runs = 0;
static SPI_HandleTypeDef *pending_spi = nullptr;
static I2C_HandleTypeDef *pending_i2c = nullptr;
static uint8_t buffer[8]{};

TaskHandle_t xTaskGetCurrentTaskHandle() { return reinterpret_cast<void *>(1); }
uint32_t ulTaskNotifyTakeIndexed(UBaseType_t index, BaseType_t, TickType_t ticks) {
    assert(index == 1);
    if (ticks) {
        // The production wait must leave its critical section before blocking.
        assert(mock_critical_depth == 0);
        if (!notifications && complete_on_wait) {
            ++other_task_runs;
            if (pending_spi) App_SpiCompleteISR(pending_spi, completed);
            if (pending_i2c) App_I2cCompleteISR(pending_i2c, completed);
            pending_spi = nullptr; pending_i2c = nullptr;
        }
    }
    auto value = notifications; notifications = 0; return value;
}
void vTaskNotifyGiveIndexedFromISR(TaskHandle_t owner, UBaseType_t index, BaseType_t *) {
    assert(owner == xTaskGetCurrentTaskHandle() && index == 1); ++notifications;
}
extern "C" uint32_t __get_IPSR() { return 0; }
extern "C" void App_Fatal() { std::abort(); }

static HAL_StatusTypeDef transfer(SPI_HandleTypeDef *spi) {
    assert(mock_critical_depth == 0);
    if (test_contention) {
        test_contention = false;
        assert(App_SpiTransfer(spi, buffer, buffer, sizeof(buffer), 5, 0) == HAL_BUSY);
    }
    if (started == HAL_OK && complete_early) App_SpiCompleteISR(spi, completed);
    else if (started == HAL_OK) pending_spi = spi;
    return started;
}
extern "C" HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef *s, const uint8_t *, uint8_t *, uint16_t) { return transfer(s); }
extern "C" HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT(SPI_HandleTypeDef *s, const uint8_t *, uint8_t *, uint16_t) { return transfer(s); }
extern "C" HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *s, const uint8_t *, uint16_t) { return transfer(s); }
extern "C" HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef *s, const uint8_t *, uint16_t) { return transfer(s); }
extern "C" HAL_StatusTypeDef HAL_SPI_Receive_DMA(SPI_HandleTypeDef *s, uint8_t *, uint16_t) { return transfer(s); }
extern "C" HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef *s, uint8_t *, uint16_t) { return transfer(s); }
extern "C" HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *) { ++aborts; pending_spi = nullptr; return HAL_OK; }
static HAL_StatusTypeDef transfer(I2C_HandleTypeDef *i) {
    assert(mock_critical_depth == 0);
    if (complete_early && started == HAL_OK) App_I2cCompleteISR(i, completed);
    else if (started == HAL_OK) pending_i2c = i;
    return started;
}
extern "C" HAL_StatusTypeDef HAL_I2C_Mem_Read_DMA(I2C_HandleTypeDef *i, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t) { return transfer(i); }
extern "C" HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *i, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t) { return transfer(i); }
extern "C" HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *i, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t) { return transfer(i); }
extern "C" HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *i, uint16_t, uint8_t *, uint16_t) { return transfer(i); }
extern "C" HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef *i, uint16_t, uint8_t *, uint16_t) { return transfer(i); }
extern "C" HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *) { pending_i2c = nullptr; return HAL_OK; }
extern "C" HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *) { return HAL_OK; }
extern "C" void MX_I2C2_Init() { ++i2c_resets; }
extern "C" void MX_I2C1_Init() { ++i2c_resets; }
extern "C" void MX_I2C4_Init() { ++i2c_resets; }

int main() {
    // Completion can arrive before the task starts waiting; it must not be lost.
    test_contention = true;
    assert(App_SpiTransfer(&hspi1, buffer, buffer, 8, 5, 0) == HAL_OK);
    assert(App_SpiTransfer(&hspi6, buffer, buffer, 8, 5, 1) == HAL_OK);
    // Deferred EtherCAT and I2C completions arrive after simulated unrelated
    // task work. The wait is outside every critical section, on I/O index 1.
    complete_early = false; complete_on_wait = true;
    assert(App_SpiTransfer(&hspi4, buffer, buffer, 8, 2, 0) == HAL_OK);
    assert(App_I2cTransfer(&hi2c1, 0x5A, 1, 1, buffer, nullptr, 8, 10, 0) == HAL_OK);
    assert(App_I2cTransfer(&hi2c4, 0xA0, 1, 2, nullptr, buffer, 8, 10, 0) == HAL_OK);
    assert(App_I2cTransfer(&hi2c2, 0x44, 0, 0, buffer, nullptr, 8, 10, 0) == HAL_OK);
    assert(App_I2cTransfer(&hi2c2, 0x44, 0, 0, nullptr, buffer, 8, 10, 0) == HAL_OK);
    assert(other_task_runs == 5);
    complete_on_wait = false;
    complete_early = false;
    assert(App_SpiTransfer(&hspi1, buffer, buffer, 8, 5, 0) == HAL_TIMEOUT);
    assert(aborts == 1);
    // Late completions after an abort have no owner and cannot poison a new wait.
    App_SpiCompleteISR(&hspi1, HAL_OK);
    assert(notifications == 0);
    complete_early = true;
    assert(App_SpiTransfer(&hspi1, nullptr, buffer, 8, 5, 0) == HAL_OK);
    completed = HAL_ERROR;
    assert(App_SpiTransfer(&hspi1, buffer, nullptr, 8, 5, 0) == HAL_ERROR);
    completed = HAL_OK; started = HAL_BUSY;
    assert(App_SpiTransfer(&hspi1, buffer, buffer, 8, 5, 0) == HAL_BUSY);
    started = HAL_OK;
    assert(App_SpiTransfer(&hspi1, buffer, buffer, 8, 5, 0) == HAL_OK);

    assert(App_HallRead(0x44, 0x12, buffer, 6, 5) == HAL_OK);
    complete_early = false;
    assert(App_HallRead(0x44, 0x12, buffer, 6, 5) == HAL_TIMEOUT);
    assert(i2c_resets == 1);
    App_I2cCompleteISR(&hi2c2, HAL_OK);
    assert(notifications == 0);
    complete_early = true;
    assert(App_HallRead(0x44, 0x12, buffer, 6, 5) == HAL_OK);
    complete_early = false;
    assert(App_I2cTransfer(&hi2c4, 0xA0, 1, 2, buffer, nullptr, 8, 10, 0) == HAL_TIMEOUT);
    assert(i2c_resets == 2);
    App_I2cCompleteISR(&hi2c4, HAL_OK);
    assert(notifications == 0);
    complete_early = true;
    assert(App_I2cTransfer(&hi2c4, 0xA0, 1, 2, buffer, nullptr, 8, 10, 0) == HAL_OK);
    assert(app_diagnostics.io_errors == 5);
    assert(mock_critical_depth == 0);
}
