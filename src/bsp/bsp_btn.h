#ifndef _BSP_BTN_H
#define _BSP_BTN_H

#include "bsp_gpio.h"

#define NUM_BUTTONS (1)

enum
{
    BTN1 = 0,
};

typedef void (*btn_cb_t)(void);

void reg_btn_pos_edge_cb(uint16_t btn, btn_cb_t cb);
void reg_btn_neg_edge_cb(uint16_t btn, btn_cb_t cb);
void ButtonHandler_EXTI_Callback(uint16_t GPIO_Pin);
void btn_init(void);

#endif /*_BSP_BTN_H*/
