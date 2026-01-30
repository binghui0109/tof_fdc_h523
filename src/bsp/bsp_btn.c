#include "bsp_btn.h"
#include "bsp_btn_timer.h"
#include "bsp_gpio.h"
#include "stddef.h"
#include "stm32h5xx_hal.h"
typedef enum
{
    RISING_EDGE = 0,
    DEBOUNCING,
    FALLING_EDGE,
} btn_state_t;

typedef struct
{
    bsp_gpio_t btn; // The GPIO pin for the button
    btn_state_t state;
    btn_cb_t rising_edge_callback;
    btn_cb_t falling_edge_callback;
} ButtonHandler_t;

uint16_t btn_count = 0;
ButtonHandler_t button_handlers[NUM_BUTTONS] = {0};

void check_btn(void);

void register_btn(uint16_t btn, ButtonHandler_t btn_hdlr)
{
    if (btn_count < NUM_BUTTONS)
    {
        button_handlers[btn] = btn_hdlr;
        btn_count++;
    }
    else
    {
        while (1)
            ;
    }
}

void reg_btn_pos_edge_cb(uint16_t btn, btn_cb_t cb)
{
    button_handlers[btn].rising_edge_callback = cb;
}

void reg_btn_neg_edge_cb(uint16_t btn, btn_cb_t cb)
{
    button_handlers[btn].falling_edge_callback = cb;
}

void btn_init(void)
{
    ButtonHandler_t btn1_hdlr = {
        .btn = btn1_gpio, // The GPIO pin for the button
        .state = RISING_EDGE,
        .rising_edge_callback = NULL,
        .falling_edge_callback = NULL};

    register_btn(BTN1, btn1_hdlr);

    bsp_btn_timer_set_cb(check_btn);
    // HAL_Delay(1000);
    bsp_btn_timer_start();
}

void check_btn(void)
{
    for (uint16_t i = 0; i < NUM_BUTTONS; i++)
    {
        uint16_t pin_state = bsp_read_gpio(button_handlers[i].btn);

        switch (button_handlers[i].state)
        {
        case RISING_EDGE:
            if (pin_state == 1)
            {
                button_handlers[i].state = DEBOUNCING;
            }
            break;
        case DEBOUNCING:
            if (pin_state == 1)
            {
                button_handlers[i].state = FALLING_EDGE;
                if (button_handlers[i].rising_edge_callback == NULL)
                    break;
                button_handlers[i].rising_edge_callback();
            }
            break;
        case FALLING_EDGE:
            if (pin_state == 0)
            {
                button_handlers[i].state = RISING_EDGE;
                if (button_handlers[i].falling_edge_callback == NULL)
                    break;
                button_handlers[i].falling_edge_callback();
            }
            break;
        }
    }
}
