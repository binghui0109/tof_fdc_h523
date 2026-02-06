#ifndef __SENSOR_MANAGER_H
#define __SENSOR_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "vl53l5cx_api.h"

void sensor_init(void);
bool sensor_get_data(const VL53L5CX_ResultsData **frame);

#endif
