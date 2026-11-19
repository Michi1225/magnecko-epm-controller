#pragma once
#include "main.h"
#include <cmath>

//Force estimation parameters
#define A_B  67.3407354f
#define C_B  0.0360662672f
#define M_B  0.528710709f
#define INV_M_B 1.8913935f

#define A_F  595.83695046f
#define C_F  0.90280164f
#define N_F  4.03057196f


//DEVICE_CONFIG_1
#define DEVICE_CONFIG_1_ADDR 0x00
#define DEVICE_CONFIG_1_VAL  0x0C

#define DEVICE_CONFIG_2_ADDR 0x01
#define DEVICE_CONFIG_2_VAL  0x12

#define SENSOR_CONFIG_1_ADDR 0x02
#define SENSOR_CONFIG_1_VAL  0x70

#define SENSOR_CONFIG_2_ADDR 0x03
#define SENSOR_CONFIG_2_VAL  0x00 //angle calculation disabled, 133mT range

#define X_THR_CONFIG_ADDR 0x04
#define X_THR_CONFIG_VAL  0x00

#define Y_THR_CONFIG_ADDR 0x05
#define Y_THR_CONFIG_VAL  0x00

#define Z_THR_CONFIG_ADDR 0x06
#define Z_THR_CONFIG_VAL  0x00

#define T_CONFIG_ADDR 0x07
#define T_CONFIG_VAL  0x01

#define INT_CONFIG_1_ADDR 0x08
#define INT_CONFIG_1_VAL  0x01

#define MAG_GAIN_CONFIG_ADDR 0x09
#define MAG_GAIN_CONFIG_VAL  0x00

#define MAG_OFFSET_CONFIG_1_ADDR 0x0A
#define MAG_OFFSET_CONFIG_1_VAL  0x00

#define MAG_OFFSET_CONFIG_2_ADDR 0x0B
#define MAG_OFFSET_CONFIG_2_VAL  0x00

#define I2C_ADDRESS_ADDR 0x0C
#define I2C_ADDRESS_VAL  0x00 // disable address update from I2C

//read only
#define DEVICE_ID_ADDR 0x0D
#define MANUFACTURER_ID__LSB_ADDR 0x0E
#define MANUFACTURER_ID__MSB_ADDR 0x0F
#define T_MSB_RESULT_ADDR 0x10
#define T_LSB_RESULT_ADDR 0x11
#define X_MSB_RESULT_ADDR 0x12
#define X_LSB_RESULT_ADDR 0x13
#define Y_MSB_RESULT_ADDR 0x14
#define Y_LSB_RESULT_ADDR 0x15
#define Z_MSB_RESULT_ADDR 0x16
#define MAG_MSB_RESULT_ADDR 0x17
#define CONV_STATUS_ADDR 0x18
#define ANGLE_RESULT_MSB_ADDR 0x19
#define ANGLE_RESULT_LSB_ADDR 0x1A
#define MAGNITUDE_RESULT_ADDR 0x1B
#define DEVICE_STATUS_ADDR 0x1C

#define DEVICE_VERSION 2 // 1 for TMAG5273x1, 2 for TMAG5273x2

#if (DEVICE_VERSION == 1)
#define MAG_SENSITIVITY 40 // LSB/mT
#elif (DEVICE_VERSION == 2)
#define MAG_SENSITIVITY 250.0f // LSB/mT
#endif

#define SENS_I2C_HANDLE &hi2c2
#define MAG_SECTION_NAME ".RAM"



class TMAG5273
{
private:
    uint8_t device_address;
    static bool initialized;
    
public:
    enum device_version
    {
        A1,
        B1,
        C1,
        D1
    };

    float bx = 0, by = 0, bz = 0, b_mag = 0;
    bool data_valid = false;
    int16_t raw_bx = 0, raw_by = 0, raw_bz = 0;

    /**
     * @brief TMAG5273 constructor
     * @param version device version
     */
    TMAG5273(device_version version);

    /**
     * @brief initialize all TMAG5273 on the bus
     * @return HAL_OK if success, HAL_ERROR if failed
     */
    static uint8_t init();

    /**
     * @brief read the X axis B-Field value
     * @return Bx value in mT
     */
    float read_Bx();

    /**
     * @brief read the Y axis B-Field value
     * @return By value in mT
     */
    float read_By();

    /**
     * @brief read the Z axis B-Field value
     * @return Bz value in mT
     */
    float read_Bz();

    /**
     * @brief read all three axes B-Field values
     * @return B-Field Magnitude in mT
     */
    float read_magnitude();

    /**
     * @brief read all three axes B-Field values in a blocking manner
     * @return B-Field Magnitude in mT
     */
    float read_magnitude_blocking();

    /**
     * @brief read the temperature value
     * @return temperature value in degree Celsius
     */
    float read_T();

    /**
     * @brief Estimate the Contact
     * @return true if contact, false if no contact
     */
    bool estimate_contact();

    /**
     * @brief Perform Force Estimation
     * @return estimated force in Newton
     */
    static float force_estimation(float b_mag_0, float b_mag_1, float b_mag_2, float b_mag_3);


};
