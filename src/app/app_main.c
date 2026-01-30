#include "bsp_serial.h"
#include "main.h"

uint8_t TxMessageBuffer[] = "MY USB IS WORKING! \r\n";
//   while(hUsbDeviceFS.pClassData == NULL);

void app_init(void)
{
    bsp_serial_init();
}

void app_main(void)
{
    bsp_serial_tx_data(TxMessageBuffer, sizeof(TxMessageBuffer));
    HAL_Delay(500);    
}