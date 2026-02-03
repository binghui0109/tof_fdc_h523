
#include <stdbool.h>
#include "vl53l5cx.h"
#include "sensor_manager.h"
#include "bsp_led.h"
#include "background.h"

typedef struct{
    int16_t distance[ROWS][COLS];
    uint8_t status[ROWS][COLS];
} tof_data_t;

tof_data_t tof_data = {0};
bool tof_data_ready = false;
static VL53L5CX_ResultsData* tof_ptr;

void tof_data_handler(VL53L5CX_ResultsData* buf, uint16_t size);

void tof_data_process_init(void)
{
    tof_ptr = get_tof_buf();
    bg_start();
    subscribe_tof_data(tof_data_handler);
}

void tof_data_process(void)
{
    if (tof_data_ready)
    {
        tof_data_ready = false;
        if(is_bg_collecting())
        {
            led_chase_enable();
            if(bg_collect(tof_ptr))
            {
                led_chase_disable();
            }
        }
        // else
        // {
        //     run_tof_pipeline();
        // }
    }
}

void tof_data_handler(VL53L5CX_ResultsData* buf, uint16_t size)
{
    if (tof_data_ready == false)
    {
        // uint8_t idx = 0
        // for (uint8_t i = 0; i < ROWS; i ++)
        // {
        //     for (uint8_t j = 0; j < COLS; j ++)
        //     {
        //         idx = (i * ROWS) + j;
        //         tof_data.distance[i][j] = buf.distance_mm[idx];
        //         tof_data.status[i][j] = buf.target_status[idx];
        //     }
        // }
        tof_data_ready = true;
        
    }
}
