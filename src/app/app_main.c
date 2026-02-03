#include "bsp_serial.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_btn.h"
#include "bsp_gpio.h"
#include "sensor_manager.h"
#include "ai_manager.h"

typedef enum{
    TOF_PROCESS_STATE,
    // BG_INIT_STATE,
    DATA_REC_STATE,
} tof_state_t;

tof_state_t tof_state = TOF_PROCESS_STATE;
uint8_t TxMessageBuffer[] = "MY USB IS WORKING! \r\n";
int16_t* tof_buf = NULL;
//   while(hUsbDeviceFS.pClassData == NULL);


void btn1_cb()
{
    bsp_serial_tx_data(TxMessageBuffer, sizeof(TxMessageBuffer));
}

void app_init(void)
{
    bsp_serial_init();
    led_init();
    btn_init();
    sensor_init();
    tof_data_process_init();
}

void change_tof_state(tof_state_t state)
{
    tof_state = state;
}

void app_main(void)
{
    sensor_get_data();
    switch (tof_state)
    {
    case TOF_PROCESS_STATE:
        tof_data_process();
        break;
    // case BG_INIT_STATE:
    //     bg_collect();
        break;
    case DATA_REC_STATE:
        break;
    }
    // handle_data_request();
}               