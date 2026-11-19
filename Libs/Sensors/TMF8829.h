#pragma once


#include "main.h"
#include <cstdint>
#include <cstring>
#include <cmath>


constexpr SPI_HandleTypeDef* TOF_SPI_HANDLE = &hspi1; // SPI handle for TOF communication, adjust if needed
constexpr const char* TOF_SECTION_NAME = ".RAM"; // Section name for placing the TMF8829 object in RAM

constexpr uint8_t TMF8829_APP_ID = 0x00;
constexpr uint8_t TMF8829_MAJOR = 0x01;
constexpr uint8_t TMF8829_MINOR = 0x02;
constexpr uint8_t TMF8829_CMD_STAT = 0x08;
constexpr uint8_t TMF8829_PREV_CMD = 0x09;
constexpr uint8_t TMF8829_GPIO_VALUE = 0x10;
constexpr uint8_t TMF8829_LIVE_BEAT_0 = 0x1A;
constexpr uint8_t TMF8829_LIVE_BEAT_1 = 0x1B;
constexpr uint8_t TMF8829_SERIAL_NUMBER_0 = 0x1C;
constexpr uint8_t TMF8829_SERIAL_NUMBER_1 = 0x1D;
constexpr uint8_t TMF8829_SERIAL_NUMBER_2 = 0x1E;
constexpr uint8_t TMF8829_SERIAL_NUMBER_3 = 0x1F;
constexpr uint8_t TMF8829_CID_RID_0 = 0x20;
constexpr uint8_t TMF8829_PAYLOAD = 0x21;
constexpr uint8_t TMF8829_I2C_DEVADDR = 0xE0;
constexpr uint8_t TMF8829_INT_STATUS = 0xE1;
constexpr uint8_t TMF8829_INT_ENAB = 0xE2;
constexpr uint8_t TMF8829_ID = 0xE3;
constexpr uint8_t TMF8829_REVID = 0xE4;
constexpr uint8_t TMF8829_INTERFACE = 0xE9;
constexpr uint8_t TMF8829_GPIO01CFG = 0xF1;
constexpr uint8_t TMF8829_GPIO23CFG = 0xF2;
constexpr uint8_t TMF8829_GPIO45CFG = 0xF3;
constexpr uint8_t TMF8829_GPIO6CFG = 0xF4;
constexpr uint8_t TMF8829_RESET = 0xF7;
constexpr uint8_t TMF8829_ENABLE = 0xF8;
constexpr uint8_t TMF8829_FIFOSTATUS = 0xFA;
constexpr uint8_t TMF8829_SYSTICK_0 = 0xFB;
constexpr uint8_t TMF8829_SYSTICK_1 = 0xFC;
constexpr uint8_t TMF8829_SYSTICK_2 = 0xFD;
constexpr uint8_t TMF8829_SYSTICK_3 = 0xFE;
constexpr uint8_t TMF8829_FIFO = 0xFF;
constexpr uint8_t TMF8829_CFG_PERIOD_MS_LSB = 0x22;
constexpr uint8_t TMF8829_CFG_PERIOD_MS_MSB = 0x23;
constexpr uint8_t TMF8829_CFG_KILO_ITERATIONS_LSB = 0x24;
constexpr uint8_t TMF8829_CFG_KILO_ITERATIONS_MSB = 0x25;
constexpr uint8_t TMF8829_CFG_FP_MODE = 0x26;
constexpr uint8_t TMF8829_CFG_SPAD_SELECT = 0x27;
constexpr uint8_t TMF8829_CFG_REF_SPAD_SELECT = 0x28;
constexpr uint8_t TMF8829_CFG_SPAD_DEADTIME = 0x29;
constexpr uint8_t TMF8829_CFG_RESULT_FORMAT = 0x2A;
constexpr uint8_t TMF8829_CFG_DUMP_HISTOGRAMS = 0x2B;
constexpr uint8_t TMF8829_CFG_POWER_MODES = 0x2E;
constexpr uint8_t TMF8829_CFG_VCSEL_ON = 0x30;
constexpr uint8_t TMF8829_CFG_DITHER = 0x31;
constexpr uint8_t TMF8829_CFG_VCDRV = 0x32;
constexpr uint8_t TMF8829_CFG_VCDRV_2 = 0x33;
constexpr uint8_t TMF8829_CFG_VCDRV_3 = 0x34;
constexpr uint8_t TMF8829_VCSEL_PERIOD_200PS_LSB = 0x36;
constexpr uint8_t TMF8829_VCSEL_PERIOD_200PS_MSB = 0x37;
constexpr uint8_t TMF8829_VCDRV_OFFSET_200PS_LSB = 0x38;
constexpr uint8_t TMF8829_VCDRV_OFFSET_200PS_MSB = 0x39;
constexpr uint8_t TMF8829_VCDRV_CP = 0x3A;
constexpr uint8_t TMF8829_HISTOGRAM_BINS_LSB = 0x40;
constexpr uint8_t TMF8829_HISTOGRAM_BINS_MSB = 0x41;
constexpr uint8_t TMF8829_BIN_SHIFT = 0x42;
constexpr uint8_t TMF8829_REF_BIN_SHIFT = 0x43;
constexpr uint8_t TMF8829_TDC_OFFSET_200PS_LSB = 0x44;
constexpr uint8_t TMF8829_TDC_OFFSET_200PS_MSB = 0x45;
constexpr uint8_t TMF8829_TDC_PRE_PERIODS_LSB = 0x46;
constexpr uint8_t TMF8829_TDC_PRE_PERIODS_MSB = 0x47;
constexpr uint8_t TMF8829_HV_CP = 0x48;
constexpr uint8_t TMF8829_CFG_HA_KILO_ITERATIONS_LSB = 0x4A;
constexpr uint8_t TMF8829_CFG_HA_KILO_ITERATIONS_MSB = 0x4B;
constexpr uint8_t TMF8829_CFG_ENABLE_DUAL_MODE = 0x4C;
constexpr uint8_t TMF8829_CFG_HV_CP_OVERLOAD_DETECT = 0x4D;
constexpr uint8_t TMF8829_CFG_ALG_PEAK_BINS =  0x50;
constexpr uint8_t TMF8829_CFG_ALG_REF_PEAK_BINS = 0x51;
constexpr uint8_t TMF8829_CFG_ALG_DISTANCE = 0x52;
constexpr uint8_t TMF8829_CFG_ALG_CONFIDENCE_THRESHOLD = 0x53;
constexpr uint8_t TMF8829_CFG_ALG_HW_PEAK_START = 0x57;
constexpr uint8_t TMF8829_CFG_ALG_CALIBRATION = 0x5F;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_0 = 0x60;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_1 = 0x61;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_2 = 0x62;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_3 = 0x63;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_4 = 0x64;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_5 = 0x65;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_6 = 0x66;
constexpr uint8_t TMF8829_CFG_INT_ZONE_MASK_7 = 0x67;
constexpr uint8_t TMF8829_CFG_INT_THRESHOLD_LOW_LSB = 0x68;
constexpr uint8_t TMF8829_CFG_INT_THRESHOLD_LOW_MSB = 0x69;
constexpr uint8_t TMF8829_CFG_INT_THRESHOLD_HIGH_LSB = 0x6A;
constexpr uint8_t TMF8829_CFG_INT_THRESHOLD_HIGH_MSB = 0x6B;
constexpr uint8_t TMF8829_CFG_INT_PERSISTENCE = 0x6C;
constexpr uint8_t TMF8829_CFG_POST_PROCESSING = 0x6D;
constexpr uint8_t TMF8829_CFG_PROX_DISTANCE = 0x6E;
constexpr uint8_t TMF8829_CFG_CROP_TOP_X = 0x70;
constexpr uint8_t TMF8829_CFG_CROP_TOP_Y = 0x71;
constexpr uint8_t TMF8829_CFG_CROP_BOTTOM_X = 0x72;
constexpr uint8_t TMF8829_CFG_CROP_BOTTOM_Y = 0x73;
constexpr uint8_t TMF8829_CFG_INFO_FOV_CORR = 0x78;
constexpr uint8_t TMF8829_CFG_GPIO_0 = 0x80;
constexpr uint8_t TMF8829_CFG_GPIO_1 = 0x81;
constexpr uint8_t TMF8829_CFG_GPIO_2 = 0x82;
constexpr uint8_t TMF8829_CFG_GPIO_3 = 0x83;
constexpr uint8_t TMF8829_CFG_GPIO_4 = 0x84;
constexpr uint8_t TMF8829_CFG_GPIO_5 = 0x85;
constexpr uint8_t TMF8829_CFG_GPIO_6 = 0x86;
constexpr uint8_t TMF8829_CFG_GPIO = 0x87;
constexpr uint8_t TMF8829_CFG_I2C_ADDRESS = 0x90;
constexpr uint8_t TMF8829_CFG_MOTION_DETECT_DISTANCE_LSB = 0xB0;
constexpr uint8_t TMF8829_CFG_MOTION_DETECT_DISTANCE_MSB = 0xB1;
constexpr uint8_t TMF8829_CFG_MOTION_DETECT_SNR = 0xB2;
constexpr uint8_t TMF8829_CFG_MOTION_RELEASE_SNR = 0xB3;
constexpr uint8_t TMF8829_CFG_MOTION_ADJACENT_PIXEL = 0xB4;

constexpr uint8_t TMF8829_CMD_LOAD_CFG_8X8 = 64;
constexpr uint8_t TMF8829_CMD_LOAD_CFG_8X8_LONG_RANGE = 65;
constexpr uint8_t TMF8829_CMD_LOAD_CFG_8X8_HIGH_ACCURACY = 66;

constexpr uint8_t TMF8829_CMD_MEASURE = 0x10;
constexpr uint8_t TMF8829_CMD_WRITE_PAGE_AND_MEASURE = 20;
constexpr uint8_t TMF8829_CMD_WRITE_PAGE = 21;

constexpr size_t BOOTLOADER_MAX_PAYLOAD = 500;
constexpr size_t SPI_MAX_PAYLOAD = 1024;
constexpr uint8_t BOOT_CMD_START_RAM_APP = 0x16;
constexpr uint8_t BOOT_CMD_SET_RAM_ADDR = 0x43;
constexpr uint8_t TMF8829_COM_BL_CMD_STAT_FIFO_BOTH = 0x45;
constexpr uint8_t BOOT_APP_ID_APPLICATION = 0x01;
constexpr uint8_t ENABLE_POWERUP_SELECT_RAM = 0x20;
constexpr uint8_t ENABLE_POWERUP_SELECT_MASK = 0x30;

enum class TMF8829_Status: uint8_t
{
    STAT_OK,
    STAT_ACCEPTED,
    STAT_ERR_CONFIG,
    STAT_ERR_APPLICATION,
    STAT_ERR_CONFIG_RESULT_SIZE,
    STAT_ERR_CONFIG_VCSEL,
    STAT_ERR_WAKEUP_TIMED,
    STAT_ERR_RESET_UNEXPECTED,
    STAT_ERR_UNKNOWN_CMD,
    STAT_ERR_UNKNOWN_CID,
    STAT_ERR_STOP_0,
    STAT_ERR_STOP_1,
    STAT_ERR_STOP_2,
    STAT_ERR_STOP_3,
    STAT_ERR_OSC_TUNE
};

typedef struct __attribute(( packed ))
{
    uint8_t fifo_status;
    uint32_t systick;
}TMF8829_Preheader_t;

typedef struct __attribute(( packed ))
{
    uint8_t fp_mode  : 4;
    uint8_t frame_id : 4;
    uint8_t format;
    uint16_t payload;
    uint32_t frame_number;
    uint8_t temp1;
    uint8_t temp2;
    uint8_t temp3;
    uint8_t reserved[5];
}TMF8829_Header_t;

typedef struct __attribute(( packed ))
{
    uint16_t distance;
    uint8_t snr;
}TMF8829_Pixel_Data_t;

typedef struct __attribute(( packed ))
{
    uint32_t t0_integration;
    uint32_t t1_integration;
    uint8_t frame_valid                 : 1;
    uint8_t                             : 2;
    uint8_t spad_max_power              : 1;
    uint8_t vcsel_max_power             : 1;
    uint8_t vcsel_burst_limit_reached   : 1;
    uint8_t frame_aborted               : 2;
    uint8_t reserved;
    uint16_t end_of_frame;
}TMF8829_Footer_t;

typedef struct __attribute(( packed ))
{
    uint8_t reserved[3];
    TMF8829_Preheader_t preheader;
    TMF8829_Header_t header;
    TMF8829_Pixel_Data_t pixel_data[8][8];
    TMF8829_Footer_t footer;
}TMF8829_Frame_t;




class TMF8829
{
private:
    bool initialized = false;
    
    HAL_StatusTypeDef ram_patch_download();
    public:
    bool data_valid, interrupt_clear_pending;
    TMF8829_Frame_t data_frame;



    TMF8829();
    HAL_StatusTypeDef init(uint16_t timeout = 1000);
    HAL_StatusTypeDef start_ranging();
    HAL_StatusTypeDef get_ranging_data();
    HAL_StatusTypeDef read_frame();
    HAL_StatusTypeDef clear_interrupt();
    void update_data();
};