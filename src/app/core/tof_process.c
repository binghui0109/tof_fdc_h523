#include "tof_process.h"

#include <string.h>

#include "background.h"
#include "bsp_btn.h"
#include "bsp_led.h"
#include "classifier.h"
#include "depth_profile.h"
#include "foreground_filter.h"
#include "presence_logic.h"
#include "segmentation.h"
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
    presence_state_t presence_state;
    float ai_out[TOF_NUM_CLASSES];
} tof_pipeline_context_t;

static tof_pipeline_context_t s_ctx;

static void tof_pipeline_clear_output(tof_pipeline_output_t *output)
{
    memset(output, 0, sizeof(*output));
}

static void tof_pipeline_reset_context(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
}

static void tof_pipeline_reset_runtime_modules(void)
{
    track_reset();
    classifier_reset();
    presence_logic_reset();
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
}

static void tof_pipeline_run_presence_logic(void)
{
    presence_logic_update(s_ctx.people.people_count, &s_ctx.presence_state);
    s_ctx.people.people_count = s_ctx.presence_state.smoothed_people_count;
}

static void tof_pipeline_update_classification(void)
{
    if (s_ctx.people.people_count == 1U) {
        preprocess_and_run_ai(s_ctx.filtered_mm, s_ctx.pixel_distance_bg_mm, s_ctx.ai_out);
        s_ctx.people.class_id = ai_output_moving_average(s_ctx.ai_out);
        return;
    }

    memset(s_ctx.ai_out, 0, sizeof(s_ctx.ai_out));
    (void)ai_output_moving_average(s_ctx.ai_out);
    s_ctx.people.class_id = 0U;
}

static void tof_pipeline_fill_output(tof_pipeline_output_t *output)
{
    output->background_collecting = false;
    output->raw_people_count = s_ctx.presence_state.raw_people_count;
    output->smoothed_people_count = s_ctx.presence_state.smoothed_people_count;
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
    presence_logic_reset();
    reg_btn_pos_edge_cb(BTN1, tof_pipeline_restart_background);
}

void tof_pipeline_process_frame(const VL53L5CX_ResultsData *frame, tof_pipeline_output_t *output)
{
    tof_pipeline_clear_output(output);
    output->background_collecting = bg_update(frame);
    if (output->background_collecting)
    {
        led_chase_enable();
        return;
    }
    fg_filter_apply(frame, bg_get_info(), s_ctx.filtered_mm, s_ctx.pixel_distance_bg_mm);
    tof_pipeline_run_segmentation_tracking();
    tof_pipeline_run_presence_logic();
    tof_pipeline_update_classification();
    tof_pipeline_fill_output(output);
    led_chase_disable();
    if (output->smoothed_people_count > 0)
    {
        led_on(LED3);
    }
    else
    {
        led_off(LED3);
    }
}

void tof_pipeline_restart_background(void)
{
    bg_reset();
    tof_pipeline_reset_runtime_modules();
    tof_pipeline_reset_context();
}
