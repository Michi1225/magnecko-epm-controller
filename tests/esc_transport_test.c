#include "esc.h"
#include "soes_pin_mapping_def.h"
#include "app_runtime.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

_ESCvar ESCvar;
SPI_HandleTypeDef hspi4;
GPIO_TypeDef test_gpio;
static size_t length, position, transactions;
static bool selected, address_phase, writing;
static uint8_t pattern(size_t i) { return (uint8_t)(i * 7); }

uint32_t HAL_GetTick(void) { return 0; }
int App_IsReady(void) { return 1; }
void App_Delay(uint32_t ms) { (void)ms; }
void Error_Handler(void) { abort(); }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin) {
    (void)p; (void)pin; return GPIO_PIN_SET;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState state) {
    (void)p; (void)pin;
    if (state == GPIO_PIN_RESET) {
        assert(!selected); selected = address_phase = true; ++transactions;
    } else {
        assert(selected && position == length); selected = false;
    }
}
HAL_StatusTypeDef App_SpiTransfer(SPI_HandleTypeDef *spi, const uint8_t *tx,
    uint8_t *rx, uint16_t size, uint32_t timeout, int use_dma) {
    assert(spi == &hspi4 && selected && timeout == 2 && !use_dma);
    if (address_phase) {
        assert(size == 2 && tx[0] == (0x1000 >> 5));
        assert((tx[1] & 7) == (writing ? 4 : 2));
        rx[0] = 0xAB; rx[1] = 0x12; address_phase = false;
        return HAL_OK;
    }
    assert(size <= 128 && position + size <= length);
    for (unsigned i = 0; i < size; ++i) {
        if (writing) assert(tx[i] == pattern(position));
        else assert(tx[i] == (position == length - 1 ? 0xFF : 0));
        rx[i] = pattern(position++);
    }
    return HAL_OK;
}

int main(void) {
    pin_mapping_typedef pins = {&hspi4, &test_gpio, 1, &test_gpio, 2};
    esc_cfg_t config = {0}; config.user_arg = &pins;
    ESC_init(&config);
    uint8_t buffer[513];
    const unsigned sizes[] = {0, 1, 127, 128, 129, 264, 512, 513};
    for (unsigned n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
        length = sizes[n]; position = 0; writing = false;
        const size_t before = transactions;
        memset(buffer, 0, sizeof(buffer));
        ESC_read(0x1000, buffer, length);
        assert(!selected && position == length);
        assert(transactions == before + (length != 0));
        for (size_t i = 0; i < length; ++i) assert(buffer[i] == pattern(i));
        if (length) assert(ESCvar.ALevent == 0x12AB);
        writing = true; position = 0;
        ESC_write(0x1000, buffer, length);
        assert(!selected && position == length);
    }
}
