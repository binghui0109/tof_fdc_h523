#include "classifier.h"

#include <stddef.h>
#include <string.h>

#include "ai.h"
#include "tof_types.h"

#define AI_BIN_NEAR 1330U
#define AI_BIN_MID 830U
#define FALL_STABLE_FRAMES 6U

#define CLASS_LYING 1U
#define CLASS_2 2U
#define CLASS_3 3U
#define CLASS_FALL 4U

typedef enum{
    LYING = 1U,
    STANDING,
    SITTING,
    FALLING
} person_state_t;

static float s_ai_input[AI_NETWORK_IN_1_SIZE];
static float s_output_history[TOF_HISTORY_SIZE][TOF_NUM_CLASSES];
static float s_output_sum[TOF_NUM_CLASSES];
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
    memset(s_output_sum, 0, sizeof(s_output_sum));
    s_history_idx = 0U;
    s_history_count = 0U;
    s_previous_class = 0U;
    s_fall_active = 0U;
    s_fall_counter = 0U;
}

void preprocess_and_run_ai(const uint16_t filtered_frame_mm[TOF_ROWS][TOF_COLS],
                        const uint16_t pixel_distance_bg_mm[TOF_ROWS][TOF_COLS],
                        float ai_out[TOF_NUM_CLASSES])
{
    uint16_t ai_idx = 0U;
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
    uint8_t raw_class_id;
    uint8_t published_class_id;

    if (s_history_count == TOF_HISTORY_SIZE) {
        for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++) {
            s_output_sum[i] -= s_output_history[s_history_idx][i];
        }
    } else {
        s_history_count++;
    }

    for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++) {
        s_output_history[s_history_idx][i] = ai_out[i];
        s_output_sum[i] += ai_out[i];
    }

    s_history_idx = (uint8_t)((s_history_idx + 1U) % TOF_HISTORY_SIZE);

    for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++) {
        ai_out[i] = s_output_sum[i] / (float)s_history_count;
    }

    raw_class_id = (uint8_t)(argmax(ai_out, TOF_NUM_CLASSES) + 1U);
    published_class_id = raw_class_id;

    if (((s_previous_class == STANDING) || (s_previous_class == SITTING)) &&
        (raw_class_id == LYING)) {
        s_fall_active = 1U;
        s_fall_counter = 1U;
        published_class_id = FALLING;
    } else if ((s_fall_active != 0U) && (raw_class_id == LYING)) {
        published_class_id = FALLING;
        s_fall_counter++;
        if (s_fall_counter >= FALL_STABLE_FRAMES) {
            s_fall_active = 0U;
            s_fall_counter = 0U;
        }
    } else {
        s_fall_active = 0U;
        s_fall_counter = 0U;
    }

    s_previous_class = published_class_id;
    return published_class_id;
}
