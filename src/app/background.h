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

typedef enum {
    BG_STATUS_INVALID = 0,
    BG_STATUS_COLLECTING,
    BG_STATUS_READY_JUST_FINISHED,
    BG_STATUS_READY,
} bg_status_t;

void bg_reset(void);
bool bg_collect(const VL53L5CX_ResultsData *tof_result);
bool is_bg_collecting(void);
const bg_info_t *bg_get_info(void);
bg_status_t bg_update(const VL53L5CX_ResultsData *tof_result);
#endif
