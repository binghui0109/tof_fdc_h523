#include "bsp_serial.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_btn.h"
#include "bsp_gpio.h"
uint8_t TxMessageBuffer[] = "MY USB IS WORKING! \r\n";
//   while(hUsbDeviceFS.pClassData == NULL);

void btn1_cb()
{
    bsp_serial_tx_data(TxMessageBuffer, sizeof(TxMessageBuffer));
}

void app_init(void)
{
    bsp_serial_init();
    led_init();
    btn_init();
    // reg_btn_pos_edge_cb(BTN1, btn1_cb); 
    bsp_gpio_exit_register_cb(btn1_cb , VL53_INT_Pin);
}

void app_main(void)
{
    // bsp_serial_tx_data(TxMessageBuffer, sizeof(TxMessageBuffer));
    // led_on(LED1);
    
    led_chase_enable();
    // HAL_Delay(500);    
}