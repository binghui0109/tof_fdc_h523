#include "segmentation.h"

#include <stddef.h>
#include <string.h>

#define SEG_NEIGHBOR_COUNT 8U

typedef struct {
    int row;
    int col;
} seg_node_t;

static const int8_t s_neighbor_offsets[SEG_NEIGHBOR_COUNT][2] = {
    {-1,  0},
    { 1,  0},
    { 0, -1},
    { 0,  1},
    {-1, -1},
    { 1,  1},
    { 1, -1},
    {-1,  1},
};

static void seg_push_if_valid(seg_node_t *stack,
                              int *top,
                              int row,
                              int col,
                              uint16_t visited[TOF_ROWS][TOF_COLS])
{
    if (*top >= (int)(TOF_ROWS * TOF_COLS)) {
        return;
    }

    if ((row >= 0) && (row < (int)TOF_ROWS) &&
        (col >= 0) && (col < (int)TOF_COLS) &&
        (visited[row][col] == 0U)) {
        visited[row][col] = 1U;
        stack[*top].row = row;
        stack[*top].col = col;
        (*top)++;
    }
}

static void seg_init_component(tof_component_t *comp, uint8_t label, int row, int col)
{
    comp->label = label;
    comp->size = 0U;
    comp->box.x1 = col;
    comp->box.y1 = row;
    comp->box.x2 = col;
    comp->box.y2 = row;
    comp->min_distance_mm = 4000U;
    comp->max_distance_mm = 0U;
    comp->second_max_distance_mm = 0U;
}

static void seg_update_component(tof_component_t *comp,
                                 const uint16_t frame_mm[TOF_ROWS][TOF_COLS],
                                 int row,
                                 int col)
{
    uint16_t value = frame_mm[row][col];

    comp->size++;
    if (col < comp->box.x1) {
        comp->box.x1 = col;
    }
    if (col > comp->box.x2) {
        comp->box.x2 = col;
    }
    if (row < comp->box.y1) {
        comp->box.y1 = row;
    }
    if (row > comp->box.y2) {
        comp->box.y2 = row;
    }

    if (value < comp->min_distance_mm) {
        comp->min_distance_mm = value;
    }
    if (value > comp->max_distance_mm) {
        comp->second_max_distance_mm = comp->max_distance_mm;
        comp->max_distance_mm = value;
    } else if ((value > comp->second_max_distance_mm) &&
               (value < comp->max_distance_mm)) {
        comp->second_max_distance_mm = value;
    }
}

void seg_clear_labels(uint8_t labels[TOF_ROWS][TOF_COLS])
{
    memset(labels, 0, sizeof(uint8_t) * TOF_ROWS * TOF_COLS);
}

uint8_t seg_label_components(const uint16_t frame_mm[TOF_ROWS][TOF_COLS],
                             uint8_t labels[TOF_ROWS][TOF_COLS],
                             tof_component_t *components,
                             uint8_t max_components,
                             uint8_t min_component_size)
{
    uint8_t current_label = 0U;
    uint8_t component_count = 0U;
    uint16_t visited[TOF_ROWS][TOF_COLS] = {0};
    seg_node_t stack[TOF_ROWS * TOF_COLS];

    for (int row = 0; row < (int)TOF_ROWS; row++) {
        for (int col = 0; col < (int)TOF_COLS; col++) {
            if ((frame_mm[row][col] == 0U) || (labels[row][col] != 0U)) {
                continue;
            }
            if (component_count >= max_components) {
                return component_count;
            }

            current_label++;
            seg_init_component(&components[component_count], current_label, row, col);

            int top = 0;
            stack[top].row = row;
            stack[top].col = col;
            top++;

            while (top > 0) {
                top--;
                int cr = stack[top].row;
                int cc = stack[top].col;

                if ((cr < 0) || (cr >= (int)TOF_ROWS) ||
                    (cc < 0) || (cc >= (int)TOF_COLS) ||
                    (frame_mm[cr][cc] == 0U) ||
                    (labels[cr][cc] != 0U)) {
                    continue;
                }

                visited[cr][cc] = 1U;
                labels[cr][cc] = current_label;
                seg_update_component(&components[component_count], frame_mm, cr, cc);
                for (uint8_t i = 0; i < SEG_NEIGHBOR_COUNT; i++)
                {
                    int nr = cr + s_neighbor_offsets[i][0];
                    int nc = cc + s_neighbor_offsets[i][1];
                    seg_push_if_valid(stack, &top, nr, nc, visited);
                }
            }

            if (components[component_count].size >= min_component_size) {
                component_count++;
            } else {
                for (int i = 0; i < (int)TOF_ROWS; i++) {
                    for (int j = 0; j < (int)TOF_COLS; j++) {
                        if (labels[i][j] == current_label) {
                            labels[i][j] = 0U;
                        }
                    }
                }
            }
        }
    }

    return component_count;
}
