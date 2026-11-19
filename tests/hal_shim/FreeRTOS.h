#pragma once
#include <cassert>
#include <cstdint>
using UBaseType_t = unsigned;
using BaseType_t = int;
using TickType_t = uint32_t;
using TaskHandle_t = void *;
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define configASSERT(c) assert(c)
#define portYIELD_FROM_ISR(wake) ((void)(wake))
extern unsigned mock_critical_depth;
#define taskENTER_CRITICAL() (++mock_critical_depth)
#define taskEXIT_CRITICAL() (--mock_critical_depth)
