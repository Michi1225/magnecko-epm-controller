#pragma once

#include "main.h"
#include "stm32h725xx.h"


constexpr SPI_HandleTypeDef* LDC_SPI_HANDLE = &hspi2; // SPI handle for LDC1614
constexpr char* LDC_SECTION_NAME = ".RAM";

constexpr uint8_t LDC1101_ADDR_RP_SET               = 0x01;
constexpr uint8_t LDC1101_ADDR_TC1                  = 0x02;
constexpr uint8_t LDC1101_ADDR_TC2                  = 0x03;
constexpr uint8_t LDC1101_ADDR_DIG_CONFIG           = 0x04;
constexpr uint8_t LDC1101_ADDR_ALT_CONFIG           = 0x05;
constexpr uint8_t LDC1101_ADDR_RP_THRESH_H_LSB      = 0x06;
constexpr uint8_t LDC1101_ADDR_RP_THRESH_H_MSB      = 0x07;
constexpr uint8_t LDC1101_ADDR_RP_THRESH_L_LSB      = 0x08;
constexpr uint8_t LDC1101_ADDR_RP_THRESH_L_MSB      = 0x09;
constexpr uint8_t LDC1101_ADDR_INTB_MODE            = 0x0A;
constexpr uint8_t LDC1101_ADDR_START_CONFIG         = 0x0B;
constexpr uint8_t LDC1101_ADDR_D_CONF               = 0x0C;
constexpr uint8_t LDC1101_ADDR_L_THRESH_HI_LSB      = 0x16;
constexpr uint8_t LDC1101_ADDR_L_THRESH_HI_MSB      = 0x17;
constexpr uint8_t LDC1101_ADDR_L_THRESH_LO_LSB      = 0x18;
constexpr uint8_t LDC1101_ADDR_L_THRESH_LO_MSB      = 0x19;
constexpr uint8_t LDC1101_ADDR_STATUS               = 0x20;
constexpr uint8_t LDC1101_ADDR_RP_DATA_LSB          = 0x21;
constexpr uint8_t LDC1101_ADDR_RP_DATA_MSB          = 0x22;
constexpr uint8_t LDC1101_ADDR_L_DATA_LSB           = 0x23;
constexpr uint8_t LDC1101_ADDR_L_DATA_MSB           = 0x24;
constexpr uint8_t LDC1101_ADDR_LHR_RCOUNT_LSB       = 0x30;
constexpr uint8_t LDC1101_ADDR_LHR_RCOUNT_MSB       = 0x31;
constexpr uint8_t LDC1101_ADDR_LHR_OFFSET_LSB       = 0x32;
constexpr uint8_t LDC1101_ADDR_LHR_OFFSET_MSB       = 0x33;
constexpr uint8_t LDC1101_ADDR_LHR_CONFIG           = 0x34;
constexpr uint8_t LDC1101_ADDR_LHR_DATA_LSB         = 0x38;
constexpr uint8_t LDC1101_ADDR_LHR_DATA_MID         = 0x39;
constexpr uint8_t LDC1101_ADDR_LHR_DATA_MSB         = 0x3A;
constexpr uint8_t LDC1101_ADDR_LHR_STATUS           = 0x3B;
constexpr uint8_t LDC1101_ADDR_RID                  = 0x3E;
constexpr uint8_t LDC1101_ADDR_CHIP_ID              = 0x3F;


constexpr uint8_t  SOD_FAST_SHIFT       = 2;
constexpr uint8_t  SOD_SLOW_SHIFT       = 5;
constexpr uint8_t  SOD_PERSISTENCE      = 3;
constexpr uint16_t SOD_EDGE_THRESHOLD   = 200;

class SlipDetector
{
public:
    bool update(int32_t x);

    bool slip_detected() const { return slip_detected_; }

private:
    int32_t fast_, slow_;
    bool initialized_, slip_detected_ = false;
};

class LDC1101 {
private:
    GPIO_TypeDef* cs_port;
    uint16_t cs_pin;
    bool initialized = false;
public:
    struct __attribute__((packed))
    {
        uint8_t addr;
        uint16_t rp_data;
        uint16_t l_data;
    }rx_data;

    LDC1101 *next;

    SlipDetector slip_detector;

    bool data_valid = false;
    

    LDC1101(GPIO_TypeDef*cs_port, uint16_t cs_pin);
    HAL_StatusTypeDef init();
    void start_measurement();
    void read_data();
};