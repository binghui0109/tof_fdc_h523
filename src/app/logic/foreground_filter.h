#ifndef FOREGROUND_FILTER_H
#define FOREGROUND_FILTER_H

#include <stdint.h>

#include "background.h"
#include "tof_types.h"
#include "vl53l5cx_api.h"

void fg_filter_apply(const VL53L5CX_ResultsData *frame,
                     const bg_info_t *bg_info,
                     uint16_t filtered_mm[TOF_ROWS][TOF_COLS],
                     uint16_t pixel_distance_bg_mm[TOF_ROWS][TOF_COLS]);

#endif
