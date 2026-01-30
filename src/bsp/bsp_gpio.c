#include "bsp_gpio.h"
#include "main.h"

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
