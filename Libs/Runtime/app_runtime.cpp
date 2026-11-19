#include "app_runtime.h"
#include "FootController.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "tim.h"
#include <cstring>

extern FootController controller;
volatile AppDiagnostics app_diagnostics{};

namespace {
constexpr EventBits_t ready_bit = 1;
constexpr uint32_t tick_event = 1, button_event = 2, dead_time_event = 4;
constexpr uint32_t control_stall_ms = 100;
TaskHandle_t control_task, acquisition_task, imu_task, tof_task, services_task;
StaticTask_t task_storage[5], idle_storage;
StackType_t control_stack[2048], acquisition_stack[1024], imu_stack[1024];
StackType_t tof_stack[2048], services_stack[2048], idle_stack[configMINIMAL_STACK_SIZE];
StaticEventGroup_t ready_storage;
EventGroupHandle_t ready_group;
volatile bool ready = false;
volatile uint32_t release_cycles = 0;

template <typename T> class LatestSample {
    StaticQueue_t storage_{};
    alignas(4) uint8_t bytes_[sizeof(T)]{};
    QueueHandle_t queue_ = nullptr;
public:
    void init() { queue_ = xQueueCreateStatic(1, sizeof(T), bytes_, &storage_); configASSERT(queue_); }
    void publish(const T &sample) { (void)xQueueOverwrite(queue_, &sample); }
    bool receive(T &sample) { return xQueueReceive(queue_, &sample, 0) == pdTRUE; }
};
LatestSample<HallSample> hall_samples;
LatestSample<LdcSample> ldc_samples;
LatestSample<ImuSample> imu_samples;
LatestSample<TofSample> tof_samples;
LatestSample<ChargerSample> charger_samples;
LatestSample<uint32_t> led_requests;

void wait_ready() {
    (void)xEventGroupWaitBits(ready_group, ready_bit, pdFALSE, pdTRUE, portMAX_DELAY);
}

void signal(TaskHandle_t task, uint32_t event) {
    if (!ready) return;
    BaseType_t wake = pdFALSE;
    (void)xTaskNotifyFromISR(task, event, eSetBits, &wake);
    portYIELD_FROM_ISR(wake);
}

bool stale(bool valid, uint32_t timestamp, uint32_t now, uint32_t age) {
    return !valid || static_cast<uint32_t>(now - timestamp) > age;
}

void consume_samples() {
    hall_samples.receive(controller.hall_sample);
    ldc_samples.receive(controller.ldc_sample);
    imu_samples.receive(controller.imu_sample);
    tof_samples.receive(controller.tof_sample);
    charger_samples.receive(controller.charger_sample);
    const uint32_t now = HAL_GetTick();
    app_diagnostics.sensor_stale_mask =
        (stale(controller.hall_sample.valid, controller.hall_sample.timestamp_ms, now, 20) ? 1U : 0U) |
        (stale(controller.ldc_sample.valid, controller.ldc_sample.timestamp_ms, now, 20) ? 2U : 0U) |
        (stale(controller.imu_sample.valid, controller.imu_sample.timestamp_ms, now, 50) ? 4U : 0U) |
        (stale(controller.tof_sample.valid, controller.tof_sample.timestamp_ms, now, 100) ? 8U : 0U) |
        (controller.charger.initialized && stale(controller.charger_sample.valid,
            controller.charger_sample.timestamp_ms, now, 100) ? 16U : 0U);
    controller.controller_error_word.charger_oc_fault = controller.charger_sample.over_current;
    controller.controller_error_word.charger_ov_fault = controller.charger_sample.over_voltage;
    controller.controller_error_word.charger_wd_fault = controller.charger_sample.watchdog;
}

void control(void *) {
    wait_ready();
    uint32_t previous_release = 0;
    for (;;) {
        uint32_t events = 0;
        if (xTaskNotifyWait(0, UINT32_MAX, &events, pdMS_TO_TICKS(10)) != pdTRUE) continue;
        if (events & dead_time_event) controller.dead_time_active = false;
        if (events & button_event) {
            /* Preserve the physical button's independent release request and
               the existing pulse/temperature/lockout checks. No retry queue. */
            controller.requested_demagnetization = true;
            controller.requested_magnetization = false;
            controller.magnetize(MAGNETIZATION_TIME);
            controller.requested_demagnetization = controller.requested_magnetization = false;
        }
        if (!(events & tick_event)) continue;
        taskENTER_CRITICAL();
        const uint32_t releases = app_diagnostics.control_releases;
        const uint32_t released_at = release_cycles;
        taskEXIT_CRITICAL();
        const uint32_t elapsed = releases - previous_release;
        if (elapsed == 0) continue; // A release sampled early left a pending notification.
        if (elapsed > 1) app_diagnostics.missed_control_releases += elapsed - 1;
        previous_release = releases;
        const uint32_t start = DWT->CYCCNT;
        const uint32_t latency = start - released_at;
        if (latency > app_diagnostics.max_wake_latency_cycles)
            app_diagnostics.max_wake_latency_cycles = latency;
        consume_samples();
        /* Preserve the existing control -> EtherCAT phase order. Never burst
           through old releases: SOES's watchdog counts calls, not elapsed ms. */
        controller.runControl();
        controller.runCommunication();
        const uint32_t duration = DWT->CYCCNT - start;
        app_diagnostics.control_cycles = duration;
        if (duration > app_diagnostics.max_control_cycles)
            app_diagnostics.max_control_cycles = duration;
        app_diagnostics.control_heartbeat_ms = HAL_GetTick();
    }
}

void acquire(void *) {
    wait_ready();
    TMAG5273 *const halls[] = {&controller.hall0, &controller.hall1,
                             &controller.hall2, &controller.hall3};
    uint8_t next_ldc = 0;
    uint32_t ldc_times[4]{};
    LdcSample ldc_sample;
    for (;;) {
        /* TIM2 also releases acquisition. Completion waits yield to control. */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        HallSample sample;
        bool valid = true;
        for (int i = 3; i >= 0; --i) {
            halls[i]->read_magnitude();
            valid &= halls[i]->data_valid;
            sample.raw[i][0] = halls[i]->raw_bx;
            sample.raw[i][1] = halls[i]->raw_by;
            sample.raw[i][2] = halls[i]->raw_bz;
            sample.magnitude[i] = halls[i]->b_mag;
        }
        sample.valid = valid;
        sample.timestamp_ms = HAL_GetTick();
        hall_samples.publish(sample);

        auto &ldc = controller.ldc[next_ldc];
        ldc.read_data();
        if (ldc.data_valid) {
            ldc_sample.frequency[next_ldc] = ldc.rx_data.l_data;
            ldc_sample.rp[next_ldc] = ldc.rx_data.rp_data;
            ldc_times[next_ldc] = HAL_GetTick();
        } else ldc_times[next_ldc] = 0;
        next_ldc = (next_ldc + 1) % 4;
        ldc_sample.valid = true;
        ldc_sample.slip_votes = 0;
        for (int i = 0; i < 4; ++i) {
            ldc_sample.valid &= ldc_times[i] != 0 && HAL_GetTick() - ldc_times[i] <= 20;
            ldc_sample.slip_votes += controller.ldc[i].slip_detector.slip_detected() ? 1 : 0;
        }
        ldc_sample.timestamp_ms = HAL_GetTick();
        ldc_samples.publish(ldc_sample);
    }
}

void imu(void *) {
    wait_ready();
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        /* A bounded drain also handles a line that remains asserted between
           reports. update() consumes one complete transaction at a time. */
        for (unsigned i = 0; i < 8; ++i) {
            if (!controller.imu.has_pending()) break;
            if (controller.imu.update() != HAL_OK) break;
            ImuSample sample;
            std::memcpy(sample.acceleration, controller.imu.output_lin_accel, sizeof(sample.acceleration));
            std::memcpy(sample.gyro, controller.imu.output_gyro, sizeof(sample.gyro));
            std::memcpy(sample.quaternion, controller.imu.output_quat, sizeof(sample.quaternion));
            sample.timestamp_ms = HAL_GetTick();
            sample.valid = controller.imu.outputs_fresh(sample.timestamp_ms);
            imu_samples.publish(sample);
        }
    }
}

void tof(void *) {
    wait_ready();
    for (;;) {
        /* Poll the level on timeout as protection against a missed startup edge. */
        const bool notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20)) != 0;
        if (!notified && HAL_GPIO_ReadPin(TOF_INT_GPIO_Port, TOF_INT_Pin) != GPIO_PIN_RESET) continue;
        if (controller.tof.read_frame() != HAL_OK) continue;
        TofSample sample;
        for (int i = 0; i < 64; ++i) {
            sample.distance[i] = controller.tof.data_frame.pixel_data[i / 8][i % 8].distance;
            sample.snr[i] = controller.tof.data_frame.pixel_data[i / 8][i % 8].snr;
        }
        sample.timestamp_ms = HAL_GetTick();
        sample.valid = true;
        tof_samples.publish(sample);
    }
}

void services(void *) {
    controller.init();
    app_diagnostics.control_heartbeat_ms = HAL_GetTick();
    ready = true;
    (void)xEventGroupSetBits(ready_group, ready_bit);
    /* Begin periodic release only once queues, owners and sensors are ready. */
    __HAL_TIM_CLEAR_FLAG(TIM_CONTROL, TIM_FLAG_UPDATE);
    if (HAL_TIM_Base_Start_IT(TIM_CONTROL) != HAL_OK) App_Fatal();
    for (;;) {
        uint32_t animation;
        if (led_requests.receive(animation))
            controller.ledController.set_animation(static_cast<LED_Animation_t>(animation));
        if (controller.charger.initialized) {
            controller.charger.transmit_receive();
            ChargerSample sample;
            sample.valid = controller.charger.data_valid;
            sample.voltage = controller.charger.status.vout_10mV;
            sample.over_current = controller.charger.status.OC_fault;
            sample.over_voltage = controller.charger.status.OV_fault;
            sample.watchdog = controller.charger.status.WD_fault;
            sample.timestamp_ms = HAL_GetTick();
            charger_samples.publish(sample);
        }
        const TaskHandle_t tasks[] = {control_task, acquisition_task, imu_task, tof_task, services_task};
        for (unsigned i = 0; i < 5; ++i)
            app_diagnostics.stack_free_words[i] = uxTaskGetStackHighWaterMark(tasks[i]);
        App_Delay(10);
    }
}
}

extern "C" void App_Start(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    hall_samples.init(); ldc_samples.init(); imu_samples.init(); tof_samples.init();
    charger_samples.init(); led_requests.init();
    ready_group = xEventGroupCreateStatic(&ready_storage);
    configASSERT(ready_group);
    control_task = xTaskCreateStatic(control, "control", 2048, nullptr, 5, control_stack, &task_storage[0]);
    acquisition_task = xTaskCreateStatic(acquire, "acquisition", 1024, nullptr, 3, acquisition_stack, &task_storage[1]);
    imu_task = xTaskCreateStatic(imu, "imu", 1024, nullptr, 4, imu_stack, &task_storage[2]);
    tof_task = xTaskCreateStatic(tof, "tof", 2048, nullptr, 2, tof_stack, &task_storage[3]);
    services_task = xTaskCreateStatic(services, "services", 2048, nullptr, 1, services_stack, &task_storage[4]);
    configASSERT(control_task && acquisition_task && imu_task && tof_task && services_task);
    vTaskStartScheduler();
    App_Fatal();
}

extern "C" void App_Delay(uint32_t milliseconds) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) HAL_Delay(milliseconds);
    else vTaskDelay(pdMS_TO_TICKS(milliseconds) + 1U);
}
extern "C" int App_IsReady(void) { return ready; }
extern "C" void App_RequestLed(uint32_t animation) {
    if (!ready) controller.ledController.set_animation(static_cast<LED_Animation_t>(animation));
    else led_requests.publish(animation);
}
extern "C" void App_ControlTickISR(void) {
    if (!ready) return;
    ++app_diagnostics.control_releases;
    release_cycles = DWT->CYCCNT;
    BaseType_t wake = pdFALSE;
    (void)xTaskNotifyFromISR(control_task, tick_event, eSetBits, &wake);
    vTaskNotifyGiveFromISR(acquisition_task, &wake);
    portYIELD_FROM_ISR(wake);
}
extern "C" void App_ImuReadyISR(void) {
    controller.imu.on_interrupt();
    if (!ready) return;
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(imu_task, &wake);
    portYIELD_FROM_ISR(wake);
}
extern "C" void App_TofReadyISR(void) {
    if (!ready) return;
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(tof_task, &wake);
    portYIELD_FROM_ISR(wake);
}
extern "C" void App_ButtonHeldISR(void) { signal(control_task, button_event); }
extern "C" void App_DeadTimeISR(void) { signal(control_task, dead_time_event); }
extern "C" void App_CheckControlHeartbeat(void) {
    if (ready && HAL_GetTick() - app_diagnostics.control_heartbeat_ms > control_stall_ms) App_Fatal();
}
extern "C" void App_Fatal(void) { Error_Handler(); }
extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char *) { App_Fatal(); }
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
    configSTACK_DEPTH_TYPE *depth) {
    *tcb = &idle_storage; *stack = idle_stack; *depth = configMINIMAL_STACK_SIZE;
}
