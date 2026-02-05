#include "foreground_filter.h"

#include <stdbool.h>
#include <stddef.h>

#define FG_MAX_DISTANCE_MM 4000U
#define FG_STD_GAIN 2U
#define FG_MIN_DELTA_MM 80U
#define FG_STATUS_VALID_RANGE 5U
#define FG_STATUS_VALID_LARGE_PULSE 9U

static bool fg_is_status_usable(uint8_t status)
{
    return (status == FG_STATUS_VALID_RANGE) || (status == FG_STATUS_VALID_LARGE_PULSE);
}

static uint32_t fg_threshold_mm(uint32_t bg_std_mm)
{
    uint32_t threshold_mm = bg_std_mm * FG_STD_GAIN;

    if (threshold_mm < FG_MIN_DELTA_MM) {
        threshold_mm = FG_MIN_DELTA_MM;
    }

    return threshold_mm;
}

static bool fg_is_foreground_pixel(uint16_t distance_mm,
                                   uint8_t status,
                                   uint32_t bg_mean_mm,
                                   uint32_t threshold_mm)
{
    int32_t delta_mm;

    if ((distance_mm > FG_MAX_DISTANCE_MM) || !fg_is_status_usable(status)) {
        return false;
    }

    delta_mm = (int32_t)bg_mean_mm - (int32_t)distance_mm;
    return delta_mm > (int32_t)threshold_mm;
}

void fg_filter_apply(const VL53L5CX_ResultsData *frame,
                     const bg_info_t *bg_info,
                     uint16_t filtered_mm[TOF_ROWS][TOF_COLS],
                     uint16_t pixel_distance_bg_mm[TOF_ROWS][TOF_COLS])
{
    if ((frame == NULL) || (bg_info == NULL) || (filtered_mm == NULL) || (pixel_distance_bg_mm == NULL)) {
        return;
    }

    for (uint8_t row = 0U; row < TOF_ROWS; row++) {
        for (uint8_t col = 0U; col < TOF_COLS; col++) {
            uint8_t zone_idx = (uint8_t)((row * TOF_COLS) + col);
            uint16_t target_idx = (uint16_t)(zone_idx * VL53L5CX_NB_TARGET_PER_ZONE);
            uint16_t distance_mm = frame->distance_mm[target_idx];
            uint8_t status = frame->target_status[target_idx];
            uint32_t bg_mean_mm = bg_info->mean[row][col];
            uint32_t threshold_mm = fg_threshold_mm(bg_info->std[row][col]);

            if (!fg_is_foreground_pixel(distance_mm, status, bg_mean_mm, threshold_mm)) {
                filtered_mm[row][col] = 0U;
                pixel_distance_bg_mm[row][col] = 0U;
                continue;
            }

            filtered_mm[row][col] = distance_mm;
            pixel_distance_bg_mm[row][col] = (bg_info->max > distance_mm)
                                                 ? (uint16_t)(bg_info->max - distance_mm)
                                                 : 0U;
        }
    }
}
