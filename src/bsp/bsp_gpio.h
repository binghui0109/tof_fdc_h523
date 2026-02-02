#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "stdint.h"
#include "stm32h523xx.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} bsp_gpio_t;

typedef enum {
    BSP_GPIO_SET = 0U,
    BSP_GPIO_RESET
}BSP_GPIO_STATE;

typedef void (*gpio_exit_cb_t)(void);

void bsp_gpio_exit_register_cb(gpio_exit_cb_t cb, uint16_t pin);

extern bsp_gpio_t led1_gpio;
extern bsp_gpio_t led2_gpio;
extern bsp_gpio_t led3_gpio;

extern bsp_gpio_t btn1_gpio;

void bsp_gpio_set(bsp_gpio_t gpio);
void bsp_gpio_reset(bsp_gpio_t gpio);
void bsp_gpio_toggle(bsp_gpio_t gpio);
uint16_t bsp_read_gpio(bsp_gpio_t gpio);

#endif //BSP_GPIO_H
