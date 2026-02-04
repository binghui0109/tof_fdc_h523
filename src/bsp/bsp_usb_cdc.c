#include "bsp_usb_cdc.h"

#include <stddef.h>

#include "usb.h"

static USB_CDC_RX_CB s_rx_cb = NULL;
static volatile uint16_t tx_free = 1U;

void bsp_cdc_init(void)
{
    // while(hUsbDeviceFS.pClassData == NULL);
}

void bsp_cdc_set_rx_cb(USB_CDC_RX_CB callback)
{
    s_rx_cb = callback;
}

uint16_t is_tx_free(void)
{
    return tx_free;
}

void tx_cplt_cb(void)
{
    tx_free = 1U;
}

void cdc_rx_handler(uint8_t *buf, uint32_t len)
{
    if (s_rx_cb != NULL) {
        s_rx_cb(buf, len);
    }
}

void bsp_cdc_tx_data(uint8_t *buf, uint16_t len)
{
    uint8_t err;

    if ((buf == NULL) || (len == 0U)) {
        return;
    }
    if (tx_free == 0U) {
        return;
    }
    if ((hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) || (hUsbDeviceFS.pClassData == NULL)) {
        return;
    }

    tx_free = 0U;
    err = CDC_Transmit(buf, len);
    if (err != USBD_OK) {
        tx_free = 1U;
    }
}
