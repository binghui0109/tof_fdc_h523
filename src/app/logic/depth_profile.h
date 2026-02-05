#ifndef DEPTH_PROFILE_H
#define DEPTH_PROFILE_H

#include <stdint.h>

#include "tof_types.h"

void depth_profile_generate(const uint16_t frame_mm[TOF_ROWS][TOF_COLS],
                            uint8_t labels[TOF_ROWS][TOF_COLS],
                            tof_component_t *components,
                            uint8_t *component_count,
                            uint8_t combined_profile[TOF_ROWS][TOF_COLS]);

#endif
