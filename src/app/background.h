#ifndef __BACKGROUND_H
#define __BACKGROUND_H

#include <stdbool.h>
#include <stdint.h>

#include "vl53l5cx_api.h"

#define ROWS 8U
#define COLS 8U

typedef struct {
    uint32_t mean[ROWS][COLS];
    uint32_t std[ROWS][COLS];
    uint16_t max;
} bg_info_t;

extern bg_info_t bg;

void bg_start(void);
bool bg_collect(const VL53L5CX_ResultsData *tof_result);
bool is_bg_collecting(void);
const bg_info_t *bg_get_info(void);

#endif
