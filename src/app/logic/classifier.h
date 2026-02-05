#ifndef CLASSIFIER_H
#define CLASSIFIER_H

#include <stdint.h>

#include "tof_types.h"

void classifier_init(void);
void classifier_reset(void);

void preprocess_frame_data(const uint16_t filtered_frame_mm[TOF_ROWS][TOF_COLS],
                        const uint16_t pixel_distance_bg_mm[TOF_ROWS][TOF_COLS],
                        float ai_out[TOF_NUM_CLASSES]);
uint8_t ai_output_moving_average(float ai_out[TOF_NUM_CLASSES]);

#endif
