#include "bsp_usb_cdc.h"

// wrapper around usbcdc

void bsp_serial_init(void)
{
    bsp_cdc_init();
}

// void bsp_serial_set_rx_cb(USB_CDC_RX_CB callback)
// {
//     bsp_cdc_set_rx_cb(callback);
// }

void bsp_serial_tx_data(uint8_t *buf, uint16_t len)
{
    bsp_cdc_tx_data(buf, len);
}

uint16_t bsp_serial_tx_status(void)
{
    return is_tx_free();
}
