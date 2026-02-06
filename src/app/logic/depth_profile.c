#include "depth_profile.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "segmentation.h"

static float depth_profile_regional_scale(const tof_component_t *comp, int row)
{
    if ((row > 3) || (comp->second_max_distance_mm <= comp->min_distance_mm)) {
        return 1.0f;
    }

    float denom = (float)comp->second_max_distance_mm - (float)comp->min_distance_mm;
    if (denom <= 0.0f) {
        return 1.0f;
    }

    float scale = 300.0f / denom;
    return (scale > 1.0f) ? 1.0f : scale;
}

static void depth_profile_split_single_component(const uint8_t combined_profile[TOF_ROWS][TOF_COLS],
                                                 uint8_t labels[TOF_ROWS][TOF_COLS],
                                                 tof_component_t *components,
                                                 uint8_t *component_count)
{
    uint16_t head_frame[TOF_ROWS][TOF_COLS] = {0};
    uint16_t body_frame[TOF_ROWS][TOF_COLS] = {0};

    seg_clear_labels(labels);
    *component_count = 0U;

    for (uint8_t row = 0U; row < TOF_ROWS; row++) {
        for (uint8_t col = 0U; col < TOF_COLS; col++) {
            if (combined_profile[row][col] == 2U) {
                head_frame[row][col] = 1U;
            }
        }
    }
    *component_count = seg_label_components(head_frame,
                                            labels,
                                            components,
                                            TOF_MAX_COMPONENTS,
                                            2U);

    if (*component_count > 1U) {
        return;
    }

    seg_clear_labels(labels);
    for (uint8_t row = 0U; row < TOF_ROWS; row++) {
        for (uint8_t col = 0U; col < TOF_COLS; col++) {
            if (combined_profile[row][col] == 1U) {
                body_frame[row][col] = 1U;
            }
        }
    }

    *component_count = seg_label_components(body_frame,
                                            labels,
                                            components,
                                            TOF_MAX_COMPONENTS,
                                            2U);
    if (*component_count == 0U) {
        *component_count = 1U;
    }
}

void depth_profile_generate(const uint16_t frame_mm[TOF_ROWS][TOF_COLS],
                            uint8_t labels[TOF_ROWS][TOF_COLS],
                            tof_component_t *components,
                            uint8_t *component_count,
                            uint8_t combined_profile[TOF_ROWS][TOF_COLS])
{

    memset(combined_profile, 0, sizeof(uint8_t) * TOF_ROWS * TOF_COLS);

    for (uint8_t c = 0U; c < *component_count; c++) {
        for (int row = components[c].box.y1; row <= components[c].box.y2; row++) {
            for (int col = components[c].box.x1; col <= components[c].box.x2; col++) {
                if ((row < 0) || (row >= (int)TOF_ROWS) ||
                    (col < 0) || (col >= (int)TOF_COLS) ||
                    (labels[row][col] != components[c].label)) {
                    continue;
                }

                float scale = depth_profile_regional_scale(&components[c], row);
                uint16_t distance = (uint16_t)lroundf((float)frame_mm[row][col] * scale);
                uint16_t local_max = (uint16_t)lroundf((float)components[c].second_max_distance_mm * scale);

                if (frame_mm[row][col] <= (components[c].min_distance_mm + TOF_DEPTH_THRESHOLD_MM)) {
                    combined_profile[row][col] = 2U; // Head or close to head
                } else {
                    combined_profile[row][col] = 1U; // Body
                }

                if (combined_profile[row][col] == 1U) {
                    uint16_t near_max_threshold = (local_max > 200U) ? (uint16_t)(local_max - 200U) : 0U;
                    if (distance >= near_max_threshold) {
                        combined_profile[row][col] = 3U; // Remain body or shoulder
                    } else {
                        combined_profile[row][col] = 1U; //close to head region
                    }
                }
            }
        }
    }

    if (*component_count == 1U) {
        depth_profile_split_single_component(combined_profile,
                                             labels,
                                             components,
                                             component_count);
    }
}
