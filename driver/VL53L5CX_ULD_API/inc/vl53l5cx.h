#ifndef __VL53L5CX_H
#define __VL53L5CX_H

#include "stdint.h"
#include "stdbool.h"
#include "vl53l5cx_api.h"

#define FRAME_RESOLUTION  	64       /* 16 if resolution is VL53L5CX_RESOLUTION_4X4 else, 64 if VL53L5CX_RESOLUTION_8X8 */
#define FRAMES              	1      /* Should be between 1 & 32 */
#define DISTANCE_ODR        	8      /* Should be between 1 -> 60Hz for VL53L5CX_RESOLUTION_4X4 and 1 -> 15Hz for VL53L5CX_RESOLUTION_8X8 */

VL53L5CX_ResultsData* vl53l5_tof_init(void);
bool vl53l5_update_data();
#endif
