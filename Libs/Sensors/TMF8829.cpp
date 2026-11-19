#include "TMF8829.h"
#include "app_runtime.h"
#include "main.h"
#include "tmf8829_image.h"
#include <cmath>
#include <cstdint>
#include <cstring>



/**
 * @brief Write the specified register of the TMF8829 via SPI
 * @param registerAddress The address of the register to write
 * @param payload Pointer to the data to write
 * @param payloadLen Length of the data to write (must not exceed SPI_MAX_PAYLOAD)
 * @return HAL status of the operation
 */
static HAL_StatusTypeDef spiWriteReg(uint8_t registerAddress, const uint8_t* payload, size_t payloadLen)
{
    if (payloadLen > SPI_MAX_PAYLOAD) return HAL_ERROR;

    uint8_t txBuf[2 + SPI_MAX_PAYLOAD] = {0};
    txBuf[0] = 0x02;
    txBuf[1] = registerAddress;
    if (payloadLen)
    {
        std::memcpy(&txBuf[2], payload, payloadLen);
    }

    HAL_StatusTypeDef status = App_SpiTransfer(TOF_SPI_HANDLE, txBuf, nullptr, 2 + payloadLen, 20, 0);
    return status;
}

static HAL_StatusTypeDef spiWriteRegDMA(uint8_t registerAddress, const uint8_t* payload, size_t payloadLen)
{
    if (payloadLen > SPI_MAX_PAYLOAD) return HAL_ERROR;

    uint8_t txBuf[2 + SPI_MAX_PAYLOAD] = {0};
    txBuf[0] = 0x02;
    txBuf[1] = registerAddress;
    if (payloadLen)
    {
        std::memcpy(&txBuf[2], payload, payloadLen);
    }

    HAL_StatusTypeDef status = App_SpiTransfer(TOF_SPI_HANDLE, txBuf, nullptr, 2 + payloadLen, 20, 0);
    return status;
}

/**
 * @brief Read the specified register of the TMF8829 via SPI
 * @param registerAddress The address of the register to read
 * @param outData Pointer to the buffer to store the read data
 * @param length Length of the data to read (must not exceed SPI_MAX_PAYLOAD)
 * @return HAL status of the operation
 */
static HAL_StatusTypeDef spiReadReg(uint8_t registerAddress, uint8_t* outData, size_t length)
{
    if (length > SPI_MAX_PAYLOAD) return HAL_ERROR;

    uint8_t txBuf[3 + SPI_MAX_PAYLOAD] = {0};
    uint8_t rxBuf[3 + SPI_MAX_PAYLOAD] = {0};
    txBuf[0] = 0x03;
    txBuf[1] = registerAddress;

    HAL_StatusTypeDef status = App_SpiTransfer(TOF_SPI_HANDLE, txBuf, rxBuf, 3 + length, 20, 0);

    if (status == HAL_OK && length)
    {
        std::memcpy(outData, &rxBuf[3], length);
    }
    return status;
}


/**
 * @brief Read the ranging data frame from the TMF8829 FIFO via SPI. This function initiates a read of the FIFO register, which will return the preheader, header, pixel data, and footer of the latest ranging frame. The data is stored in the provided TMF8829 instance's data_frame member. The function uses an interrupt-based SPI receive to read the data asynchronously, and the data_valid flag of the TMF8829 instance will be set to true in the SPI receive complete callback when the data is ready.
 * @param sensor Pointer to the TMF8829 instance to store the read data
 * @return HAL status of the operation (HAL_OK if the read was initiated successfully, HAL_ERROR if there was an error initiating the read)
 */
static HAL_StatusTypeDef spiReadResult(TMF8829 *sensor)
{
    uint8_t tx_data[sizeof(TMF8829_Frame_t)] = {0};
    tx_data[0] = 0x03; // Read command
    tx_data[1] = TMF8829_FIFOSTATUS;

    HAL_StatusTypeDef status = App_SpiTransfer(TOF_SPI_HANDLE, tx_data, reinterpret_cast<uint8_t*>(&(sensor->data_frame)), sizeof(TMF8829_Frame_t), 20, 0);
    return status;
}

/**
 * @brief Write the specified register of the TMF8829 via SPI (bootloader mode)
 * @param registerAddress The address of the register to write
 * @param payload Pointer to the data to write
 * @param payloadLen Length of the data to write (must not exceed BOOTLOADER_MAX_PAYLOAD)
 * @return HAL status of the operation
 */
static HAL_StatusTypeDef spiWriteBootloader(uint8_t registerAddress, const uint8_t* payload, size_t payloadLen)
{
    if (payloadLen > BOOTLOADER_MAX_PAYLOAD) return HAL_ERROR;

    return spiWriteReg(registerAddress, payload, payloadLen);
}


/**
 * @brief Check if a register has the expected value, with timeout
 * @param regAddr The address of the register to check
 * @param expected The expected value of the register
 * @param timeoutMs Timeout in milliseconds to wait for the expected value
 * @return HAL status of the operation (HAL_OK if expected value is read, HAL_ERROR if timeout occurs or read fails)
 */
static HAL_StatusTypeDef checkRegister(uint8_t regAddr, uint8_t expected, uint32_t timeoutMs)
{
    uint8_t readValue = 0;
    while (timeoutMs--)
    {
        if (spiReadReg(regAddr, &readValue, 1) != HAL_OK)
        {
            return HAL_ERROR;
        }
        if (readValue == expected)
        {
            return HAL_OK;
        }
        App_Delay(0);
    }
    return HAL_ERROR;
}


/**
 * @brief Setup size for FIFO writes to both CPU RAMs in parallel (only supported in bootloader mode)
 * @param address The starting address in the TMF8829 memory to write to (taken from the header of the image)
 * @param len The length of the data to write (taken from the header of the image)
 * @return HAL status of the operation
 */
static HAL_StatusTypeDef bootloaderWriteFifoBoth(uint32_t address, uint32_t len)
{
    uint16_t wsize = ( len + ( 4 - 1 ) ) / 4; // round value to a word size

    uint8_t payload[8];
    payload[0] = TMF8829_COM_BL_CMD_STAT_FIFO_BOTH;
    payload[1] = 6;
    payload[2] = static_cast<uint8_t>(address & 0xFF);
    payload[3] = static_cast<uint8_t>((address >> 8) & 0xFF);
    payload[4] = static_cast<uint8_t>((address >> 16) & 0xFF);
    payload[5] = static_cast<uint8_t>((address >> 24) & 0xFF);
    payload[6] = wsize & 0xFF;
    payload[7] = (wsize >> 8) & 0xFF;

    int status = spiWriteBootloader(TMF8829_CMD_STAT, payload, sizeof(payload));

    status |= checkRegister(TMF8829_CMD_STAT, 0x00, 10); // Wait for command to be processed

    return static_cast<HAL_StatusTypeDef>(status);
}

/**
 * @brief Start the application from RAM after downloading the patch (only supported in bootloader mode)
 * @return HAL status of the operation
 */
static HAL_StatusTypeDef bootloaderStartRamApp()
{
    uint8_t command = BOOT_CMD_START_RAM_APP;
    HAL_StatusTypeDef status = spiWriteBootloader(TMF8829_CMD_STAT, &command, 1);
    if (status != HAL_OK)
    {
        return status;
    }
    return checkRegister(TMF8829_APP_ID, BOOT_APP_ID_APPLICATION, 100);
}

/**
 * @brief Download a RAM patch to the TMF8829. The Image must be stored in the tmf8829_image array, and its start address and length must be specified in tmf8829_image_start and tmf8829_image_length respectively. This function writes the patch to the TMF8829 using the bootloader FIFO write command, then starts the application from RAM.
 * @return HAL status of the operation
 */
HAL_StatusTypeDef TMF8829::ram_patch_download() 
{
    const uint8_t* image = tmf8829_image;
    uint32_t imageStart = static_cast<uint32_t>(tmf8829_image_start);
    uint32_t imageLength = static_cast<uint32_t>(tmf8829_image_length);

    if (image == nullptr || imageLength == 0)
    {
        return HAL_ERROR;
    }

    bootloaderWriteFifoBoth(imageStart, imageLength);

    uint32_t idx = 0;
    while (idx < imageLength)
    {
        size_t chunk = static_cast<size_t>(imageLength - idx);
        if (chunk > BOOTLOADER_MAX_PAYLOAD)
        {
            chunk = BOOTLOADER_MAX_PAYLOAD;
        }

        if (spiWriteBootloader(TMF8829_FIFO, &image[idx], chunk) != HAL_OK)
        {
            return HAL_ERROR;
        }
        idx += static_cast<uint32_t>(chunk);
    }

    if (bootloaderStartRamApp() != HAL_OK)
    {
        return HAL_ERROR;
    }

    uint8_t enableRegister = 0;
    if (spiReadReg(TMF8829_ENABLE, &enableRegister, 1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    enableRegister = (enableRegister & ~ENABLE_POWERUP_SELECT_MASK) | ENABLE_POWERUP_SELECT_RAM;
    if (spiWriteReg(TMF8829_ENABLE, &enableRegister, 1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

TMF8829::TMF8829() {
  this->data_valid = false;
  this->interrupt_clear_pending = false;
  std::memset(&this->data_frame, 0, sizeof(this->data_frame));
}

HAL_StatusTypeDef TMF8829::init(uint16_t timeout)
{
    int status = HAL_OK;

    uint8_t readValue = 0;
    uint8_t configValue = 0;

    //Power up
    configValue = 0x04;
    status |= spiWriteReg(TMF8829_ENABLE, &configValue, 1);

    App_Delay(2);

    uint16_t elapsed_time = 3;

    //Check if device has booted
    do
    {
        status |= spiReadReg(TMF8829_ENABLE, &readValue, 1);
        App_Delay(9);
        elapsed_time += 10;
        if(elapsed_time > timeout) return HAL_ERROR;
    } while (!(readValue & 0x80));
    
    // Disable I2C interface
    configValue = 0x22;
    status |= spiWriteReg(TMF8829_CMD_STAT, &configValue, 1);
    
    //RAM Patch Download
    status |= this->ram_patch_download();

    // Set Interrupt Mask
    configValue = 0x01; // Enable interrupt for new ranging data
    status |= spiWriteReg(TMF8829_INT_ENAB, &configValue, 1);

    App_Delay(0);



    // Load Configuration
    configValue = TMF8829_CMD_LOAD_CFG_8X8;
    status |= spiWriteReg(TMF8829_CMD_STAT, &configValue, 1);

    uint8_t cmdStatus = 0;
    const uint32_t configuration_start = HAL_GetTick();
    do
    {
        status |= spiReadReg(TMF8829_CMD_STAT, &cmdStatus, 1);
        App_Delay(9);
        elapsed_time += 10;
        if(elapsed_time > timeout) return HAL_ERROR;
    } while ((cmdStatus & 0x0F) != 0x00); // Wait for configuration to be loaded

    // Set Ranging Period to 16ms
    configValue = 0x10; // 16ms
    status |= spiWriteReg(TMF8829_CFG_PERIOD_MS_LSB, &configValue, 1);
    configValue = 0x00;
    status |= spiWriteReg(TMF8829_CFG_PERIOD_MS_MSB, &configValue, 1);

    // Set Iterations
    configValue = 0xE1; // 225 Kilo iterations
    status |= spiWriteReg(TMF8829_CFG_KILO_ITERATIONS_LSB, &configValue, 1);
    configValue = 0x00; // 225 Kilo iterations
    status |= spiWriteReg(TMF8829_CFG_KILO_ITERATIONS_MSB, &configValue, 1);

    // Set Result Format
    configValue = 0x01; // Default format, Only distance and SNR
    status |= spiWriteReg(TMF8829_CFG_RESULT_FORMAT, &configValue, 1);

    // Write Page
    configValue = TMF8829_CMD_WRITE_PAGE;
    status |= spiWriteReg(TMF8829_CMD_STAT, &configValue, 1);

    do
    {
        status |= spiReadReg(TMF8829_CMD_STAT, &cmdStatus, 1);
        App_Delay(10);
        if (HAL_GetTick() - configuration_start >= timeout) return HAL_TIMEOUT;
    } while ((static_cast<TMF8829_Status>(cmdStatus & 0x0F) != TMF8829_Status::STAT_ACCEPTED) &&
            ((static_cast<TMF8829_Status>(cmdStatus & 0x0F) != TMF8829_Status::STAT_OK))); // Wait for page to be written

    

    if(status == HAL_OK) this->initialized = true;

    return (HAL_StatusTypeDef)status;
}

HAL_StatusTypeDef TMF8829::start_ranging() 
{
    if(!this->initialized) return HAL_ERROR; // Return error if sensor is not initialized

    uint8_t command = TMF8829_CMD_MEASURE; // Start measurement command
    HAL_StatusTypeDef status = spiWriteReg(TMF8829_CMD_STAT, &command, 1);
    if (status != HAL_OK)
    {
        return status;
    }

    return checkRegister(TMF8829_CMD_STAT, (uint8_t)TMF8829_Status::STAT_ACCEPTED, 100);
}

HAL_StatusTypeDef TMF8829::get_ranging_data()
{
    if (!initialized) return HAL_ERROR;
    data_valid = false;
    const auto status = spiReadResult(this);
    data_valid = status == HAL_OK && data_frame.footer.frame_valid;
    return data_valid ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef TMF8829::clear_interrupt()
{
    if (!initialized) return HAL_ERROR;
    uint8_t clear = 0x0F;
    return spiWriteRegDMA(TMF8829_INT_STATUS, &clear, 1);
}

HAL_StatusTypeDef TMF8829::read_frame()
{
    const auto status = clear_interrupt();
    if (status != HAL_OK) { data_valid = false; return status; }
    return get_ranging_data();
}
