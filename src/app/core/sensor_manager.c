#include "sensor_manager.h"

#include <stddef.h>

#include "vl53l5cx.h"

static VL53L5CX_ResultsData *s_tof_frame = NULL;

void sensor_init(void)
{
    s_tof_frame = vl53l5_tof_init();
}

bool sensor_get_data(const VL53L5CX_ResultsData **frame)
{
    if ((frame == NULL) || (s_tof_frame == NULL) || (!vl53l5_update_data())) {
        return false;
    }

    *frame = s_tof_frame;
    return true;
}
