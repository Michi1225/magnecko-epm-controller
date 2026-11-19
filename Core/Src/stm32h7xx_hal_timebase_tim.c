/* HAL milliseconds use TIM7 independently of the FreeRTOS SysTick. */
#include "stm32h7xx_hal.h"
#include "app_runtime.h"

static TIM_HandleTypeDef hal_tick_timer;

HAL_StatusTypeDef HAL_InitTick(uint32_t priority) {
    (void)priority;
    __HAL_RCC_TIM7_CLK_ENABLE();
    if (hal_tick_timer.Instance != NULL) HAL_TIM_Base_Stop_IT(&hal_tick_timer);
    hal_tick_timer.Instance = TIM7;
    RCC_ClkInitTypeDef clocks;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks, &latency);
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq();
    if (clocks.APB1CLKDivider != RCC_APB1_DIV1) timer_clock *= 2U;
    hal_tick_timer.Init.Prescaler = timer_clock / 1000000U - 1U;
    hal_tick_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    hal_tick_timer.Init.Period = 999;
    hal_tick_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    hal_tick_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&hal_tick_timer) != HAL_OK) return HAL_ERROR;
    /* Above the RTOS syscall ceiling; this handler never calls the RTOS. */
    HAL_NVIC_SetPriority(TIM7_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
    uwTickPrio = 4;
    __HAL_TIM_CLEAR_FLAG(&hal_tick_timer, TIM_FLAG_UPDATE);
    return HAL_TIM_Base_Start_IT(&hal_tick_timer);
}

void HAL_SuspendTick(void) { __HAL_TIM_DISABLE_IT(&hal_tick_timer, TIM_IT_UPDATE); }
void HAL_ResumeTick(void) { __HAL_TIM_ENABLE_IT(&hal_tick_timer, TIM_IT_UPDATE); }

void TIM7_IRQHandler(void) {
    if (__HAL_TIM_GET_FLAG(&hal_tick_timer, TIM_FLAG_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_IT(&hal_tick_timer, TIM_IT_UPDATE);
        HAL_IncTick();
        App_CheckControlHeartbeat();
    }
}
