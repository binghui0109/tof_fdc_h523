#include "presence_logic.h"

#include <string.h>

#include "tof_types.h"

#define PRESENCE_HYSTERESIS_ENABLED 0U
#define PRESENCE_ENTER_FRAMES 2U
#define PRESENCE_EXIT_FRAMES 2U

static uint8_t s_history[TOF_HISTORY_SIZE];
static uint8_t s_history_idx = 0U;
static uint8_t s_history_count = 0U;
static uint8_t s_enter_counter = 0U;
static uint8_t s_exit_counter = 0U;

static uint8_t presence_clamp_people_count(uint8_t people_count)
{
    return (people_count > TOF_MAX_PEOPLE_COUNT) ? TOF_MAX_PEOPLE_COUNT : people_count;
}

static uint8_t presence_majority_vote(uint8_t raw_people_count)
{
    uint8_t counters[TOF_MAX_PEOPLE_COUNT + 1U] = {0};
    uint8_t result = 0U;
    uint8_t best = 0U;
    s_history[s_history_idx] = raw_people_count;
    s_history_idx = (uint8_t)((s_history_idx + 1U) % TOF_HISTORY_SIZE);
    if (s_history_count < TOF_HISTORY_SIZE)
    {
        s_history_count++;
    }

    for (uint8_t i = 0U; i < s_history_count; i++)
    {
        counters[s_history[i]]++;
    }

    for (uint8_t count = 0U; count <= TOF_MAX_PEOPLE_COUNT; count++)
    {
        if (counters[count] > best)
        {
            best = counters[count];
            result = count;
        }
    }

    return result;
}

void presence_logic_reset(void)
{
    memset(s_history, 0, sizeof(s_history));
    s_history_idx = 0U;
    s_history_count = 0U;
    s_enter_counter = 0U;
    s_exit_counter = 0U;
}

void presence_logic_update(uint8_t raw_people_count, presence_state_t *state)
{
    presence_state_t updated_state;

    updated_state.raw_people_count = presence_clamp_people_count(raw_people_count);
    updated_state.smoothed_people_count = presence_majority_vote(updated_state.raw_people_count);

    if (state != NULL)
    {
        *state = updated_state;
    }
}
