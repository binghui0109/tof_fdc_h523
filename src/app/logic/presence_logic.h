#ifndef PRESENCE_LOGIC_H
#define PRESENCE_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t raw_people_count;
    uint8_t smoothed_people_count;
} presence_state_t;

void presence_logic_reset(void);
void presence_logic_update(uint8_t raw_people_count, presence_state_t *state);

#endif
