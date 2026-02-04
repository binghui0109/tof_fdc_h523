#ifndef BSP_USB_CDC_H
#define BSP_USB_CDC_H
#include "stdint.h"

typedef void (*USB_CDC_RX_CB)(uint8_t *, uint32_t);

void bsp_cdc_init(void);
void bsp_cdc_set_rx_cb(USB_CDC_RX_CB callback);
void bsp_cdc_tx_data(uint8_t *buf, uint16_t len);
uint16_t is_tx_free(void);

void tx_cplt_cb(void);
void cdc_rx_handler(uint8_t *buf, uint32_t len);

#endif //BSP_USB_CDC_H
