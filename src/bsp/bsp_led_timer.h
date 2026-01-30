#ifndef BSP_TIMER_H
#define BSP_TIMER_H

typedef void (*bsp_led_timer_cb_t)(void);

void bsp_led_timer_set_cb(bsp_led_timer_cb_t callback);
void bsp_led_timer_start(void);
void bsp_led_timer_stop(void);

#endif // BSP_TIMER_H
