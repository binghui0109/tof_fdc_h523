#ifndef TRACKING_H
#define TRACKING_H

#include <stdint.h>

#include "tof_types.h"

void track_reset(void);
void track_update(const tof_component_t *components,
                  uint8_t component_count,
                  tof_people_data_t *people,
                  tof_person_info_t *person_info,
                  uint8_t *person_info_count);
void track_smooth_people_count(uint8_t *people_count);

#endif
