#include "BNO086.h"
#include "app_runtime.h"
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>

SPI_HandleTypeDef hspi3{};
GPIO_TypeDef test_gpio{};
unsigned mock_critical_depth = 0;
using Bytes = std::vector<uint8_t>;
static uint32_t now_ms = 100;
static BNO086 *active;
static bool cs_active = false, wake_low = false, boot_high = true, reset_low = false;
static bool clock_primed = false;
static bool auto_boot = true, wake_ready = true, acknowledge = true, edges = true;
static bool wrong_ack = false, disabled_ack = false, reset_on_write = false;
static uint8_t ack_channel = 2, ack_id = 0;
static int fail_after = -1;
static unsigned transfers = 0;
static uint32_t reset_at = 0, wake_at = 0;
static size_t offset = 0;
static std::deque<Bytes> pending;
static Bytes wire_in, wire_out, on_wake;
static std::vector<Bytes> boot_packets, commands;

static Bytes frame(uint8_t channel, Bytes payload) {
    Bytes result = {0, 0, channel, 0};
    result.insert(result.end(), payload.begin(), payload.end());
    result[0] = result.size() & 255; result[1] = result.size() >> 8;
    return result;
}
static Bytes init_response(uint8_t status = 0) {
    Bytes payload(16); payload[0] = 0xF1; payload[2] = 0x84;
    payload[5] = status; payload[6] = 1;
    return frame(2, payload);
}
static void enqueue(Bytes packet) {
    pending.push_back(std::move(packet));
    if (edges) active->on_interrupt();
}
static void vector_report(Bytes &payload, uint8_t id, int16_t x, uint8_t status = 2) {
    payload.insert(payload.end(), {id, 1, status, 0, static_cast<uint8_t>(x),
        static_cast<uint8_t>(x >> 8), 2, 0, 3, 0});
}
static void rotation_report(Bytes &payload, uint8_t status = 2, uint16_t accuracy = 100) {
    payload.insert(payload.end(), {BNO086_ID_ROTATION, 1, status, 0,
        1, 0, 2, 0, 3, 0, 4, 0, static_cast<uint8_t>(accuracy), static_cast<uint8_t>(accuracy >> 8)});
}
static Bytes samples() {
    Bytes payload = {0xFB, 0, 0, 0, 0};
    vector_report(payload, BNO086_ID_GYROSCOPE, 1234);
    vector_report(payload, BNO086_ID_LINEAR_ACCELERATION, -321);
    rotation_report(payload);
    return frame(3, payload);
}
static Bytes feature_response(const Bytes &command, uint8_t channel = 2, uint8_t id = 0) {
    Bytes result(command.begin(), command.begin() + 21);
    result[2] = channel; result[4] = 0xFC;
    if (id) result[5] = id;
    return result;
}
static void clear_bus() {
    assert(!cs_active && !wake_low);
    pending.clear(); on_wake.clear(); commands.clear();
    auto_boot = wake_ready = acknowledge = edges = true;
    wrong_ack = disabled_ack = reset_on_write = false;
    ack_channel = 2; ack_id = 0;
    fail_after = -1;
    // Vary advertisement length and put executable reset before SH-2 init.
    boot_packets = {frame(0, Bytes(303, 0)), frame(1, {1}), init_response()};
}

extern "C" uint32_t HAL_GetTick() { return now_ms; }
extern "C" void App_Delay(uint32_t ms) {
    assert(mock_critical_depth == 0);
    now_ms += ms + 1;
}
extern "C" GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *, uint16_t pin) {
    assert(pin == IMU_INT_Pin);
    return !pending.empty() || (wake_low && wake_ready && now_ms - wake_at >= 2)
        ? GPIO_PIN_RESET : GPIO_PIN_SET;
}
extern "C" void HAL_GPIO_WritePin(GPIO_TypeDef *, uint16_t pin, GPIO_PinState state) {
    if (pin == IMU_BOOTN_Pin) boot_high = state == GPIO_PIN_SET;
    if (pin == IMU_WAKE_Pin) {
        wake_low = state == GPIO_PIN_RESET;
        if (wake_low) {
            wake_at = now_ms;
            if (!on_wake.empty()) { enqueue(std::move(on_wake)); on_wake.clear(); }
        }
    }
    if (pin == IMU_NRST_Pin) {
        if (state == GPIO_PIN_RESET) {
            reset_low = true; reset_at = now_ms; pending.clear(); clock_primed = false;
        } else {
            assert(reset_low && now_ms - reset_at >= 10 && boot_high && !wake_low && !cs_active);
            reset_low = false;
            if (auto_boot) for (const auto &packet : boot_packets) enqueue(packet);
        }
    }
    if (pin != IMU_NCS_Pin) return;
    if (state == GPIO_PIN_RESET) {
        assert(!cs_active);
        assert(clock_primed);
        assert(HAL_GPIO_ReadPin(nullptr, IMU_INT_Pin) == GPIO_PIN_RESET);
        cs_active = true; offset = 0; wire_out.clear();
        wire_in = pending.empty() ? Bytes(4, 0) : pending.front();
        if (!pending.empty()) pending.pop_front();
    } else if (cs_active) {
        cs_active = false;
        if (wire_out.size() >= 21 && wire_out[4] == 0xFD) {
            assert(wake_low); // Do not let the BNO sleep before command completion.
            assert(offset >= wire_in.size()); // Drain a longer duplex packet.
            assert(wire_out[2] == 2 && wire_out[3] == commands.size());
            for (size_t i = 13; i < 21; ++i) assert(wire_out[i] == 0);
            commands.push_back(wire_out);
            if (reset_on_write) enqueue(frame(1, {1}));
            else if (acknowledge) {
                if (wrong_ack) {
                    enqueue(feature_response(wire_out, 0)); // Wrong channel.
                    enqueue(feature_response(wire_out, 2, 0x3F)); // Wrong sensor.
                }
                auto response = feature_response(wire_out, ack_channel, ack_id);
                if (disabled_ack) std::memset(response.data() + 9, 0, 4);
                if (wrong_ack) {
                    auto unrelated = feature_response(wire_out, 2, 0x3F);
                    unrelated.insert(unrelated.end(), response.begin() + 4, response.end());
                    unrelated[0] = unrelated.size();
                    response = std::move(unrelated);
                }
                enqueue(std::move(response));
            }
        }
    }
}
extern "C" HAL_StatusTypeDef App_SpiTransfer(SPI_HandleTypeDef *, const uint8_t *tx,
    uint8_t *rx, uint16_t length, uint32_t timeout, int dma) {
    assert(mock_critical_depth == 0 && tx && rx && !dma);
    assert(length && timeout && timeout <= 10);
    ++transfers; ++now_ms;
    if (fail_after == 0) return HAL_TIMEOUT;
    if (fail_after > 0) --fail_after;
    if (!cs_active) {
        assert(reset_low && length == 1 && boot_high && !wake_low);
        clock_primed = true; rx[0] = 0;
        return HAL_OK;
    }
    wire_out.insert(wire_out.end(), tx, tx + length);
    for (unsigned i = 0; i < length; ++i)
        rx[i] = offset + i < wire_in.size() ? wire_in[offset + i] : 0;
    offset += length;
    return HAL_OK;
}
static uint8_t read(BNO086 &imu, Bytes packet) {
    enqueue(std::move(packet));
    const auto status = imu.update();
    assert(!cs_active && offset >= wire_in.size());
    return status;
}

int main() {
    BNO086 imu; active = &imu; clear_bus();
    // A live low level works with every interrupt missed, in either boot order.
    edges = false;
    assert(imu.init(100) == HAL_OK);
    assert(!imu.outputs_fresh(now_ms));
    wrong_ack = true; on_wake = samples();
    assert(imu.start(150) == HAL_OK);
    assert(commands.size() == 3);
    assert(imu.output_gyro[0] == 1234 && imu.output_lin_accel[0] == -321);
    assert(imu.output_quat[3] == 4); // Accuracy need not equal the old magic value.

    // Stale edge counts are hints, never authorization to clock an asleep part.
    imu.msgs_ready = 9;
    const auto before_transfers = transfers;
    assert(!imu.has_pending() && imu.update() == HAL_BUSY && transfers == before_transfers);
    assert(read(imu, samples()) == HAL_OK && imu.outputs_fresh(now_ms));
    now_ms += 51; assert(!imu.outputs_fresh(now_ms));

    // Upper status bits contain delay, even when the low two bits are unreliable.
    Bytes payload; vector_report(payload, BNO086_ID_GYROSCOPE, 99, 0xFE);
    assert(read(imu, frame(3, payload)) == HAL_OK && imu.output_gyro[0] == 99);
    payload.clear(); vector_report(payload, BNO086_ID_GYROSCOPE, 55, 0x81);
    assert(read(imu, frame(3, payload)) == HAL_OK && imu.output_gyro[0] == 99);
    assert(!imu.outputs_fresh(now_ms));
    assert(read(imu, samples()) == HAL_OK && imu.outputs_fresh(now_ms));
    payload.clear(); rotation_report(payload, 0xFC, 12868);
    assert(read(imu, frame(3, payload)) == HAL_OK && !imu.outputs_fresh(now_ms));

    // Control packets are drained without pretending that their bytes are samples.
    assert(read(imu, frame(2, Bytes(40, 0))) == HAL_OK);
    assert(read(imu, frame(2, Bytes(508, 0))) == HAL_OK); // Maximum packet.
    payload = {0xFB, 0, 0, 0, 0};
    vector_report(payload, BNO086_ID_GYROSCOPE, 42);
    payload.insert(payload.end(), {0xFE, 1, 2, 3});
    assert(read(imu, frame(3, payload)) == HAL_ERROR && imu.output_gyro[0] == 42);
    payload = {0xFB, 0, 0, 0, 0, BNO086_ID_GYROSCOPE, 1, 2};
    assert(read(imu, frame(3, payload)) == HAL_ERROR && imu.output_gyro[0] == 42);
    assert(read(imu, frame(4, {0xFB, 0, 0, 0, 0})) == HAL_OK);
    // A failed body transfer never publishes partially overwritten samples.
    fail_after = 1;
    enqueue(samples()); assert(imu.update() == HAL_TIMEOUT && !cs_active);
    assert(imu.output_gyro[0] == 42);
    fail_after = -1; assert(read(imu, samples()) == HAL_OK);
    auto fragment = samples(); fragment[1] |= 0x80;
    assert(read(imu, fragment) == HAL_ERROR);
    enqueue({0, 0, 3, 0}); assert(imu.update() == HAL_BUSY && !cs_active);
    enqueue({3, 0, 3, 0}); assert(imu.update() == HAL_ERROR && !cs_active);
    enqueue({0xFF, 0x7F, 3, 0}); assert(imu.update() == HAL_ERROR && !cs_active);
    assert(read(imu, samples()) == HAL_OK);

    // Tick zero and rollover are valid acquisition times.
    now_ms = UINT32_MAX - 1;
    assert(read(imu, samples()) == HAL_OK && now_ms == 0 && imu.outputs_fresh(0));
    now_ms = 20; assert(imu.outputs_fresh(now_ms));
    assert(read(imu, frame(1, {1})) == HAL_ERROR && !imu.has_pending());
    assert(!imu.outputs_fresh(now_ms) && imu.output_quat[3] == 0);

    // Reinitialization clears output validity and restarts sequence numbers.
    clear_bus(); std::swap(boot_packets[1], boot_packets[2]);
    assert(imu.init(100) == HAL_OK && imu.seqNum == 0 && !imu.outputs_fresh(now_ms));
    assert(imu.start(100) == HAL_OK && commands.size() == 3);
    clear_bus(); assert(imu.init(100) == HAL_OK);
    // Missing or disabled feature acknowledgements cannot report startup success.
    acknowledge = false; wrong_ack = false;
    const auto missing_ack_at = now_ms;
    assert(imu.start(20) == HAL_TIMEOUT && now_ms - missing_ack_at <= 22 && !wake_low);
    clear_bus(); assert(imu.init(100) == HAL_OK); disabled_ack = true;
    assert(imu.start(20) == HAL_TIMEOUT && !wake_low);
    clear_bus(); assert(imu.init(100) == HAL_OK); ack_id = 0x3F;
    assert(imu.start(20) == HAL_TIMEOUT && !wake_low);
    clear_bus(); assert(imu.init(100) == HAL_OK); ack_channel = 0;
    assert(imu.start(20) == HAL_TIMEOUT && !wake_low);
    clear_bus(); assert(imu.init(100) == HAL_OK); reset_on_write = true;
    assert(imu.start(100) == HAL_ERROR && !imu.has_pending());
    clear_bus(); assert(imu.init(100) == HAL_OK); wake_ready = false;
    imu.msgs_ready = 100;
    assert(imu.start(20) == HAL_TIMEOUT && !wake_low && !cs_active);
    clear_bus(); assert(imu.init(100) == HAL_OK); fail_after = 0;
    assert(imu.start(20) == HAL_TIMEOUT && !wake_low && !cs_active);

    // A missing device and an incomplete or failed boot are bounded and unusable.
    clear_bus(); auto_boot = false;
    const auto missing_at = now_ms;
    assert(imu.init(20) == HAL_TIMEOUT && now_ms - missing_at <= 24);
    assert(!imu.has_pending() && imu.start(10) == HAL_ERROR);
    clear_bus(); boot_packets = {frame(0, Bytes(10)), init_response()};
    assert(imu.init(20) == HAL_TIMEOUT && !imu.has_pending());
    clear_bus(); boot_packets = {frame(0, Bytes(10)), frame(1, {1}), init_response(1)};
    assert(imu.init(100) == HAL_ERROR && !imu.has_pending());
    clear_bus(); fail_after = 0;
    assert(imu.init(20) == HAL_TIMEOUT && !cs_active);
    clear_bus(); fail_after = 2; // Boot payload failure after the clock priming.
    assert(imu.init(100) == HAL_TIMEOUT && !imu.has_pending() && !cs_active);
    clear_bus(); assert(imu.init(100) == HAL_OK && imu.start(100) == HAL_OK);
}
