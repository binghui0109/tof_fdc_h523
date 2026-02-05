#include <string.h>
#include <math.h>
#include "background.h"
#define BG_DEFAULT_FRAMES 100U
#define MIN_VALID_SAMPLES 1U

static bg_info_t bg = {0};

static bool s_collecting = true;
static uint16_t s_collected_frames = 0U;

static uint32_t s_sum[TOF_ROWS][TOF_COLS];
static uint32_t s_sum_sq[TOF_ROWS][TOF_COLS];
static uint16_t s_valid_count[TOF_ROWS][TOF_COLS];
static uint32_t s_max_sum = 0U;

static void bg_compute(void)
{
    for (uint8_t row = 0U; row < TOF_ROWS; row++) {
        for (uint8_t col = 0U; col < TOF_COLS; col++) {
            uint16_t cnt = s_valid_count[row][col];
            if (cnt >= MIN_VALID_SAMPLES) {
                uint32_t mean = s_sum[row][col] / cnt;
                uint32_t mean_sq = s_sum_sq[row][col] / cnt;
                uint32_t sq_mean = mean * mean;
                uint32_t var = (mean_sq > sq_mean) ? (mean_sq - sq_mean) : 0U;
                bg.mean[row][col] = mean;
                bg.std[row][col] = (uint32_t)(sqrt((float)var));
            } else {
                bg.std[row][col] = 0U;
                bg.mean[row][col] = 4000U;
            }
        }
    }
    bg.max = (s_collected_frames > 0U) ? (uint16_t)(s_max_sum / s_collected_frames) : 0U;
}

void bg_reset(void)
{
    s_collected_frames = 0U;
    s_max_sum = 0U;
    s_collecting = true;

    memset(&bg, 0, sizeof(bg));
    memset(s_sum, 0, sizeof(s_sum));
    memset(s_sum_sq, 0, sizeof(s_sum_sq));
    memset(s_valid_count, 0, sizeof(s_valid_count));
}

void bg_collect(const VL53L5CX_ResultsData *tof_result)
{
    uint16_t frame_max = 0U;

    for (uint8_t row = 0U; row < TOF_ROWS; row++) 
    {
        for (uint8_t col = 0U; col < TOF_COLS; col++) 
        {
            uint8_t idx = (uint8_t)(row * TOF_COLS + col);
            uint8_t status = tof_result->target_status[idx];

            if (status == 255U) 
            {
                continue;
            }
            uint16_t value = tof_result->distance_mm[idx];
            s_valid_count[row][col]++;
            s_sum[row][col] += value;
            s_sum_sq[row][col] += (uint32_t)value * (uint32_t)value;

            if (((status == 5U) || (status == 9U)) && (value > frame_max)) 
            {
                frame_max = value;
            }
        }
    }

    s_max_sum += frame_max;
}

bool is_bg_collecting(void)
{
    return s_collecting;
}

const bg_info_t *bg_get_info(void)
{
    return &bg;
}

bool bg_update(const VL53L5CX_ResultsData *tof_result)
{
    if (s_collecting) 
    {
        bg_collect(tof_result);
        s_collected_frames++;
        if (s_collected_frames == BG_DEFAULT_FRAMES) 
        {
            bg_compute();
            s_collecting = false;
        }
    }
    return s_collecting;
}
