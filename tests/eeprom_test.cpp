#include "eeprom.h"
#include "app_runtime.h"
#include <cassert>
#include <vector>

I2C_HandleTypeDef hi2c4{};
static std::vector<uint16_t> addresses, lengths;
static unsigned delays = 0;
static bool writing;
extern "C" void App_Delay(uint32_t milliseconds) { assert(milliseconds == 4); ++delays; }
extern "C" HAL_StatusTypeDef App_I2cTransfer(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t width, const uint8_t *tx, uint8_t *rx, uint16_t size,
    uint32_t timeout, int dma) {
    assert(bus == &hi2c4 && address == (0x50 << 1) && width == I2C_MEMADD_SIZE_16BIT);
    assert(size > 0 && size <= 32 && timeout == 10 && !dma);
    assert((writing && tx && !rx) || (!writing && !tx && rx));
    addresses.push_back(reg); lengths.push_back(size);
    return HAL_OK;
}
int main() {
    uint8_t buffer[64]{};
    for (bool write : {false, true}) {
        writing = write; addresses.clear(); lengths.clear(); delays = 0;
        if (write) EEPROM_Write(2, 0, buffer, 64);
        else EEPROM_Read(2, 0, buffer, 64);
        assert((addresses == std::vector<uint16_t>{64, 96}));
        assert((lengths == std::vector<uint16_t>{32, 32}));
        assert(delays == (write ? 2U : 0U));
        addresses.clear(); lengths.clear();
        if (write) EEPROM_Write(1, 31, buffer, 3);
        else EEPROM_Read(1, 31, buffer, 3);
        assert((addresses == std::vector<uint16_t>{63, 64}));
        assert((lengths == std::vector<uint16_t>{1, 2}));
        addresses.clear();
        if (write) EEPROM_Write(0, 0, buffer, 0);
        else EEPROM_Read(0, 0, buffer, 0);
        assert(addresses.empty());
    }
}
