#ifndef TOF_TYPES_H
#define TOF_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define TOF_ROWS 8U
#define TOF_COLS 8U

#define TOF_MAX_COMPONENTS 10U
#define TOF_MAX_TRACKS 10U
#define TOF_MAX_PEOPLE_COUNT 2U
#define TOF_HISTORY_SIZE 10U
#define TOF_NUM_CLASSES 3U

#define TOF_DEPTH_THRESHOLD_MM 150U
#define TOF_MATCH_DISTANCE_THRESHOLD 5.0f
#define TOF_MAX_INACTIVE_FRAMES 5U

typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
} tof_bounding_box_t;

typedef struct {
    uint8_t label;
    uint8_t size;
    tof_bounding_box_t box;
    uint16_t min_distance_mm;
    uint16_t max_distance_mm;
    uint16_t second_max_distance_mm;
} tof_component_t;

typedef struct {
    int id;
    int current_row;
    int current_col;
    int previous_row;
    int previous_col;
    uint8_t inactive_frames;
    uint8_t duration_frames;
    bool active;
    bool counted_in;
    bool counted_out;
} tof_track_t;

typedef struct {
    int id;
    int x;
    int y;
    uint32_t duration_frames;
} tof_person_info_t;

typedef struct {
    uint8_t people_count;
    uint16_t people_in;
    uint16_t people_out;
    uint8_t class_id;
} tof_people_data_t;

#endif
