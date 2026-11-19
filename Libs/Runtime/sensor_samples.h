#pragma once
#include <cstdint>

/* Copied through single-item RTOS queues; only the control task consumes them. */
struct HallSample {
    int16_t raw[4][3]{};
    float magnitude[4]{};
    uint32_t timestamp_ms = 0;
    bool valid = false;
};
struct LdcSample {
    uint16_t frequency[4]{}, rp[4]{};
    uint8_t slip_votes = 0;
    uint32_t timestamp_ms = 0;
    bool valid = false;
};
struct ImuSample {
    int16_t acceleration[3]{}, gyro[3]{}, quaternion[4]{};
    uint32_t timestamp_ms = 0;
    bool valid = false;
};
struct TofSample {
    uint16_t distance[64]{};
    uint8_t snr[64]{};
    uint32_t timestamp_ms = 0;
    bool valid = false;
};
struct ChargerSample {
    uint16_t voltage = 0;
    bool over_current = false, over_voltage = false, watchdog = false;
    uint32_t timestamp_ms = 0;
    bool valid = false;
};
