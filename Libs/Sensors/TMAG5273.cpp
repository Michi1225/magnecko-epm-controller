#include "TMAG5273.h"
#include "app_runtime.h"


__section(MAG_SECTION_NAME) uint8_t rxDataHall[6];
__section(MAG_SECTION_NAME) bool TMAG5273::initialized = false;


TMAG5273::TMAG5273(device_version version)
{
    switch (version)
    {
        case A1:
            this->device_address = 0x35;
            break;
        case B1:
            this->device_address = 0x22;
            break;
        case C1:
            this->device_address = 0x78;
            break;
        case D1:
            this->device_address = 0x44;
            break;
        default:
            this->device_address = 0x35; // Default to A1 if invalid version
            break;
    }
}


uint8_t TMAG5273::init()
{
    initialized = false;
    uint8_t data[0x0E] =
    {
        DEVICE_CONFIG_1_ADDR,
        DEVICE_CONFIG_1_VAL,
        DEVICE_CONFIG_2_VAL,
        SENSOR_CONFIG_1_VAL,
        SENSOR_CONFIG_2_VAL,
        X_THR_CONFIG_VAL,
        Y_THR_CONFIG_VAL,
        Z_THR_CONFIG_VAL,
        T_CONFIG_VAL,
        INT_CONFIG_1_VAL,
        MAG_GAIN_CONFIG_VAL,
        MAG_OFFSET_CONFIG_1_VAL,
        MAG_OFFSET_CONFIG_2_VAL,
        I2C_ADDRESS_VAL
    };
    uint8_t general_call_write = 0x00;
    uint8_t error_code = App_I2cTransfer(SENS_I2C_HANDLE, general_call_write, 0, 0, data, nullptr, 0x0E, 1000, 0);
    if(error_code == HAL_OK) TMAG5273::initialized = true;
    return error_code;
}

float TMAG5273::read_Bx()
{
    if(TMAG5273::initialized == false) return 0.0f; // Return 0 if sensor is not initialized
    uint8_t txData[1] = {X_MSB_RESULT_ADDR}; // Read operation
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, txData, nullptr, 1, 100, 0) != HAL_OK) Error_Handler();
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, nullptr, rxDataHall, 2, 100, 0) != HAL_OK) Error_Handler();

    int16_t raw_value = (static_cast<int16_t>(rxDataHall[0]) << 8) | static_cast<int16_t>(rxDataHall[1]);
    float bx = static_cast<float>(raw_value) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    return bx;
}


float TMAG5273::read_By()
{
    if(TMAG5273::initialized == false) return 0.0f; // Return 0 if sensor is not initialized
    uint8_t txData[1] = {Y_MSB_RESULT_ADDR};
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, txData, nullptr, 1, 100, 0) != HAL_OK) Error_Handler();
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, nullptr, rxDataHall, 2, 100, 0) != HAL_OK) Error_Handler();

    int16_t raw_value = (static_cast<int16_t>(rxDataHall[0]) << 8) | static_cast<int16_t>(rxDataHall[1]);
    float by = static_cast<float>(raw_value) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    return by;
}



float TMAG5273::read_Bz()
{
    if(TMAG5273::initialized == false) return 0.0f; // Return 0 if sensor is not initialized
    uint8_t txData[1] = {Z_MSB_RESULT_ADDR};
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, txData, nullptr, 1, 100, 0) != HAL_OK) Error_Handler();
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, nullptr, rxDataHall, 2, 100, 0) != HAL_OK) Error_Handler();

    int16_t raw_value = (static_cast<int16_t>(rxDataHall[0]) << 8) | static_cast<int16_t>(rxDataHall[1]);
    float bz = static_cast<float>(raw_value) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    return bz;
}

float TMAG5273::read_magnitude()
{
    if(TMAG5273::initialized == false) 
    {
        this->bx = 0.0f;
        this->by = 0.0f;
        this->bz = 0.0f;
        this->b_mag = 0.0f;
        this->raw_bx = 0;
        this->raw_by = 0;
        this->raw_bz = 0;
        return 0.0f; // Return 0 if sensor is not initialized
    }
    this->data_valid = false;
    if (App_HallRead(this->device_address << 1, X_MSB_RESULT_ADDR,
            rxDataHall, sizeof(rxDataHall), 5) != HAL_OK) return this->b_mag;
    this->data_valid = true;

    int16_t raw_value_x = (static_cast<int16_t>(rxDataHall[0]) << 8) | static_cast<int16_t>(rxDataHall[1]);
    this->bx = static_cast<float>(raw_value_x) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    this->raw_bx = raw_value_x;

    int16_t raw_value_y = (static_cast<int16_t>(rxDataHall[2]) << 8) | static_cast<int16_t>(rxDataHall[3]);
    this->by = static_cast<float>(raw_value_y) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    this->raw_by = raw_value_y;

    int16_t raw_value_z = (static_cast<int16_t>(rxDataHall[4]) << 8) | static_cast<int16_t>(rxDataHall[5]);
    this->bz = static_cast<float>(raw_value_z) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    this->raw_bz = raw_value_z;
    
    float magnitude = sqrt(this->bx * this->bx + this->by * this->by + this->bz * this->bz);
    this->b_mag = magnitude;
    return magnitude;
}

float TMAG5273::read_magnitude_blocking()
{
    if(TMAG5273::initialized == false) return 0.0f; // Return 0 if sensor is not initialized
    App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, X_MSB_RESULT_ADDR, I2C_MEMADD_SIZE_8BIT, nullptr, rxDataHall, 6, 100, 0);



    int16_t raw_value_x = (static_cast<int16_t>(rxDataHall[0]) << 8) | static_cast<int16_t>(rxDataHall[1]);
    this->bx = static_cast<float>(raw_value_x) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    this->raw_bx = raw_value_x;

    int16_t raw_value_y = (static_cast<int16_t>(rxDataHall[2]) << 8) | static_cast<int16_t>(rxDataHall[3]);
    this->by = static_cast<float>(raw_value_y) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    this->raw_by = raw_value_y;

    int16_t raw_value_z = (static_cast<int16_t>(rxDataHall[4]) << 8) | static_cast<int16_t>(rxDataHall[5]);
    this->bz = static_cast<float>(raw_value_z) / 65536.0f * 2.0f * MAG_SENSITIVITY;
    this->raw_bz = raw_value_z;
    
    float magnitude = sqrt(this->bx * this->bx + this->by * this->by + this->bz * this->bz);
    this->b_mag = magnitude;
    return magnitude;    
}

float TMAG5273::read_T()
{
    if(TMAG5273::initialized == false) return 0.0f; // Return 0 if sensor is not initialized
    uint8_t txData[1] = {T_MSB_RESULT_ADDR};
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, txData, nullptr, 1, 100, 0) != HAL_OK) Error_Handler();
    if(App_I2cTransfer(SENS_I2C_HANDLE, this->device_address << 1, 0, 0, nullptr, rxDataHall, 2, 100, 0) != HAL_OK) Error_Handler();

    
    int16_t raw = (static_cast<int16_t>(rxDataHall[0]) << 8) | rxDataHall[1];
    float t = 25.0f + (raw - 17508) / 60.1f;
    return t;
}

bool TMAG5273::estimate_contact()
{
    if(TMAG5273::initialized == false) return false; // Return false if sensor is not initialized
    float b_mag = this->read_magnitude();
    float b_mag_threshold = 0.5f; // Threshold for contact estimation
    return b_mag < b_mag_threshold;
}

float TMAG5273::force_estimation(float b_mag_0, float b_mag_1, float b_mag_2, float b_mag_3)
{
    if(TMAG5273::initialized == false) return 0.0f; // Return 0 if sensor is not initialized
    // Average B-field
    const float B = 0.25f * (b_mag_0 + b_mag_1 + b_mag_2 + b_mag_3);

    //Air gap estimate
    float g = powf(B / A_B, INV_M_B) - C_B;

    //Force estimate
    float F = A_F / powf(g + C_F, N_F);
    return F;
}
