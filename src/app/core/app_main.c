#include "app_main.h"

#include "bsp_btn.h"
#include "bsp_led.h"
#include "bsp_serial.h"
#include "connection_manager.h"
#include "sensor_manager.h"
#include "tof_process.h"
#include "pb_manager.h"
#include "usart.h"

typedef struct
{
    app_mode_t mode;
    tof_pipeline_output_t pipeline_output;
} app_context_t;

static app_context_t s_app_ctx = {
    .mode = APP_MODE_INFERENCE,
};

uint8_t received_data[12] = {1};
volatile uint8_t test = 0;

void app_init(void)
{
    bsp_serial_init();
    led_init();
    btn_init();
    sensor_init();
    conn_init();
    tof_pipeline_init();
}

void app_main(void)
{
    const VL53L5CX_ResultsData *frame = NULL;

    conn_process_pending_commands();

    if (!sensor_get_data(&frame))
    {
        return;
    }

    switch (s_app_ctx.mode)
    {
    case APP_MODE_INFERENCE:
        tof_pipeline_process_frame(frame, &s_app_ctx.pipeline_output);
        send_pb_result(frame, &s_app_ctx.pipeline_output);
        break;
    case APP_MODE_DATA_RECORD:
        break;
    default:
        break;
    }

    conn_publish_frame(s_app_ctx.mode, frame, &s_app_ctx.pipeline_output);
}

void app_set_mode(app_mode_t mode)
{
    s_app_ctx.mode = mode;
}

app_mode_t app_get_mode(void)
{
    return s_app_ctx.mode;
}
