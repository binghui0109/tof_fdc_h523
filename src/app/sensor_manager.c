#include "sensor_manager.h"

#include <stddef.h>

#include "vl53l5cx.h"

#define SENSOR_MAX_SUBSCRIBERS 3U

static TOF_SUB_PTR s_tof_subscribers[SENSOR_MAX_SUBSCRIBERS] = {0};
static uint8_t s_tof_sub_count = 0U;
static VL53L5CX_ResultsData *s_tof_ptr = NULL;

static void sensor_notify_subscribers(VL53L5CX_ResultsData *buf, uint16_t size)
{
    for (uint8_t i = 0U; i < s_tof_sub_count; i++) {
        if (s_tof_subscribers[i] != NULL) {
            s_tof_subscribers[i](buf, size);
        }
    }
}

void sensor_init(void)
{
    s_tof_ptr = vl53l5_tof_init();
}

void sensor_get_data(void)
{
    if (vl53l5_update_data()) {
        sensor_notify_subscribers(s_tof_ptr, FRAME_RESOLUTION);
    }
}

void subscribe_tof_data(TOF_SUB_PTR sub)
{
    if ((sub == NULL) || (s_tof_sub_count >= SENSOR_MAX_SUBSCRIBERS)) {
        return;
    }

    for (uint8_t i = 0U; i < s_tof_sub_count; i++) {
        if (s_tof_subscribers[i] == sub) {
            return;
        }
    }

    s_tof_subscribers[s_tof_sub_count] = sub;
    s_tof_sub_count++;
}

VL53L5CX_ResultsData *get_tof_buf(void)
{
    return s_tof_ptr;
}

void unsubscribe_tof_data(TOF_SUB_PTR sub)
{
    if ((sub == NULL) || (s_tof_sub_count == 0U)) {
        return;
    }

    for (uint8_t i = 0U; i < s_tof_sub_count; i++) {
        if (s_tof_subscribers[i] == sub) {
            for (uint8_t j = i; j < (s_tof_sub_count - 1U); j++) {
                s_tof_subscribers[j] = s_tof_subscribers[j + 1U];
            }
            s_tof_subscribers[s_tof_sub_count - 1U] = NULL;
            s_tof_sub_count--;
            return;
        }
    }
}
