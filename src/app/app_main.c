#include "bsp_serial.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_btn.h"
#include "bsp_gpio.h"
#include "vl53l5cx_api.h"
uint8_t TxMessageBuffer[] = "MY USB IS WORKING! \r\n";
VL53L5CX_Configuration Dev;
uint8_t status, isAlive;
#ifndef FRAME_RESOLUTION
#define FRAME_RESOLUTION  	64       /* 16 if resolution is VL53L5CX_RESOLUTION_4X4 else, 64 if VL53L5CX_RESOLUTION_8X8 */
#endif
#ifndef FRAMES
#define FRAMES              	1      /* Should be between 1 & 32 */
#endif
#ifndef DISTANCE_ODR
#define DISTANCE_ODR        	8      /* Should be between 1 -> 60Hz for VL53L5CX_RESOLUTION_4X4 and 1 -> 15Hz for VL53L5CX_RESOLUTION_8X8 */
#endif
//   while(hUsbDeviceFS.pClassData == NULL);
void vl53l5cx_initialize() {
    HAL_GPIO_WritePin(VL53_xshut_GPIO_Port, VL53_xshut_Pin, GPIO_PIN_SET);
    Dev.platform.address = VL53L5CX_DEFAULT_I2C_ADDRESS;
	do {
		HAL_Delay(10);
		/* Check if there is a VL53L5CX sensor connected */
		status = vl53l5cx_is_alive(&Dev, &isAlive);
	} while (!isAlive || status);

	/* Init VL53L5CX sensor */
	status = vl53l5cx_init(&Dev);
//  status = vl53l5cx_motion_indicator_init(&Dev, &motion_config, VL53L5CX_RESOLUTION_8X8);
	if (status) {
		while (1)
			;
	}
	// status = vl53l5cx_set_xtalk_margin(&Dev, 50);
	status = vl53l5cx_set_target_order(&Dev, VL53L5CX_TARGET_ORDER_STRONGEST);
	status = vl53l5cx_set_sharpener_percent(&Dev, 5);
	status = vl53l5cx_set_ranging_frequency_hz(&Dev, DISTANCE_ODR);
	status = vl53l5cx_set_resolution(&Dev, FRAME_RESOLUTION);
	status = vl53l5cx_set_ranging_mode(&Dev, VL53L5CX_RANGING_MODE_CONTINUOUS);
	status = vl53l5cx_start_ranging(&Dev);

}

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
    vl53l5cx_initialize();
}

void app_main(void)
{
    // bsp_serial_tx_data(TxMessageBuffer, sizeof(TxMessageBuffer));
    // led_on(LED1);
    
    led_chase_enable();
    // HAL_Delay(500);    
}