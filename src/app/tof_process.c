#include "tof_process.h"

#include <stdbool.h>
#include <string.h>

#include "background.h"
#include "bsp_led.h"
#include "classifier.h"
#include "connection_manager.h"
#include "depth_profile.h"
#include "segmentation.h"
#include "sensor_manager.h"
#include "tof_types.h"
#include "tracking.h"

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
static bool s_tof_data_ready = false;
static VL53L5CX_ResultsData *s_tof_ptr = NULL;

static void tof_data_handler(VL53L5CX_ResultsData *buf, uint16_t size);

static void tof_context_reset_runtime(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
}

static void tof_prepare_filtered_frame(const VL53L5CX_ResultsData *raw, const bg_info_t *bg_info)
{
    for (uint8_t row = 0U; row < TOF_ROWS; row++) {
        for (uint8_t col = 0U; col < TOF_COLS; col++) {
            uint8_t zone_idx = (uint8_t)((row * TOF_COLS) + col);
            uint16_t idx = (uint16_t)(zone_idx * VL53L5CX_NB_TARGET_PER_ZONE);
            uint8_t status = raw->target_status[idx];
            uint16_t distance = raw->distance_mm[idx];
            uint32_t mean = bg_info->mean[row][col];
            uint32_t std = bg_info->std[row][col];
            int32_t delta = (int32_t)mean - (int32_t)distance;
            uint32_t threshold = std * 2U;

            if ((distance > 4000U) || (status == 255U) || (delta <= (int32_t)threshold)) {
                s_ctx.filtered_mm[row][col] = 0U;
                s_ctx.pixel_distance_bg_mm[row][col] = 0U;
            } else {
                s_ctx.filtered_mm[row][col] = distance;
                if (bg_info->max > distance) {
                    s_ctx.pixel_distance_bg_mm[row][col] = (uint16_t)(bg_info->max - distance);
                } else {
                    s_ctx.pixel_distance_bg_mm[row][col] = 0U;
                }
            }
        }
    }
}

void tof_data_process_init(void)
{
    tof_context_reset_runtime();
    s_tof_data_ready = false;
    s_tof_ptr = get_tof_buf();

    bg_start();
    track_reset();
    classifier_init();
    conn_init();
    subscribe_tof_data(tof_data_handler);
}

void tof_data_process(void)
{
    if (conn_consume_bg_reinit_request()) {
        tof_restart_background_collection();
    }

    if (!s_tof_data_ready) {
        return;
    }

    s_tof_data_ready = false;
    if (s_tof_ptr == NULL) {
        return;
    }

    if (is_bg_collecting()) {
        led_chase_enable();
        if (bg_collect(s_tof_ptr)) {
            led_chase_disable();
            tof_context_reset_runtime();
            track_reset();
            classifier_reset();
        }
        conn_send_background_status(is_bg_collecting());
        return;
    }

    tof_prepare_filtered_frame(s_tof_ptr, bg_get_info());

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

    if (s_ctx.people.people_count > 0U) {
        led_on(LED3);
    } else {
        led_off(LED3);
    }

    if (s_ctx.people.people_count == 1U) {
        process_frame_data(s_ctx.filtered_mm, s_ctx.pixel_distance_bg_mm, s_ctx.ai_out);
        s_ctx.people.class_id = ai_output_moving_average(s_ctx.ai_out);
    } else {
        memset(s_ctx.ai_out, 0, sizeof(s_ctx.ai_out));
        (void)ai_output_moving_average(s_ctx.ai_out);
        s_ctx.people.class_id = 0U;
    }

    conn_send_runtime_data(s_tof_ptr,
                           &s_ctx.people,
                           s_ctx.person_info,
                           s_ctx.person_info_count);
}

void tof_restart_background_collection(void)
{
    bg_start();
    track_reset();
    classifier_reset();
    tof_context_reset_runtime();
}

static void tof_data_handler(VL53L5CX_ResultsData *buf, uint16_t size)
{
    (void)buf;
    (void)size;

    if (!s_tof_data_ready) {
        s_tof_data_ready = true;
    }
}
