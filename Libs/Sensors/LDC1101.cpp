#include "LDC1101.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_spi.h"

LDC1101::LDC1101(GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
    this->cs_port = cs_port;
    this->cs_pin = cs_pin;
    this->initialized = false;
    this->data_valid = false;
    this->next = nullptr;
}

HAL_StatusTypeDef LDC1101::init() 
{
    int status = HAL_OK;

    struct {
        uint8_t addr;
        uint8_t data;
    } config_data;
    config_data.data = 0x00;

    // Waiting for the sensor to be ready
    uint8_t ready[2] = {0, 0};
    uint8_t attempts = 0;
    config_data.addr = LDC1101_ADDR_STATUS | 0x80; // Read command
    do {
        HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
        HAL_SPI_TransmitReceive(LDC_SPI_HANDLE, (uint8_t *)&config_data, ready,
                                sizeof(config_data), 10);
        HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
        attempts++;
        if(attempts > 100) 
        {
            this->initialized = false;
            return HAL_TIMEOUT;
        }
    } while (ready[1] & 0b1);

    // Rp_Set
    config_data.addr = LDC1101_ADDR_RP_SET;
    config_data.data |= (0b111);      // RP_MIN = 0.75kOhm
    config_data.data |= (0b000 << 4); // RP_MAX = 96kOhm

    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    status |= HAL_SPI_Transmit(LDC_SPI_HANDLE, (uint8_t *)&config_data, sizeof(config_data), 10);
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
    config_data.data = 0x00; // Default value

    // TC1
    config_data.addr = LDC1101_ADDR_TC1;
    config_data.data |= (0b11110 << 0); // R1 = 30 -> R1 = 33.9kOhm
    config_data.data |= (0b11 << 6);    // C1 = 6pF
    

    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    status |= HAL_SPI_Transmit(LDC_SPI_HANDLE, (uint8_t *)&config_data, sizeof(config_data), 10);
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
    config_data.data = 0x00; // Default value

    // TC2
    config_data.addr = LDC1101_ADDR_TC2;
    config_data.data |= (0b111101 << 0); // R2 = 61 -> R2 = 56kOhm
    config_data.data |= (0b11 << 6);     // C2 = 24pF

    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    status |= HAL_SPI_Transmit(LDC_SPI_HANDLE, (uint8_t *)&config_data, sizeof(config_data), 10);
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
    config_data.data = 0x00; // Default value

    // DIG_CONF
    config_data.addr = LDC1101_ADDR_DIG_CONFIG;
    config_data.data |= (0b111 << 0);   // RESP_TIME = 7 -> 6144 Cycles
    config_data.data |= (0b11100 << 4); // MIN_FREQ  = 12 -> 2MHz

    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    status |= HAL_SPI_Transmit(LDC_SPI_HANDLE, (uint8_t *)&config_data, sizeof(config_data), 10);
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
    config_data.data = 0x00; // Default value

    // ALT_CONFIG
    config_data.addr = LDC1101_ADDR_ALT_CONFIG;
    config_data.data |= (0b0 << 0); // LOPTIMAL Disabled
    config_data.data |= (0b0 << 1); // SHUTDOWN_EN Disabled

    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    status |= HAL_SPI_Transmit(LDC_SPI_HANDLE, (uint8_t *)&config_data, sizeof(config_data), 10);
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
    config_data.data = 0x00; // Default value

    // RP_THRESH_HI_LSB
    // SKIP

    // RP_THRESH_HI_MSB
    // SKIP

    // RP_THRESH_LO_LSB
    // SKIP

    // RP_THRESH_LO_MSB
    // SKIP

    // INTB_MODE
    // SKIP Use default values

    // START_CONFIG
    // SKIP Use default values

    // D_CONFIG
    config_data.addr = LDC1101_ADDR_D_CONF;
    config_data.data |= (0b1 << 0); // DOK_REPORT Disabled

    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    status |= HAL_SPI_Transmit(LDC_SPI_HANDLE, (uint8_t *)&config_data, sizeof(config_data), 10);
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
    config_data.data = 0x00; // Default value

    // L_THRESH_HI_LSB
    // SKIP

    // L_THRESH_HI_MSB
    // SKIP

    // L_THRESH_LO_LSB
    // SKIP

    // L_THRESH_LO_MSB
    // SKIP

    // Skipping LHR registers since they are not used in this application

    if(status == HAL_OK) this->initialized = true;

    return (HAL_StatusTypeDef)status;
}

void LDC1101::start_measurement() 
{
    if(!this->initialized) return; // Don't start measurement if sensor is not initialized

    struct {
        uint8_t addr;
        uint8_t data;
    } config_data;

    config_data.addr = LDC1101_ADDR_START_CONFIG;
    config_data.data = (0b0 << 0); // START

    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(LDC_SPI_HANDLE, (uint8_t*)&config_data, sizeof(config_data), 10);
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_SET);
}

void LDC1101::read_data()
{
    if(!this->initialized) return; // Don't read data if sensor is not initialized


    if(HAL_SPI_GetState(LDC_SPI_HANDLE) != HAL_SPI_STATE_READY) return;

    struct __attribute__((packed))
    {
        uint8_t addr;
        uint8_t data[4] = {0};
    } tx_data;
    tx_data.addr = LDC1101_ADDR_RP_DATA_LSB | 0x80; // Read command
    

    this->data_valid = false;
    HAL_GPIO_WritePin(this->cs_port, this->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive_IT(LDC_SPI_HANDLE, (uint8_t*)&tx_data, (uint8_t*)&(this->rx_data), sizeof(tx_data));
}




bool SlipDetector::update(int32_t x)
{
    if (!initialized_)
    {
        fast_ = slow_ = x;
        initialized_ = true;
        return false;
    }

    fast_ += (x - fast_) >> SOD_FAST_SHIFT;
    slow_ += (x - slow_) >> SOD_SLOW_SHIFT;

    int32_t diff = fast_ - slow_;
    slip_detected_ = (diff > SOD_EDGE_THRESHOLD);
    return slip_detected_;
}