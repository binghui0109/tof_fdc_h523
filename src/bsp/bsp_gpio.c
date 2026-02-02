#include "bsp_gpio.h"
#include "main.h"

#define NUM_GPIO_EXIT 4

typedef struct 
{
    uint16_t exit_pin;
    gpio_exit_cb_t exit_cb;
} gpio_exit_handlers_t;

static gpio_exit_handlers_t gpio_exit_handlers[NUM_GPIO_EXIT];
static uint8_t gpio_exit_count = 0;

bsp_gpio_t led1_gpio =
    {
        .port = LED1_GPIO_Port,
        .pin = LED1_Pin,
};

bsp_gpio_t led2_gpio =
    {
        .port = LED2_GPIO_Port,
        .pin = LED2_Pin,
};

bsp_gpio_t led3_gpio =
    {
        .port = LED3_GPIO_Port,
        .pin = LED3_Pin,
};

bsp_gpio_t btn1_gpio =
    {
        .port = USER_BTN_GPIO_Port,
        .pin = USER_BTN_Pin,
};

void bsp_gpio_set(bsp_gpio_t gpio)
{
    HAL_GPIO_WritePin(gpio.port, gpio.pin, GPIO_PIN_SET);
}

void bsp_gpio_reset(bsp_gpio_t gpio)
{
    HAL_GPIO_WritePin(gpio.port, gpio.pin, GPIO_PIN_RESET);
}

void bsp_gpio_toggle(bsp_gpio_t gpio)
{
    HAL_GPIO_TogglePin(gpio.port, gpio.pin);
}

uint16_t bsp_read_gpio(bsp_gpio_t gpio)
{
    if (HAL_GPIO_ReadPin(gpio.port, gpio.pin) == GPIO_PIN_RESET)
        return 1;
    else
        return 0;
}

void bsp_gpio_exit_register_cb(gpio_exit_cb_t cb, uint16_t pin)
{
    if (gpio_exit_count >= NUM_GPIO_EXIT)
    {
        return;
    }
    gpio_exit_handlers[gpio_exit_count].exit_cb = cb;
    gpio_exit_handlers[gpio_exit_count].exit_pin = pin;
    gpio_exit_count++;
}


void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
	for(uint8_t i = 0; i < gpio_exit_count; i++)
    {
        if(gpio_exit_handlers[i].exit_pin == GPIO_Pin && gpio_exit_handlers[i].exit_cb != NULL)
        {
            gpio_exit_handlers[i].exit_cb();
        }
    }
}
