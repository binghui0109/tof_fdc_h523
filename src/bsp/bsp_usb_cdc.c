#include "bsp_usb_cdc.h"
#include "stdint.h"
#include "usb.h"

static USB_CDC_RX_CB cb;
uint16_t tx_free = 1;

void bsp_cdc_init(void)
{
    // while(hUsbDeviceFS.pClassData == NULL);
}

// void bsp_cdc_set_rx_cb(USB_CDC_RX_CB callback)
// {
//     cb = callback;
// }

uint16_t is_tx_free(void)
{
    return tx_free;
}

void tx_cplt_cb(void)
{
    tx_free = 1; // mark tx as free
}

// void cdc_rx_handler(uint8_t *buf, uint32_t len)
// {
//     cb(buf, len);
// }

void bsp_cdc_tx_data(uint8_t *buf, uint16_t len)
{
    tx_free = 0; // set tx busy flag
    uint8_t err = 0;
    if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != NULL)
    {
        err = CDC_Transmit(buf, len);
    }
    if (err == USBD_FAIL)
    {
        while (1)
            ;
    }
}
