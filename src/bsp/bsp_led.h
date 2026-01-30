#ifndef BSP_LED_H
#define BSP_LED_H

#include "stdint.h"

enum LEDS{
	LED1 = 0,
	LED2,
	LED3,
};

/* user led functions */
void led_init(void);
void led_on(uint16_t led);
void led_off(uint16_t led);
void led_blink(uint16_t led);
void led_chase_enable(void);
void led_chase_disable(void);

#endif //BSP_LED_H

