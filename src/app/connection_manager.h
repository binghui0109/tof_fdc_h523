#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "app_main.h"
#include "tof_process.h"
#include "vl53l5cx_api.h"

void conn_init(void);
void conn_process_pending_commands(void);
void conn_publish_frame(app_mode_t app_mode,
                        const VL53L5CX_ResultsData *raw_frame,
                        const tof_pipeline_output_t *pipeline_output);

#endif
