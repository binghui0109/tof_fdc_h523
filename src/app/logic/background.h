#ifndef __BACKGROUND_H
#define __BACKGROUND_H

#include <stdbool.h>
#include <stdint.h>

#include "tof_types.h"
#include "vl53l5cx_api.h"

typedef struct {
    uint32_t mean[TOF_ROWS][TOF_COLS];
    uint32_t std[TOF_ROWS][TOF_COLS];
    uint16_t max;
} bg_info_t;

void bg_reset(void);
const bg_info_t *bg_get_info(void);
bool bg_update(const VL53L5CX_ResultsData *tof_result);
#endif
