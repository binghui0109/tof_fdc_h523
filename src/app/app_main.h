#ifndef __APP_MAIN_H
#define __APP_MAIN_H

typedef enum {
    APP_MODE_INFERENCE = 0,
    APP_MODE_DATA_RECORD,
} app_mode_t;

void app_init(void);
void app_main(void);
void app_set_mode(app_mode_t mode);
app_mode_t app_get_mode(void);

#endif
