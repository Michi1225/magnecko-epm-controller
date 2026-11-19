#include "BNO086.h"
#include "app_runtime.h"
#include "FreeRTOS.h"
#include "task.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr uint8_t control_channel = 2;
constexpr uint8_t set_feature = 0xFD;
constexpr uint8_t get_feature = 0xFC;
constexpr uint8_t command_response = 0xF1;

uint16_t read16(const uint8_t *bytes) {
    return bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8);
}
uint32_t read32(const uint8_t *bytes) {
    return read16(bytes) | (static_cast<uint32_t>(read16(bytes + 2)) << 16);
}
uint16_t control_report_size(uint8_t id) {
    switch (id) {
    case get_feature: return 17;
    case command_response: return 16;
    case 0xF8: return 16; // Product ID response.
    default: return 0;
    }
}

// H_INTN is deasserted by CS, not by completion of a packet. Keep CS low
// throughout the header and payload, including all asynchronous SPI waits.
struct ChipSelect {
    ChipSelect() { HAL_GPIO_WritePin(IMU_NCS_GPIO_Port, IMU_NCS_Pin, GPIO_PIN_RESET); }
    ~ChipSelect() { HAL_GPIO_WritePin(IMU_NCS_GPIO_Port, IMU_NCS_Pin, GPIO_PIN_SET); }
};
struct Wake {
    Wake() { HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, GPIO_PIN_RESET); }
    ~Wake() { HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, GPIO_PIN_SET); }
};
}

VectorData BNO086::gyro_data{};
VectorData BNO086::accel_data{};
VectorData BNO086::mag_data{};
VectorData BNO086::lin_accel_data{};
VectorData BNO086::grav_data{};
RotationVectorData BNO086::rot_data{};

BNO086::BNO086() : initialized(false) { reset_outputs(); }

void BNO086::reset_outputs()
{
    reports_seen = 0;
    std::memset(report_times, 0, sizeof(report_times));
    std::memset(output_accel, 0, sizeof(output_accel));
    std::memset(output_gyro, 0, sizeof(output_gyro));
    std::memset(output_lin_accel, 0, sizeof(output_lin_accel));
    std::memset(output_quat, 0, sizeof(output_quat));
    gyro_data = {0, 0, 0, 0, 0, 0, BNO086_Q_POINT_GYROSCOPE};
    accel_data = {0, 0, 0, 0, 0, 0, BNO086_Q_POINT_ACCELEROMETER};
    mag_data = {0, 0, 0, 0, 0, 0, BNO086_Q_POINT_MAGNETOMETER};
    lin_accel_data = {0, 0, 0, 0, 0, 0, BNO086_Q_POINT_LINEAR_ACCELERATION};
    grav_data = {0, 0, 0, 0, 0, 0, BNO086_Q_POINT_GRAVITY};
    rot_data = {0, 0, 0, 0, 0, 0, 0, 0, BNO086_Q_POINT_ROTATION, BNO086_Q_POINT_ACCURACY_ROTATION};
}

HAL_StatusTypeDef BNO086::wait_for_message(uint32_t start_ms, uint16_t timeout_ms)
{
    while (HAL_GetTick() - start_ms < timeout_ms) {
        // The BNO086 retries unserviced interrupts after ~10 ms. Edge counts
        // therefore do not represent queued packets and can outlive readiness.
        if (HAL_GPIO_ReadPin(IMU_INT_GPIO_Port, IMU_INT_Pin) == GPIO_PIN_RESET) {
            taskENTER_CRITICAL();
            msgs_ready = 0;
            taskEXIT_CRITICAL();
            return HAL_OK;
        }
        App_Delay(1);
    }
    return HAL_TIMEOUT;
}

HAL_StatusTypeDef BNO086::transfer_packet(const uint8_t *tx, uint16_t tx_length,
                                         uint32_t start_ms, uint16_t timeout_ms)
{
    packet_length = 0;
    ChipSelect selected;
    auto transfer = [&](const uint8_t *out, uint8_t *in, uint16_t size) {
        const uint32_t elapsed = HAL_GetTick() - start_ms;
        if (elapsed >= timeout_ms) return HAL_TIMEOUT;
        return App_SpiTransfer(BNO086_SPI_HANDLE, out, in, size,
            std::min<uint32_t>(10, timeout_ms - elapsed), 0);
    };
    // Always drive MOSI: receive-only mode can leave the previous command's
    // last bit on the wire. A zero SHTP length denotes no outgoing packet.
    uint8_t zeros[128]{};
    auto status = transfer(tx ? tx : zeros, packet, 4);
    if (status != HAL_OK) return status;
    const uint16_t length = read16(packet) & 0x7FFF;
    if (length != 0 && (length < 4 || length > sizeof(packet))) return HAL_ERROR;
    const uint16_t wire_length = std::max<uint16_t>(length, tx_length);
    for (uint16_t offset = 4; offset < wire_length;) {
        const uint16_t chunk = std::min<uint16_t>(sizeof(zeros), wire_length - offset);
        std::memset(zeros, 0, chunk);
        if (tx && offset < tx_length)
            std::memcpy(zeros, tx + offset, std::min<uint16_t>(chunk, tx_length - offset));
        status = transfer(zeros, packet + offset, chunk);
        if (status != HAL_OK) return status;
        offset += chunk;
    }
    // We consume whole packets. Reject continuation fragments rather than
    // decoding a partial report as a new one.
    if (packet[1] & 0x80) return HAL_ERROR;
    packet_length = length;
    return length || tx ? HAL_OK : HAL_BUSY;
}

uint8_t BNO086::init(uint16_t timeout)
{
    const uint32_t start_time = HAL_GetTick();
    initialized = false;
    reset_outputs();
    seqNum = 0;
    feature_count = 0;
#if ENABLE_ACCEL
    add_feature(BNO086_ID_ACCELEROMETER, BNO086_PERIOD_ACCELEROMETER);
#endif
#if ENABLE_ROT
    add_feature(BNO086_ID_ROTATION, BNO086_PERIOD_ROTATION);
#endif
#if ENABLE_GYRO
    add_feature(BNO086_ID_GYROSCOPE, BNO086_PERIOD_GYROSCOPE);
#endif
#if ENABLE_LIN_ACCEL
    add_feature(BNO086_ID_LINEAR_ACCELERATION, BNO086_PERIOD_LINEAR_ACCELERATION);
#endif
#if ENABLE_GRAVITY
    add_feature(BNO086_ID_GRAVITY, BNO086_PERIOD_GRAVITY);
#endif
#if ENABLE_MAG
    add_feature(BNO086_ID_MAGNETOMETER, BNO086_PERIOD_MAGNETOMETER);
#endif
    HAL_GPIO_WritePin(IMU_NCS_GPIO_Port, IMU_NCS_Pin, GPIO_PIN_SET);
    // BOOTN selects normal firmware; PS0/WAKE must stay high through the first
    // H_INTN assertion so a retry also selects SPI rather than UART.
    HAL_GPIO_WritePin(IMU_BOOTN_GPIO_Port, IMU_BOOTN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(IMU_NRST_GPIO_Port, IMU_NRST_Pin, GPIO_PIN_RESET);
    App_Delay(10);
    // As in CEVA's SPI HAL, establish the clock's mode-3 idle level before
    // selecting the sensor for the first time. The H7 retains it with
    // MasterKeepIOState enabled. No sensor packet is clocked while in reset.
    uint8_t dummy_tx = 0, dummy_rx = 0;
    HAL_StatusTypeDef clock_status = HAL_TIMEOUT;
    const uint32_t elapsed = HAL_GetTick() - start_time;
    if (elapsed < timeout)
        clock_status = App_SpiTransfer(BNO086_SPI_HANDLE, &dummy_tx, &dummy_rx, 1,
            std::min<uint32_t>(10, timeout - elapsed), 0);
    taskENTER_CRITICAL();
    msgs_ready = 0;
    taskEXIT_CRITICAL();
    HAL_GPIO_WritePin(IMU_NRST_GPIO_Port, IMU_NRST_Pin, GPIO_PIN_SET);
    if (clock_status != HAL_OK) return clock_status;

    bool advertised = false, reset = false, ready = false;
    while (!(advertised && reset && ready)) {
        auto status = wait_for_message(start_time, timeout);
        if (status != HAL_OK) return status;
        status = transfer_packet(nullptr, 0, start_time, timeout);
        if (status == HAL_BUSY) { App_Delay(1); continue; }
        if (status != HAL_OK) return status;
        if (packet_length > 4 && packet[2] == 0 && packet[4] == 0) advertised = true;
        if (packet_length == 5 && packet[2] == 1 && packet[4] == 1) reset = true;
        if (packet_length >= 20 && packet[2] == control_channel &&
            packet[4] == command_response && packet[6] == 0x84 && packet[10] == 1) {
            if (packet[9] != 0) return HAL_ERROR; // Initialization status R0.
            ready = true;
        }
    }
    initialized = true;
    return HAL_OK;
}

uint8_t BNO086::start(uint16_t timeout)
{
    if (!initialized) return HAL_ERROR;
    const uint32_t start_time = HAL_GetTick();
    for (unsigned feature = 0; feature < feature_count; ++feature) {
        const auto [id, period] = features[feature];
        uint8_t tx[21] = {21, 0, control_channel, seqNum, set_feature, id, 0x08};
        for (unsigned byte = 0; byte < 4; ++byte) tx[9 + byte] = period >> (8 * byte);
        // Zero batch interval requests immediate delivery, rather than keeping
        // up to UINT32_MAX microseconds of data in the sensor's batch queue.
        HAL_StatusTypeDef status;
        {
            Wake awake;
            status = wait_for_message(start_time, timeout);
            if (status != HAL_OK) return status;
            status = transfer_packet(tx, sizeof(tx), start_time, timeout);
        }
        if (status != HAL_OK) return status;
        ++seqNum;
        // SPI is full duplex. Preserve and process any packet received while
        // sending, including packets longer than the outgoing command.
        status = process_packet();
        if (status != HAL_OK) return status;
        for (;;) {
            if (feature_enabled(id)) break;
            status = wait_for_message(start_time, timeout);
            if (status != HAL_OK) return status;
            status = transfer_packet(nullptr, 0, start_time, timeout);
            if (status == HAL_BUSY) { App_Delay(1); continue; }
            if (status != HAL_OK) return status;
            status = process_packet();
            if (status != HAL_OK) return status;
        }
    }
    return HAL_OK;
}

bool BNO086::feature_enabled(uint8_t id) const
{
    if (packet_length < 4 || packet[2] != control_channel) return false;
    // Rate changes can generate responses for several sensors in one packet.
    for (uint16_t offset = 4; offset < packet_length;) {
        const uint16_t size = control_report_size(packet[offset]);
        if (size == 0 || packet_length - offset < size) return false;
        if (packet[offset] == get_feature && packet[offset + 1] == id &&
            read32(packet + offset + 5) != 0) return true;
        offset += size;
    }
    return false;
}

bool BNO086::has_pending() const
{
    return initialized && HAL_GPIO_ReadPin(IMU_INT_GPIO_Port, IMU_INT_Pin) == GPIO_PIN_RESET;
}

bool BNO086::outputs_fresh(uint32_t now) const
{
    return (!ENABLE_LIN_ACCEL || ((reports_seen & 1) && now - report_times[0] <= 50)) &&
           (!ENABLE_GYRO || ((reports_seen & 2) && now - report_times[1] <= 50)) &&
           (!ENABLE_ROT || ((reports_seen & 4) && now - report_times[2] <= 50));
}

uint8_t BNO086::update()
{
    if (!has_pending()) return HAL_BUSY;
    const uint32_t start_time = HAL_GetTick();
    auto status = wait_for_message(start_time, 10);
    if (status != HAL_OK) return status;
    status = transfer_packet(nullptr, 0, start_time, 10);
    return status == HAL_OK ? process_packet() : status;
}

HAL_StatusTypeDef BNO086::process_packet()
{
    if (packet_length == 0) return HAL_OK;
    if (packet[2] == 1 && packet_length == 5 && packet[4] == 1) {
        initialized = false; // Sensor reset: configured reports are no longer running.
        reset_outputs();
        return HAL_ERROR;
    }
    // Advertisements and control responses have no sensor timestamp prefix.
    if (packet[2] != 3 && packet[2] != 4) return HAL_OK;
    for (uint16_t offset = 4; offset < packet_length;) {
        const uint8_t id = packet[offset];
        if (id == 0xFB || id == 0xFA) { // Base timestamp / timestamp rebase.
            if (packet_length - offset < 5) return HAL_ERROR;
            offset += 5;
            continue;
        }
        if (id < BNO086_ID_ACCELEROMETER || id > BNO086_ID_GRAVITY) return HAL_ERROR;
        const uint16_t size = id == BNO086_ID_ROTATION ? 14 : 10;
        if (packet_length - offset < size) return HAL_ERROR;
        const uint8_t *data = packet + offset + 1;
        const bool usable = (data[1] & 0x03) >= 2; // Upper six bits encode delay.
        int freshness = -1;
        if (id == BNO086_ID_ROTATION) {
            std::memcpy(&rot_data, data, 13);
            if (usable) {
                output_quat[0] = rot_data.quaternion_i;
                output_quat[1] = rot_data.quaternion_j;
                output_quat[2] = rot_data.quaternion_k;
                output_quat[3] = rot_data.quaternion_real;
            }
            freshness = 2;
        } else {
            VectorData *target = nullptr;
            int16_t *output = nullptr;
            switch (id) {
            case BNO086_ID_ACCELEROMETER: target = &accel_data; output = output_accel; break;
            case BNO086_ID_GYROSCOPE: target = &gyro_data; output = output_gyro; freshness = 1; break;
            case BNO086_ID_MAGNETOMETER: target = &mag_data; break;
            case BNO086_ID_LINEAR_ACCELERATION: target = &lin_accel_data; output = output_lin_accel; freshness = 0; break;
            case BNO086_ID_GRAVITY: target = &grav_data; break;
            }
            std::memcpy(target, data, 9);
            if (usable && output) {
                output[0] = target->axis_x;
                output[1] = target->axis_y;
                output[2] = target->axis_z;
            }
        }
        if (freshness >= 0) {
            const uint8_t mask = 1U << freshness;
            if (usable) {
                report_times[freshness] = HAL_GetTick();
                reports_seen |= mask;
            } else reports_seen &= ~mask;
        }
        offset += size;
    }
    return HAL_OK;
}

float q_to_float(int16_t raw, uint8_t q_point)
{
    return static_cast<float>(raw) / (1UL << q_point);
}
