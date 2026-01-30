#ifndef BSP_TIMER_H
#define BSP_TIMER_H


typedef void (*bsp_btn_timer_cb_t)(void);

void bsp_btn_timer_set_cb(bsp_btn_timer_cb_t callback);
void bsp_btn_timer_start();
void bsp_btn_timer_stop();

#endif // BSP_TIMER_H
