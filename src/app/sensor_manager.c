#include "main.h"
#include "sensor_manager.h"
#include "vl53l5cx.h"
#include "vl53l5cx_api.h"

#define MAX_SUBSCRIBERS (3)

TOF_SUB_PTR tof_subscribers[MAX_SUBSCRIBERS];
static uint8_t tof_sub_count = 0;
static VL53L5CX_ResultsData* tof_ptr;

void notify_tof_subscribers(VL53L5CX_ResultsData* buf, uint16_t size);

void sensor_init(void)
{
	tof_ptr = vl53l5_tof_init();
}

void sensor_get_data(void)
{
	bool status = vl53l5_update_data();
	if (status)
	{
		notify_tof_subscribers(tof_ptr, FRAME_RESOLUTION);
	}
}

void subscribe_tof_data(TOF_SUB_PTR sub)
{
	if (tof_sub_count < MAX_SUBSCRIBERS)
	{
		tof_subscribers[tof_sub_count] = sub;
		tof_sub_count++;
	}
	else
	{
		while (1);
	}
}

VL53L5CX_ResultsData* get_tof_buf(void)
{
	return tof_ptr;
}

void notify_tof_subscribers(VL53L5CX_ResultsData* buf, uint16_t size)
{
	for (uint8_t i = 0; i < tof_sub_count; i++)
	{
		tof_subscribers[i](buf, size);
	}
}

void unsubscribe_tof_data(TOF_SUB_PTR sub)
{
	for (uint8_t i = 0; i < tof_sub_count; i++)
	{
		if (tof_subscribers[i] == sub)
		{
			tof_subscribers[i] = NULL;
			tof_sub_count--;
		}
	}
}


