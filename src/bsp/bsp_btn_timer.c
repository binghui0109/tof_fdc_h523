#include "bsp_btn_timer.h"
#include "tim.h"

static bsp_btn_timer_cb_t cb;

void bsp_btn_timer_set_cb(bsp_btn_timer_cb_t callback)
{
    cb = callback;
}

__attribute__((used)) void bsp_btn_timer_cb(TIM_HandleTypeDef *htim)
{
    (void)htim;
    cb();
}

void bsp_btn_timer_start(void)
{
    HAL_TIM_RegisterCallback(&htim2, HAL_TIM_PERIOD_ELAPSED_CB_ID, bsp_btn_timer_cb);
    HAL_TIM_Base_Start_IT(&htim2);
}

void bsp_btn_timer_stop(void)
{
    HAL_TIM_UnRegisterCallback(&htim2, HAL_TIM_PERIOD_ELAPSED_CB_ID);
    HAL_TIM_Base_Start_IT(&htim2);
}
