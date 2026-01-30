#include "bsp_gpio.h"

#include <assert.h>
#include <stdbool.h>

#include "bsp_led_timer.h"

#define LED_COUNT (3)
#define LED_TIME_BASE (10)                     // in ms
#define LED_BLINK_PERIOD (500 / LED_TIME_BASE) // 500ms
#define LED_CHASER_SPEED (200 / LED_TIME_BASE)

typedef enum LED_STATE
{
    ON,
    OFF,
    BLINK,
    CHASER
} led_state_t;

typedef struct
{
    bsp_gpio_t gpio;
    led_state_t state;
} led_t;

static uint32_t led_count = 0;
static led_t led_array[LED_COUNT] = {0};
static uint8_t chasing = 0;

void led_sm(void);
void register_led(led_t led);

void register_led(led_t led)
{
    if (led_count >= LED_COUNT)
    {
        assert(false); // you registered more led than it is on the board
        return;
    }
    led_array[led_count] = led;
    led_count++;
}

void led_timebase_cb(void)
{
    led_sm();
}

void led_init(void)
{
    led_t led1 = {
        .gpio = led1_gpio,
        .state = OFF,
    };

    led_t led2 = {
        .gpio = led2_gpio,
        .state = OFF,
    };
    led_t led3 = {
        .gpio = led3_gpio,
        .state = OFF,
    };

    register_led(led1);
    register_led(led2);
    register_led(led3);

    bsp_led_timer_set_cb(led_sm);
    bsp_led_timer_start();
}

void led_on(uint16_t led)
{
    if (led < led_count)
    {
        led_array[led].state = ON;
    }
}

void led_off(uint16_t led)
{
    if (led < led_count)
    {
        led_array[led].state = OFF;
    }
}

void led_blink(uint16_t led)
{
    if (led < led_count)
    {
        led_array[led].state = BLINK;
    }
}

void led_chase_enable(void)
{
    if (!chasing)
    {
        for (uint32_t i = 0; i < led_count; i++)
        {
            led_array[i].state = CHASER;
            bsp_gpio_reset(led_array[i].gpio);
        }
        chasing = 1;
    }
}

void led_chase_disable(void)
{
    chasing = 0;
    for (uint32_t i = 0; i < led_count; i++)
    {
        led_array[i].state = OFF;
        bsp_gpio_reset(led_array[i].gpio);
    }
}

void led_sm(void)
{
    static uint32_t led_tick = 0;
    static uint32_t next_chaser = 0;
    led_tick++;

    if (led_count == 0U)
    {
        return;
    }

    for (uint32_t i = 0; i < led_count; i++)
    {
        switch (led_array[i].state)
        {
        case ON:
            bsp_gpio_set(led_array[i].gpio);
            break;

        case OFF:
            bsp_gpio_reset(led_array[i].gpio);
            break;

        case BLINK:
            if (led_tick % LED_BLINK_PERIOD == 0)
            {
                bsp_gpio_toggle(led_array[i].gpio);
            }
            break;

        case CHASER:
            if (led_tick % LED_CHASER_SPEED == 0)
            {
                if (i == next_chaser)
                {
                    bsp_gpio_set(led_array[i].gpio);
                }
                else
                {
                    bsp_gpio_reset(led_array[i].gpio);
                }
                if ((led_count > 0U) && (i == (led_count - 1U)))
                {
                    next_chaser = (next_chaser + 1U) % led_count;
                }
            }

            break;
        }
    }
}
