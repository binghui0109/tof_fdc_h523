#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "tof_types.h"
#include "vl53l5cx_api.h"

void conn_init(void);
void conn_set_distance_stream_enabled(bool enabled);
bool conn_is_distance_stream_enabled(void);
bool conn_consume_bg_reinit_request(void);

void conn_send_background_status(bool background_collecting);
void conn_send_runtime_data(const VL53L5CX_ResultsData *raw_frame,
                            const tof_people_data_t *people,
                            const tof_person_info_t *person_info,
                            uint8_t person_count);

bool conn_handle_command(const uint8_t *packet, uint16_t packet_len, bool *request_bg_reinit);

#endif
