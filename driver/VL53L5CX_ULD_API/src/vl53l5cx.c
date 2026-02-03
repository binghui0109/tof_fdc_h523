#include "main.h"
#include "bsp_gpio.h"
#include "vl53l5cx_api.h"
#include "vl53l5cx_plugin_xtalk.h"
#include "vl53l5cx.h"

static VL53L5CX_Configuration Dev;
static VL53L5CX_ResultsData Results;
volatile uint8_t vl53l5_data_ready = 0;

void vl53l5_cb(void);

VL53L5CX_ResultsData* vl53l5_tof_init(void) 
{
    uint8_t status, isAlive;
    HAL_GPIO_WritePin(VL53_xshut_GPIO_Port, VL53_xshut_Pin, GPIO_PIN_SET);
    Dev.platform.address = VL53L5CX_DEFAULT_I2C_ADDRESS;

	do 
    {
		HAL_Delay(10);
		/* Check if there is a VL53L5CX sensor connected */
		status = vl53l5cx_is_alive(&Dev, &isAlive);
	} while (!isAlive || status);

	/* Init VL53L5CX sensor */
	status = vl53l5cx_init(&Dev);
//  status = vl53l5cx_motion_indicator_init(&Dev, &motion_config, VL53L5CX_RESOLUTION_8X8);
	if (status) 
    {
		while (1);
	}
	status = vl53l5cx_set_xtalk_margin(&Dev, 50);
	status = vl53l5cx_set_target_order(&Dev, VL53L5CX_TARGET_ORDER_STRONGEST);
	status = vl53l5cx_set_sharpener_percent(&Dev, 5);
	status = vl53l5cx_set_ranging_frequency_hz(&Dev, DISTANCE_ODR);
	status = vl53l5cx_set_resolution(&Dev, FRAME_RESOLUTION);
	status = vl53l5cx_set_ranging_mode(&Dev, VL53L5CX_RANGING_MODE_CONTINUOUS);
	status = vl53l5cx_start_ranging(&Dev);
    bsp_gpio_exit_register_cb(vl53l5_cb , VL53_INT_Pin);
    return &Results;
}

bool vl53l5_update_data(void)
{
    if (vl53l5_data_ready)
    {
        vl53l5_data_ready = 0;
        vl53l5cx_get_ranging_data(&Dev, &Results);
        return true;
    }
	else
    {
        return false;
    }
}

void vl53l5_cb(void)
{
    vl53l5_data_ready = 1;
}