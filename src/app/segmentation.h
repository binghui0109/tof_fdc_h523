#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include <stdint.h>

#include "tof_types.h"

void seg_clear_labels(uint8_t labels[TOF_ROWS][TOF_COLS]);
uint8_t seg_label_components(const uint16_t frame_mm[TOF_ROWS][TOF_COLS],
                             uint8_t labels[TOF_ROWS][TOF_COLS],
                             tof_component_t *components,
                             uint8_t max_components,
                             uint8_t min_component_size);

#endif
