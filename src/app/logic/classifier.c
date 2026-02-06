#include "classifier.h"

#include <stddef.h>
#include <string.h>

#include "ai.h"
#include "tof_types.h"

#define AI_BIN_NEAR 1330U
#define AI_BIN_MID 830U
#define FALL_PRE_HOLD_FRAMES 2U
#define FALL_TRANSITION_FRAMES 6U

#define CLASS_LYING 1U
#define CLASS_STANDING 2U
#define CLASS_SITTING 3U
#define CLASS_FALLING 4U

static float s_ai_input[AI_NETWORK_IN_1_SIZE];
static float s_output_history[TOF_HISTORY_SIZE][TOF_NUM_CLASSES];
static float s_output_sum[TOF_NUM_CLASSES];
static uint8_t s_history_idx = 0U;
static uint8_t s_history_count = 0U;
static uint8_t s_previous_raw_class = 0U;
static bool s_fall_sequence_active = false;
static uint8_t s_fall_sequence_counter = 0U;
static uint8_t s_pre_fall_class = 0U;

static bool classifier_is_lying(uint8_t class_id)
{
    return class_id == CLASS_LYING;
}

static bool classifier_is_upright(uint8_t class_id)
{
    return (class_id == CLASS_STANDING) || (class_id == CLASS_SITTING);
}

static uint8_t classifier_apply_fall_state(uint8_t raw_class_id)
{
    const uint8_t transition_total_frames = FALL_PRE_HOLD_FRAMES + FALL_TRANSITION_FRAMES;
    uint8_t published_class_id = raw_class_id;
    bool upright_to_lying = classifier_is_upright(s_previous_raw_class) && classifier_is_lying(raw_class_id);

    if (!s_fall_sequence_active && upright_to_lying)
    {
        s_fall_sequence_active = true;
        s_fall_sequence_counter = 0U;
        s_pre_fall_class = s_previous_raw_class;
    }

    if (s_fall_sequence_active)
    {
        if (!classifier_is_lying(raw_class_id))
        {
            s_fall_sequence_active = false;
            s_fall_sequence_counter = 0U;
            s_pre_fall_class = 0U;
        }
        else
        {
            s_fall_sequence_counter++;

            if (s_fall_sequence_counter <= FALL_PRE_HOLD_FRAMES)
            {
                published_class_id = s_pre_fall_class;
            }
            else if (s_fall_sequence_counter <= transition_total_frames)
            {
                published_class_id = CLASS_FALLING;
            }
            else
            {
                published_class_id = CLASS_LYING;
            }
        }
    }

    s_previous_raw_class = raw_class_id;
    return published_class_id;
}

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
    s_previous_raw_class = 0U;
    s_fall_sequence_active = false;
    s_fall_sequence_counter = 0U;
    s_pre_fall_class = 0U;
}

void preprocess_and_run_ai(const uint16_t filtered_frame_mm[TOF_ROWS][TOF_COLS],
                           const uint16_t pixel_distance_bg_mm[TOF_ROWS][TOF_COLS], float ai_out[TOF_NUM_CLASSES])
{
    uint16_t ai_idx = 0U;
    memset(s_ai_input, 0, sizeof(s_ai_input));

    for (uint8_t row = 0U; row < TOF_ROWS; row++)
    {
        for (uint8_t col = 0U; col < TOF_COLS; col++)
        {
            float feature = 0.0f;

            if (filtered_frame_mm[row][col] != 0U)
            {
                if (pixel_distance_bg_mm[row][col] > AI_BIN_NEAR)
                {
                    feature = 3.0f;
                }
                else if (pixel_distance_bg_mm[row][col] > AI_BIN_MID)
                {
                    feature = 2.0f;
                }
                else
                {
                    feature = 1.0f;
                }
            }

            if (ai_idx < AI_NETWORK_IN_1_SIZE)
            {
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

    if (s_history_count == TOF_HISTORY_SIZE)
    {
        for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++)
        {
            s_output_sum[i] -= s_output_history[s_history_idx][i];
        }
    }
    else
    {
        s_history_count++;
    }

    for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++)
    {
        s_output_history[s_history_idx][i] = ai_out[i];
        s_output_sum[i] += ai_out[i];
    }

    s_history_idx = (uint8_t)((s_history_idx + 1U) % TOF_HISTORY_SIZE);

    for (uint8_t i = 0U; i < TOF_NUM_CLASSES; i++)
    {
        ai_out[i] = s_output_sum[i] / (float)s_history_count;
    }

    raw_class_id = (uint8_t)(argmax(ai_out, TOF_NUM_CLASSES) + 1U);
    return classifier_apply_fall_state(raw_class_id);
}
