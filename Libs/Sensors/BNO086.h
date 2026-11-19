#pragma once



#include "main.h"
#include "stm32h725xx.h"
#include <array>
#include <utility>


//BNO update periods in us
#define BNO086_ID_ACCELEROMETER (uint8_t)0x01
#define BNO086_PERIOD_ACCELEROMETER 2500UL
#define BNO086_Q_POINT_ACCELEROMETER 8

#define BNO086_ID_GYROSCOPE (uint8_t)0x02
#define BNO086_PERIOD_GYROSCOPE 2500UL
#define BNO086_Q_POINT_GYROSCOPE 9

#define BNO086_ID_MAGNETOMETER (uint8_t)0x03
#define BNO086_PERIOD_MAGNETOMETER 2500UL
#define BNO086_Q_POINT_MAGNETOMETER 4

#define BNO086_ID_LINEAR_ACCELERATION (uint8_t)0x04
#define BNO086_PERIOD_LINEAR_ACCELERATION 2500UL
#define BNO086_Q_POINT_LINEAR_ACCELERATION 8

#define BNO086_ID_ROTATION (uint8_t)0x05
#define BNO086_PERIOD_ROTATION 2500UL
#define BNO086_Q_POINT_ROTATION 14
#define BNO086_Q_POINT_ACCURACY_ROTATION 12

#define BNO086_ID_GRAVITY (uint8_t)0x06
#define BNO086_PERIOD_GRAVITY 2500UL
#define BNO086_Q_POINT_GRAVITY 8


#define ENABLE_GRAVITY 0
#define ENABLE_ACCEL 0
#define ENABLE_LIN_ACCEL 1
#define ENABLE_GYRO 1
#define ENABLE_ROT 1
#define ENABLE_MAG 0



#define BNO086_SPI_HANDLE &hspi3
#define PACKED __attribute__((packed))

typedef struct PACKED
{
    int8_t sequence_number;
    int8_t status;
    int8_t delay;
    int16_t axis_x;
    int16_t axis_y;
    int16_t axis_z;
    uint8_t q_point;
}VectorData;

typedef struct PACKED
{
    int8_t sequence_number;
    int8_t status;
    int8_t delay;
    int16_t quaternion_i;
    int16_t quaternion_j;
    int16_t quaternion_k;
    int16_t quaternion_real;
    int16_t accuracy_estimate;
    uint8_t q_point;
    uint8_t q_point_accuracy;
}RotationVectorData;




class BNO086
{
private:
    std::array<std::pair<uint8_t, uint32_t>, 6> features{};
    unsigned feature_count = 0;
    void add_feature(uint8_t id, uint32_t period) { features[feature_count++] = {id, period}; }
    uint32_t report_times[3]{};
    uint8_t reports_seen = 0;
    uint8_t packet[512]{};
    uint16_t packet_length = 0;
    HAL_StatusTypeDef wait_for_message(uint32_t start_ms, uint16_t timeout_ms);
    HAL_StatusTypeDef transfer_packet(const uint8_t *tx, uint16_t tx_length,
                                     uint32_t start_ms, uint16_t timeout_ms);
    HAL_StatusTypeDef process_packet();
    bool feature_enabled(uint8_t id) const;
    void reset_outputs();
    bool initialized;
    
    static VectorData gyro_data;
    static VectorData accel_data;
    static VectorData mag_data;
    static VectorData lin_accel_data;
    static VectorData grav_data;
    static RotationVectorData rot_data;

public:
    BNO086();

    /**
     * @brief   Initialize BNO with the correct settings and define the Feature reports. 
     * @param[in]   timeout: Timeout for the initialization in ms
     * @retval  HAL status; HAL_OK only after the startup messages are validated.
     */
    uint8_t init(uint16_t timeout = 1000);

    /**
     * @brief   Start feature reports, as defined in init()
     * @param[in]   timeout: Timeout for the start command in ms
     * @retval  HAL status; HAL_OK only after each feature is acknowledged.
     */
    uint8_t start(uint16_t timeout);

    /**
     * @brief   Updates the sensor values from the SPI interface. 
                This function should be called within 10ms of
                the BNO interrupt line assert.
     * @retval  HAL Status Code. 0 if all SPI Receive were successful
     */
    uint8_t update();


    uint8_t seqNum = 0;
    volatile uint32_t msgs_ready = 0;
    void on_interrupt() { if (msgs_ready != UINT32_MAX) ++msgs_ready; }
    bool has_pending() const;
    bool outputs_fresh(uint32_t now) const;

    int16_t output_gyro[3]{};
    int16_t output_accel[3]{};
    int16_t output_lin_accel[3]{};
    int16_t output_quat[4]{};

};

float q_to_float(int16_t raw, uint8_t q_point);
