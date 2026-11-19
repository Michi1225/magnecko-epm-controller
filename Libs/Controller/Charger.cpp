#include "Charger.h"
#include "app_runtime.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32h7xx_hal_crc.h"
#include <cstdint>
#include <cstring>

ChargerData __attribute__((section(CHARGER_SECTION_NAME))) Charger::status;
TransmitData __attribute__((section(CHARGER_SECTION_NAME))) Charger::tx_data;

// BDMA can only access D3 SRAM. Never use the DTCM task stack as its buffer.
alignas(32) static TransmitData wire_tx __attribute__((section(CHARGER_SECTION_NAME)));
alignas(32) static ChargerData wire_rx __attribute__((section(CHARGER_SECTION_NAME)));
static_assert(sizeof(TransmitData) == sizeof(ChargerData), "Charger frame sizes differ");


Charger::Charger()
{
    std::memset(&Charger::status, 0, sizeof(Charger::status));
    std::memset(&Charger::tx_data, 0, sizeof(Charger::tx_data));
    this->initialized = false;
    this->data_valid = false;
}


/**
 * @brief Wait for the charger to be ready.
 * @param timeout The timeout value in milliseconds.
 * @return true if the charger is ready within the timeout, false otherwise.
 */
bool Charger::wait_ready(uint16_t timeout)
{
    uint32_t start_time = HAL_GetTick();
    while(HAL_GetTick() - start_time < timeout)
    {
        this->transmit_receive_init();
        if(Charger::status.ready && this->data_valid) 
        {
            this->initialized = true;
            return true;
        }
        App_Delay(1);
    }
    return false;
}


/**
 * @brief Transmit data to the charger and receive its status. 
 */
void Charger::transmit_receive()
{
    if (!initialized) return;
    transmit_receive_init();
}

void Charger::transmit_receive_init()
{
    // The service task owns SPI6/CRC. Control owns desired command fields.
    taskENTER_CRITICAL();
    std::memcpy(&wire_tx, &tx_data, sizeof(wire_tx));
    taskEXIT_CRITICAL();
    wire_tx.crc = HAL_CRC_Calculate(&hcrc, reinterpret_cast<uint32_t*>(&wire_tx),
        sizeof(wire_tx) - sizeof(wire_tx.crc));
    data_valid = false;
    if (App_SpiTransfer(CHARGER_SPI_HANDLE, reinterpret_cast<uint8_t*>(&wire_tx),
            reinterpret_cast<uint8_t*>(&wire_rx), sizeof(wire_rx), 10, 1) == HAL_OK)
        rx_data_validate();
}

bool Charger::rx_data_validate()
{
    const auto crc = HAL_CRC_Calculate(&hcrc, reinterpret_cast<uint32_t*>(&wire_rx),
        sizeof(wire_rx) - sizeof(wire_rx.crc));
    data_valid = crc == wire_rx.crc;
    if (data_valid) std::memcpy(&status, &wire_rx, sizeof(status));
    return data_valid;
}
