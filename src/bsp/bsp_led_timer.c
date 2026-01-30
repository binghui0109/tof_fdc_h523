#include "bsp_led_timer.h"
#include "tim.h"

static bsp_led_timer_cb_t cb;

void bsp_led_timer_set_cb(bsp_led_timer_cb_t callback)
{
    cb = callback;
}

__attribute__((used)) void bsp_led_timer_cb(TIM_HandleTypeDef *htim)
{
    (void)htim;
    if (cb != NULL)
    {
        cb();
    }
}

void bsp_led_timer_start(void)
{
    HAL_TIM_RegisterCallback(&htim6, HAL_TIM_PERIOD_ELAPSED_CB_ID, bsp_led_timer_cb);
    HAL_TIM_Base_Start_IT(&htim6);
}

void bsp_led_timer_stop(void)
{
    HAL_TIM_UnRegisterCallback(&htim6, HAL_TIM_PERIOD_ELAPSED_CB_ID);
    HAL_TIM_Base_Stop_IT(&htim6);
}
