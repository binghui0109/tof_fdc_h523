#ifndef BSP_SERIAL_H
#define BSP_SERIAL_H

#include "stdint.h"

typedef void (*SERIAL_RX_CB)(uint8_t *, uint32_t);

void bsp_serial_init(void);
void bsp_serial_set_rx_cb(SERIAL_RX_CB callback);
void bsp_serial_tx_data(uint8_t *buf, uint16_t len);
uint16_t bsp_serial_tx_status(void);

#endif // BSP_SERIAL_H
