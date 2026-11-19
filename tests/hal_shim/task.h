#pragma once
#include "FreeRTOS.h"
TaskHandle_t xTaskGetCurrentTaskHandle();
uint32_t ulTaskNotifyTakeIndexed(UBaseType_t, BaseType_t, TickType_t);
void vTaskNotifyGiveIndexedFromISR(TaskHandle_t, UBaseType_t, BaseType_t *);
