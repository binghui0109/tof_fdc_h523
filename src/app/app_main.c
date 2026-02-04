#include "app_main.h"

#include <stdint.h>

#include "bsp_btn.h"
#include "bsp_led.h"
#include "bsp_serial.h"
#include "sensor_manager.h"
#include "tof_process.h"

typedef enum {
    APP_STATE_TOF_PIPELINE = 0,
    APP_STATE_DATA_RECORD,
} app_state_t;

static app_state_t s_app_state = APP_STATE_TOF_PIPELINE;
static uint8_t s_btn_msg[] = "Background reinit\r\n";

static void app_btn1_rising_cb(void)
{
    tof_restart_background_collection();
    bsp_serial_tx_data(s_btn_msg, sizeof(s_btn_msg) - 1U);
}

void app_init(void)
{
    bsp_serial_init();
    led_init();
    btn_init();
    reg_btn_pos_edge_cb(BTN1, app_btn1_rising_cb);

    sensor_init();
    tof_data_process_init();
}

void app_main(void)
{
    sensor_get_data();

    switch (s_app_state) {
    case APP_STATE_TOF_PIPELINE:
        tof_data_process();
        break;
    case APP_STATE_DATA_RECORD:
    default:
        break;
    }
}
