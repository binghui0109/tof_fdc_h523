#include "tof_process.h"

#include <stdbool.h>
#include <string.h>

#include "background.h"
#include "bsp_btn.h"
#include "bsp_led.h"
#include "classifier.h"
#include "depth_profile.h"
#include "segmentation.h"
#include "tof_types.h"
#include "tracking.h"

#define TOF_INVALID_DISTANCE_MM 4000U
#define TOF_INVALID_STATUS 255U
#define TOF_BG_STD_GAIN 2U

typedef struct {
    uint16_t filtered_mm[TOF_ROWS][TOF_COLS];
    uint16_t pixel_distance_bg_mm[TOF_ROWS][TOF_COLS];
    uint8_t labels[TOF_ROWS][TOF_COLS];
    uint8_t depth_profile[TOF_ROWS][TOF_COLS];
    tof_component_t components[TOF_MAX_COMPONENTS];
    uint8_t component_count;
    tof_people_data_t people;
    tof_person_info_t person_info[TOF_MAX_TRACKS];
    uint8_t person_info_count;
    float ai_out[TOF_NUM_CLASSES];
} tof_pipeline_context_t;

static tof_pipeline_context_t s_ctx;

static void tof_pipeline_clear_output(tof_pipeline_output_t *output)
{
    if (output != NULL) {
        memset(output, 0, sizeof(*output));
    }
}

static void tof_pipeline_reset_context(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
}

static void tof_pipeline_reset_runtime_modules(void)
{
    track_reset();
    classifier_reset();
}

static void tof_pipeline_prepare_filtered_frame(const VL53L5CX_ResultsData *frame,
                                                const bg_info_t *bg_info)
{
    for (uint8_t row = 0U; row < TOF_ROWS; row++) {
        for (uint8_t col = 0U; col < TOF_COLS; col++) {
            uint8_t zone_idx = (uint8_t)((row * TOF_COLS) + col);
            uint16_t target_idx = (uint16_t)(zone_idx * VL53L5CX_NB_TARGET_PER_ZONE);
            uint16_t distance_mm = frame->distance_mm[target_idx];
            uint8_t status = frame->target_status[target_idx];
            uint32_t bg_mean = bg_info->mean[row][col];
            uint32_t bg_std = bg_info->std[row][col];
            uint32_t threshold = bg_std * TOF_BG_STD_GAIN;
            int32_t delta = (int32_t)bg_mean - (int32_t)distance_mm;

            if ((distance_mm > TOF_INVALID_DISTANCE_MM) || (status == TOF_INVALID_STATUS) ||
                (delta <= (int32_t)threshold)) {
                s_ctx.filtered_mm[row][col] = 0U;
                s_ctx.pixel_distance_bg_mm[row][col] = 0U;
            } else {
                s_ctx.filtered_mm[row][col] = distance_mm;
                s_ctx.pixel_distance_bg_mm[row][col] = (bg_info->max > distance_mm)
                                                            ? (uint16_t)(bg_info->max - distance_mm)
                                                            : 0U;
            }
        }
    }
}

static bool tof_pipeline_handle_background_state(bg_status_t bg_status,
                                                 tof_pipeline_output_t *output)
{
    if (bg_status == BG_STATUS_COLLECTING) {
        led_chase_enable();
        if (output != NULL) {
            output->background_collecting = true;
        }
        return false;
    }

    if (bg_status == BG_STATUS_READY_JUST_FINISHED) {
        led_chase_disable();
        tof_pipeline_reset_context();
        tof_pipeline_reset_runtime_modules();
        if (output != NULL) {
            output->background_collecting = false;
        }
        return false;
    }

    return true;
}

static void tof_pipeline_run_segmentation_tracking(void)
{
    seg_clear_labels(s_ctx.labels);
    s_ctx.component_count = seg_label_components(s_ctx.filtered_mm,
                                                 s_ctx.labels,
                                                 s_ctx.components,
                                                 TOF_MAX_COMPONENTS,
                                                 2U);

    depth_profile_generate(s_ctx.filtered_mm,
                           s_ctx.labels,
                           s_ctx.components,
                           &s_ctx.component_count,
                           s_ctx.depth_profile);

    track_update(s_ctx.components,
                 s_ctx.component_count,
                 &s_ctx.people,
                 s_ctx.person_info,
                 &s_ctx.person_info_count);
    track_smooth_people_count(&s_ctx.people.people_count);
}

static void tof_pipeline_update_presence_indicator(void)
{
    if (s_ctx.people.people_count > 0U) {
        led_on(LED3);
    } else {
        led_off(LED3);
    }
}

static void tof_pipeline_update_classification(void)
{
    if (s_ctx.people.people_count == 1U) {
        process_frame_data(s_ctx.filtered_mm, s_ctx.pixel_distance_bg_mm, s_ctx.ai_out);
        s_ctx.people.class_id = ai_output_moving_average(s_ctx.ai_out);
        return;
    }

    memset(s_ctx.ai_out, 0, sizeof(s_ctx.ai_out));
    (void)ai_output_moving_average(s_ctx.ai_out);
    s_ctx.people.class_id = 0U;
}

static void tof_pipeline_fill_output(tof_pipeline_output_t *output)
{
    if (output == NULL) {
        return;
    }

    output->background_collecting = false;
    output->people = s_ctx.people;
    output->person_info_count = s_ctx.person_info_count;

    if (s_ctx.person_info_count > 0U) {
        memcpy(output->person_info,
               s_ctx.person_info,
               (size_t)s_ctx.person_info_count * sizeof(tof_person_info_t));
    }
}

void tof_pipeline_init(void)
{
    tof_pipeline_reset_context();
    bg_reset();
    track_reset();
    classifier_init();
    reg_btn_pos_edge_cb(BTN1, tof_pipeline_restart_background);
}

void tof_pipeline_process_frame(const VL53L5CX_ResultsData *frame, tof_pipeline_output_t *output)
{
    bg_status_t bg_status;

    tof_pipeline_clear_output(output);
    bg_status = bg_update(frame);
    if (!tof_pipeline_handle_background_state(bg_status, output)) {
        return;
    }

    tof_pipeline_prepare_filtered_frame(frame, bg_get_info());
    tof_pipeline_run_segmentation_tracking();
    tof_pipeline_update_presence_indicator();
    tof_pipeline_update_classification();
    tof_pipeline_fill_output(output);
}

void tof_pipeline_restart_background(void)
{
    bg_reset();
    tof_pipeline_reset_runtime_modules();
    tof_pipeline_reset_context();
}
