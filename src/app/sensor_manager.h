#ifndef __SENSOR_MANAGER_H
#define __SENSOR_MANAGER_H

#include "stdint.h"
#include "vl53l5cx_api.h"

typedef void (*TOF_SUB_PTR) (VL53L5CX_ResultsData* tof_val, uint16_t size);

void sensor_init(void);
void sensor_get_data(void);
void subscribe_tof_data(TOF_SUB_PTR sub);
VL53L5CX_ResultsData* get_tof_buf(void);
void unsubscribe_tof_data(TOF_SUB_PTR sub);
#endif