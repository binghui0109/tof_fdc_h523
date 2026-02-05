#ifndef TOF_PROCESS_H
#define TOF_PROCESS_H

#include <stdbool.h>

#include "tof_types.h"
#include "vl53l5cx_api.h"

typedef struct {
    bool background_collecting;
    tof_people_data_t people;
    tof_person_info_t person_info[TOF_MAX_TRACKS];
    uint8_t person_info_count;
} tof_pipeline_output_t;

void tof_pipeline_init(void);
void tof_pipeline_process_frame(const VL53L5CX_ResultsData *frame, tof_pipeline_output_t *output);
void tof_pipeline_restart_background(void);

#endif
