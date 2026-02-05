#include "tracking.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    int comp_idx;
    int track_idx;
    float distance;
} track_match_pair_t;

static tof_track_t s_tracks[TOF_MAX_TRACKS];
static uint8_t s_track_count = 0U;
static uint8_t s_next_id = 1U;

void track_reset(void)
{
    memset(s_tracks, 0, sizeof(s_tracks));
    s_track_count = 0U;
    s_next_id = 1U;
}

static float track_component_distance(const tof_component_t *comp, const tof_track_t *track)
{
    float comp_center_row = (float)(comp->box.y1 + comp->box.y2) / 2.0f;
    float comp_center_col = (float)(comp->box.x1 + comp->box.x2) / 2.0f;
    float row_diff = comp_center_row - (float)track->current_row;
    float col_diff = comp_center_col - (float)track->current_col;
    return sqrtf((row_diff * row_diff) + (col_diff * col_diff));
}

static void track_sort_pairs(track_match_pair_t *pairs, int pair_count)
{
    for (int i = 0; i < (pair_count - 1); i++) {
        for (int j = i + 1; j < pair_count; j++) {
            if (pairs[i].distance > pairs[j].distance) {
                track_match_pair_t tmp = pairs[i];
                pairs[i] = pairs[j];
                pairs[j] = tmp;
            }
        }
    }
}

static void track_collect_people_info(tof_people_data_t *people,
                                      tof_person_info_t *person_info,
                                      uint8_t *person_info_count)
{
    uint8_t stable_count = 0U;
    uint8_t info_count = 0U;

    for (uint8_t i = 0U; i < s_track_count; i++) {
        if (s_tracks[i].duration_frames > 4U) {
            if (person_info != NULL) {
                person_info[info_count].id = s_tracks[i].id;
                person_info[info_count].x = s_tracks[i].current_col;
                person_info[info_count].y = s_tracks[i].current_row;
                person_info[info_count].duration_frames = s_tracks[i].duration_frames;
            }
            info_count++;
            stable_count++;
            if (info_count >= TOF_MAX_TRACKS) {
                break;
            }
        }
    }

    if (person_info_count != NULL) {
        *person_info_count = info_count;
    }

    people->people_count = (stable_count > 2U) ? 2U : stable_count;
}

void track_update(const tof_component_t *components,
                  uint8_t component_count,
                  tof_people_data_t *people,
                  tof_person_info_t *person_info,
                  uint8_t *person_info_count)
{
    if ((components == NULL) || (people == NULL)) {
        return;
    }

    for (uint8_t i = 0U; i < s_track_count; i++) {
        if (s_tracks[i].active) {
            s_tracks[i].inactive_frames++;
        }
    }

    track_match_pair_t pairs[TOF_MAX_COMPONENTS * TOF_MAX_TRACKS];
    int pair_count = 0;
    memset(pairs, 0, sizeof(pairs));

    for (uint8_t c = 0U; c < component_count; c++) {
        for (uint8_t t = 0U; t < s_track_count; t++) {
            if (!s_tracks[t].active) {
                continue;
            }
            pairs[pair_count].comp_idx = c;
            pairs[pair_count].track_idx = t;
            pairs[pair_count].distance = track_component_distance(&components[c], &s_tracks[t]);
            pair_count++;
        }
    }

    track_sort_pairs(pairs, pair_count);

    bool comp_matched[TOF_MAX_COMPONENTS] = {false};
    bool track_matched[TOF_MAX_TRACKS] = {false};

    for (int i = 0; i < pair_count; i++) {
        int c = pairs[i].comp_idx;
        int t = pairs[i].track_idx;
        float dist = pairs[i].distance;

        if ((dist <= TOF_MATCH_DISTANCE_THRESHOLD) &&
            !comp_matched[c] &&
            !track_matched[t]) {
            comp_matched[c] = true;
            track_matched[t] = true;

            s_tracks[t].previous_row = s_tracks[t].current_row;
            s_tracks[t].previous_col = s_tracks[t].current_col;
            s_tracks[t].current_row = (components[c].box.y1 + components[c].box.y2) / 2;
            s_tracks[t].current_col = (components[c].box.x1 + components[c].box.x2) / 2;
            s_tracks[t].inactive_frames = 0U;
            s_tracks[t].duration_frames++;
        }
    }

    for (uint8_t c = 0U; c < component_count; c++) {
        if (!comp_matched[c] && (s_track_count < TOF_MAX_TRACKS)) {
            float center_row = (float)(components[c].box.y1 + components[c].box.y2) / 2.0f;
            float center_col = (float)(components[c].box.x1 + components[c].box.x2) / 2.0f;

            s_tracks[s_track_count].id = s_next_id++;
            s_tracks[s_track_count].current_row = (int)center_row;
            s_tracks[s_track_count].current_col = (int)center_col;
            s_tracks[s_track_count].previous_row = s_tracks[s_track_count].current_row;
            s_tracks[s_track_count].previous_col = s_tracks[s_track_count].current_col;
            s_tracks[s_track_count].active = true;
            s_tracks[s_track_count].inactive_frames = 0U;
            s_tracks[s_track_count].duration_frames = 1U;
            s_tracks[s_track_count].counted_in = false;
            s_tracks[s_track_count].counted_out = false;
            s_track_count++;
        }
    }

    uint8_t active_count = 0U;
    for (uint8_t t = 0U; t < s_track_count; t++) {
        if (!s_tracks[t].active) {
            continue;
        }

        if (s_tracks[t].inactive_frames > TOF_MAX_INACTIVE_FRAMES) {
            s_tracks[t].active = false;
            s_tracks[t].duration_frames = 1U;
            if (s_tracks[t].counted_in) {
                people->people_out++;
            }
            s_tracks[t].counted_in = false;
            s_tracks[t].counted_out = false;
            continue;
        }

        if (!s_tracks[t].counted_in && (s_tracks[t].duration_frames > 8U)) {
            people->people_in++;
            s_tracks[t].counted_in = true;
        }

        if (t != active_count) {
            s_tracks[active_count] = s_tracks[t];
        }
        active_count++;
    }
    s_track_count = active_count;

    track_collect_people_info(people, person_info, person_info_count);
}
