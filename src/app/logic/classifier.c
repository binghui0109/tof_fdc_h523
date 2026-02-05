#include "classifier.h"

#include <stddef.h>
#include <string.h>

#include "ai.h"
#include "tof_types.h"

#define AI_BIN_NEAR 1330U
#define AI_BIN_MID 830U

static float s_ai_input[AI_NETWORK_IN_1_SIZE];
static float s_output_history[TOF_HISTORY_SIZE][TOF_NUM_CLASSES];
static uint8_t s_history_idx = 0U;
static uint8_t s_history_count = 0U;
static uint8_t s_previous_class = 0U;
static uint8_t s_fall_active = 0U;
static uint8_t s_fall_counter = 0U;

void classifier_init(void)
{
    AI_Init();
    classifier_reset();
}

void classifier_reset(void)
{
    memset(s_ai_input, 0, sizeof(s_ai_input));
    memset(s_output_history, 0, sizeof(s_output_history));
    s_history_idx = 0U;
    s_history_count = 0U;
    s_previous_class = 0U;
    s_fall_active = 0U;
    s_fall_counter = 0U;
}

void preprocess_frame_data(const uint16_t filtered_frame_mm[TOF_ROWS][TOF_COLS],
                        const uint16_t pixel_distance_bg_mm[TOF_ROWS][TOF_COLS],
                        float ai_out[TOF_NUM_CLASSES])
{
    uint16_t ai_idx = 0U;

    if ((filtered_frame_mm == NULL) || (pixel_distance_bg_mm == NULL) || (ai_out == NULL)) {
        return;
    }

    memset(s_ai_input, 0, sizeof(s_ai_input));

    for (uint8_t row = 0U; row < TOF_ROWS; row++) {
        for (uint8_t col = 0U; col < TOF_COLS; col++) {
            float feature = 0.0f;

            if (filtered_frame_mm[row][col] != 0U) {
                if (pixel_distance_bg_mm[row][col] > AI_BIN_NEAR) {
                    feature = 3.0f;
                } else if (pixel_distance_bg_mm[row][col] > AI_BIN_MID) {
                    feature = 2.0f;
                } else {
                    feature = 1.0f;
                }
            }

            if (ai_idx < AI_NETWORK_IN_1_SIZE) {
                s_ai_input[ai_idx] = feature;
                ai_idx++;
            }
        }
    }

    AI_Run(s_ai_input, ai_out);
}

uint8_t ai_output_moving_average(float ai_out[TOF_NUM_CLASSES])
{
    float smoothed[TOF_NUM_CLASSES] = {0.0f};
    uint8_t class_id;

    if (ai_out == NULL) {
        return 0U;
    }

    for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++) {
        s_output_history[s_history_idx][i] = ai_out[i];
    }

    s_history_idx = (uint8_t)((s_history_idx + 1U) % TOF_HISTORY_SIZE);
    if (s_history_count < TOF_HISTORY_SIZE) {
        s_history_count++;
    }

    for (uint8_t frame = 0U; frame < s_history_count; frame++) {
        for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++) {
            smoothed[i] += s_output_history[frame][i];
        }
    }

    for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++) {
        ai_out[i] = smoothed[i] / (float)s_history_count;
    }

    class_id = (uint8_t)(argmax(ai_out, TOF_NUM_CLASSES) + 1);

    if (((s_previous_class == 2U) || (s_previous_class == 3U)) && (class_id == 1U)) {
        s_fall_active = 1U;
        s_fall_counter = 1U;
        class_id = 4U;
    } else if ((s_fall_active != 0U) && (class_id == 1U)) {
        class_id = 4U;
        s_fall_counter++;
        if (s_fall_counter >= 6U) {
            s_fall_active = 0U;
            s_fall_counter = 0U;
        }
    } else {
        s_fall_active = 0U;
        s_fall_counter = 0U;
    }

    s_previous_class = class_id;
    return class_id;
}
