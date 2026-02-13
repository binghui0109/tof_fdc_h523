#ifndef PB_MANAGER_H
#define PB_MANAGER_H

#include "tof_process.h"
#include "vl53l5cx_api.h"

void send_pb_result(const VL53L5CX_ResultsData *raw_frame, const tof_pipeline_output_t *pipeline_output);

#endif
