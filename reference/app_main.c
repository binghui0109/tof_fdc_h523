/*
 * app_main.c
 *
 *  Created on: Mar 19, 2025
 *      Author: sai
 */

#include "stdbool.h"
#include "main.h"
#include "tx_api.h"
#include "ux_api.h"
#include "usart.h"
#include "app_main.h"
#include "vl53l5cx_api.h"
#include "ai_func.h"
#include "vl53l5cx_plugin_xtalk.h"
#include "vl53l5cx_plugin_motion_indicator.h"
#include "ux_device_cdc_acm.h"
#include "tim.h"
//#include "eeprom_emul.h"

#ifndef FRAME_RESOLUTION
#define FRAME_RESOLUTION  	64       /* 16 if resolution is VL53L5CX_RESOLUTION_4X4 else, 64 if VL53L5CX_RESOLUTION_8X8 */
#endif
#ifndef FRAMES
#define FRAMES              	1      /* Should be between 1 & 32 */
#endif
#ifndef DISTANCE_ODR
#define DISTANCE_ODR        	8      /* Should be between 1 -> 60Hz for VL53L5CX_RESOLUTION_4X4 and 1 -> 15Hz for VL53L5CX_RESOLUTION_8X8 */
#endif
#define ROWS 					8
#define COLS 					8

#define BACKGROUND_FRAMES 		100
#define MIN_VALID_SAMPLES 		1
#define DEPTH_THRESHOLD 		150
#define MAX_TRACKS 				10
#define ENTRY_EXIT_THRESHOLD 	3
#define MAX_INACTIVE_FRAMES 	5
#define HISTORY_SIZE 			10
#define NUM_CLASSES 			3
#define COMMAND_LENGTH 			12
#define MAX_COMPONENTS   		10
#define MATCH_DISTANCE_THRESHOLD 5.0f
#define ICACHE_ENABLED                 /* ICache is enabled by default for this example */

typedef struct {
	uint16_t distance[FRAMES][ROWS][COLS];
	uint16_t status[FRAMES][ROWS][COLS];
	uint16_t reflectance[FRAMES][ROWS][COLS];
	uint32_t motion_indicator[FRAMES][ROWS][COLS];
	uint32_t signal_per_spad[FRAMES][ROWS][COLS];
	uint8_t frame_count;
} FrameBuffer;

// Enum for states
typedef struct {
	int x1, y1, x2, y2;
} BoundingBox;

typedef struct {
	uint8_t label;
	uint8_t size;
	BoundingBox box;
	uint16_t min_distance;
	uint16_t depth_profile[ROWS][COLS];
	uint16_t max_distance;
	uint16_t second_max_distance;
} Component;

typedef struct {
	int id;
	int current_row;
	int current_col;
	int previous_row;
	int previous_col;
	int active;
	int inactive_frames;
	int duration_frames;
	bool counted_in;
	bool counted_out;
//    int state;
} Track;

typedef struct {
	int id;
	int x;
	int y;
	uint32_t duration_frames;
} PersonInfo;

typedef struct {
	int comp_idx;   // which component
	int track_idx;  // which track
	float distance;
} MatchPair;

typedef struct {
	uint8_t people_count;
	uint16_t people_in;
	uint16_t people_out;
	uint8_t class;
} PeopleData;

typedef enum
{
    RISING_EDGE = 0,
    DEBOUNCING,
    FALLING_EDGE,
} btn_state_t;

btn_state_t btn_state = RISING_EDGE;
uint8_t status, isAlive, isReady;
volatile uint8_t vl53l5_data_ready = 0;
int16_t neai_buffer[ROWS][COLS] = { 0 };
VL53L5CX_Configuration Dev; /* Sensor configuration */
VL53L5CX_Motion_Configuration motion_config;
VL53L5CX_ResultsData Results; /* Results data from VL53L5CX */
uint16_t background_std[ROWS][COLS] = { 0 };
uint16_t background_variance[ROWS][COLS] = { 0 };
volatile bool background_initialized = true;

PersonInfo person_info[MAX_TRACKS];
uint8_t person_info_count = 0;
Track tracks[MAX_TRACKS];
uint8_t track_count = 0;
uint8_t next_id = 1;
Component components[10];
uint8_t component_count = 0;
FrameBuffer frame_buffer = { 0 };
PeopleData People_Data = { 0 };
uint8_t labeled_frame[ROWS][COLS];
uint8_t consecutive_state_count = 0;
uint8_t people_out_history[HISTORY_SIZE] = { 0 }; // Buffer to store recent values
uint8_t history_buffer_idx = 0;
float scale;
const char *class_names[] = { "None", "Lying floor", "Sitting", "Standing",
		"Falling Down!" };
float output_buffer[HISTORY_SIZE][NUM_CLASSES] = { 0 };
uint8_t count = 0;
uint8_t buffer_buffer_idx;
uint8_t received_data[COMMAND_LENGTH];
volatile bool distance_flag = true;
uint8_t combined_depth_profile[ROWS][COLS] = { 0 };
uint8_t status, isAlive, isReady;
uint8_t buffer[140];
uint8_t buffer_idx, checksum;
uint16_t max_value_mean;
uint16_t pixel_distance_background[ROWS][COLS];
uint8_t pin_state;
/* Declare AI variables */
float aiInData[AI_NETWORK_IN_1_SIZE];
float aiOutData[AI_NETWORK_OUT_1_SIZE];

__attribute__((section(".noinit")))
volatile uint32_t boot_flag;

/* RTOS Stuffs */
#define TOF_DATA_RDY 0x01
TX_EVENT_FLAGS_GROUP TOF_EVENT_FLAG;
extern UART_HandleTypeDef huart1;
extern TX_EVENT_FLAGS_GROUP CDC_EVENT_FLAG;
extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm;

/*  function prototypes   */
void vl53l5cx_initialize(void);
void initialize_background();
void vl53l5cx_data_collect();
void process_frame_data();
void label_components(uint16_t current_frame[ROWS][COLS]);
void update_tracks();
void create_depth_profiles();
void update_people_out(uint8_t *current_people_out);
void ai_output_moving_average(float *aiOutData);
void send_data_uart(void);
void SaveDistanceFlagToFlash(bool distance_flag);
bool ReadDistanceFlagFromFlash(void);
void cdc_send(void);
void User_Btn_ElapsedCallback(TIM_HandleTypeDef *htim);

void app_main_init(void) {
	AI_Init();

	HAL_GPIO_WritePin(VL53_xshut_GPIO_Port, VL53_xshut_Pin, GPIO_PIN_SET);
//	HAL_Delay(1000);

	Dev.platform.address = VL53L5CX_DEFAULT_I2C_ADDRESS;
	vl53l5cx_initialize();

	__HAL_UART_FLUSH_DRREGISTER(&huart1);
	HAL_UART_Receive_IT(&huart1, (uint8_t*) received_data, COMMAND_LENGTH);

	if (tx_event_flags_create(&TOF_EVENT_FLAG, "TOF Event Flag") != TX_SUCCESS)
	{
		while(1);
	}
	HAL_TIM_RegisterCallback(&htim2, HAL_TIM_PERIOD_ELAPSED_CB_ID, User_Btn_ElapsedCallback);
	HAL_TIM_Base_Start_IT(&htim2);
}


void app_main_thread(uint32_t args)
{
	(void) args;
//	tx_thread_sleep(MS_TO_TICK(10));
	while (1) {
		if (background_initialized) {
			initialize_background();
			background_initialized = false;
		}
		if (background_initialized == false) {
			memset(labeled_frame, 0, sizeof(labeled_frame));
			component_count = 0;
			People_Data.people_count = 0;

			vl53l5cx_data_collect();
			label_components(frame_buffer.distance[0]);
			create_depth_profiles();
			update_tracks();
			update_people_out(&People_Data.people_count);
			if (People_Data.people_count > 0){
				HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
			}
			else {
				HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
			}
			if (People_Data.people_count == 1) {
				process_frame_data();
				ai_output_moving_average(aiOutData);
			} else {
				memset(aiOutData, 0, sizeof(aiOutData));
				ai_output_moving_average(aiOutData);
				People_Data.class = 0;
			}
			send_data_uart();
			tx_thread_sleep(MS_TO_TICK(10));
			// UART4_SendPeopleData(&People_Data);
			// Process accumulated frames if buffer is full
		}
	}
}

/**
 * @brief  VL53L5CX initialization function with the defined settings
 * @retval
 * @param
 */
void vl53l5cx_initialize() {
	do {
		HAL_Delay(10);
		/* Check if there is a VL53L5CX sensor connected */
		status = vl53l5cx_is_alive(&Dev, &isAlive);
	} while (!isAlive || status);

	/* Init VL53L5CX sensor */
	status = vl53l5cx_init(&Dev);
//  status = vl53l5cx_motion_indicator_init(&Dev, &motion_config, VL53L5CX_RESOLUTION_8X8);
	if (status) {
		while (1)
			;
	}
	status = vl53l5cx_set_xtalk_margin(&Dev, 50);
	status = vl53l5cx_set_target_order(&Dev, VL53L5CX_TARGET_ORDER_STRONGEST);
	status = vl53l5cx_set_sharpener_percent(&Dev, 5);
	status = vl53l5cx_set_ranging_frequency_hz(&Dev, DISTANCE_ODR);
	status = vl53l5cx_set_resolution(&Dev, FRAME_RESOLUTION);
	status = vl53l5cx_set_ranging_mode(&Dev, VL53L5CX_RANGING_MODE_CONTINUOUS);
	status = vl53l5cx_start_ranging(&Dev);

}

// Function to compute the mean of each zone
void initialize_background() {
	uint32_t dataflag;
	//    printf("initializing background\n");
	uint16_t temp_frame[ROWS][COLS];
	uint16_t temp_frame_status[ROWS][COLS];
	uint32_t sum_values[ROWS][COLS] = { 0 };
	uint32_t sum_squared_values[ROWS][COLS] = { 0 }; // For variance calculation
	uint8_t valid_count[ROWS][COLS] = { 0 };
	uint32_t max_value_sum = 0;
	People_Data.people_in = 0;
	People_Data.people_out = 0;
	People_Data.people_count = 0;

	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);

	// Collect data, compute both sum and sum of squares in a single pass
	for (uint8_t frame = 0; frame < BACKGROUND_FRAMES; frame++) {
//	    while (!vl53l5_data_ready) {}
		tx_event_flags_get(&TOF_EVENT_FLAG, TOF_DATA_RDY, TX_OR_CLEAR,
				&dataflag, TX_WAIT_FOREVER);
		send_data_uart();
		vl53l5cx_get_ranging_data(&Dev, &Results);
		uint16_t max_value = 0;
		for (uint8_t i = 0; i < ROWS; i++) {
			for (uint8_t j = 0; j < COLS; j++) {
				uint8_t idx = (i * ROWS) + j;
				temp_frame[i][j] =
						Results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * idx];
				temp_frame_status[i][j] =
						Results.target_status[VL53L5CX_NB_TARGET_PER_ZONE * idx];
			}
		}

		for (uint8_t i = 0; i < ROWS; i++) {
			for (uint8_t j = 0; j < COLS; j++) {
				if (temp_frame_status[i][j] != 255) { // Valid data
					valid_count[i][j]++;
					uint16_t value = temp_frame[i][j];
					if (value > max_value && (temp_frame_status[i][j] == 5 || temp_frame_status[i][j] == 9)){
						max_value = value;
					}
					// Accumulate for mean
					sum_values[i][j] += value;
					// Accumulate for variance
					sum_squared_values[i][j] += value * value;
				}
			}
		}
		max_value_sum += max_value;
		vl53l5_data_ready = 0;
		tx_thread_sleep(MS_TO_TICK(100));
	}

	// Compute mean and variance using the same data
	for (uint8_t i = 0; i < ROWS; i++) {
		for (uint8_t j = 0; j < COLS; j++) {
			if (valid_count[i][j] >= MIN_VALID_SAMPLES) {
				// Calculate the mean
				uint16_t mean_value = sum_values[i][j] / valid_count[i][j];
				background_std[i][j] = mean_value;

				// Calculate the variance
				uint32_t mean_square = sum_squared_values[i][j]
															 / valid_count[i][j];
				uint16_t variance_value = mean_square
						- (mean_value * mean_value);
				background_variance[i][j] = variance_value;
			} else {
				background_std[i][j] = 4000; // Not enough valid samples
				background_variance[i][j] = 0; // No variance for insufficient data
			}
		}
	}
	max_value_mean = max_value_sum/BACKGROUND_FRAMES;
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
}

void vl53l5cx_data_collect() {
	uint32_t dataflag;
	uint8_t status = 0;
	// Wait until data is ready
//    while (!vl53l5_data_ready) {}
	tx_event_flags_get(&TOF_EVENT_FLAG, TOF_DATA_RDY, TX_OR_CLEAR,
			           &dataflag, TX_WAIT_FOREVER);
	// Get ranging data
	vl53l5cx_get_ranging_data(&Dev, &Results);

	// Store values in the frame buffer
	uint8_t current_buffer_buffer_idx = frame_buffer.frame_count;

	for (uint8_t i = 0; i < ROWS; i++) {
		for (uint8_t j = 0; j < COLS; j++) {
			uint8_t idx = (i * ROWS) + j;
			float threshold = 2 * sqrt(background_variance[i][j]);
			neai_buffer[i][j] = background_std[i][j]
					- Results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * idx];
			if (Results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * idx] > 4000
					|| (Results.target_status[VL53L5CX_NB_TARGET_PER_ZONE * idx]
							== 255)
//                && Results.target_status[VL53L5CX_NB_TARGET_PER_ZONE * idx] != 9)
					|| neai_buffer[i][j] <= threshold) {
				frame_buffer.distance[current_buffer_buffer_idx][i][j] = 0;
				pixel_distance_background[i][j] = 0;
//              frame_buffer.signal_per_spad[current_buffer_buffer_idx][i][j] = 0;
//              frame_buffer.status[current_buffer_buffer_idx][i][j] = 0;
//              frame_buffer.motion_indicator[current_buffer_buffer_idx][i][j] = 0;
//              frame_buffer.reflectance[current_buffer_buffer_idx][i][j] = 0;
			} else {
				frame_buffer.distance[current_buffer_buffer_idx][i][j] =
						Results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * idx];
				pixel_distance_background[i][j] = max_value_mean - Results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * idx];
//              frame_buffer.signal_per_spad[current_buffer_buffer_idx][i][j] = Results.signal_per_spad[VL53L5CX_NB_TARGET_PER_ZONE * idx];
//              frame_buffer.status[current_buffer_buffer_idx][i][j] = Results.target_status[VL53L5CX_NB_TARGET_PER_ZONE * idx];
//              frame_buffer.motion_indicator[current_buffer_buffer_idx][i][j] = Results.motion_indicator.motion[motion_config.map_id[idx]];
//              frame_buffer.reflectance[current_buffer_buffer_idx][i][j] = Results.reflectance[VL53L5CX_NB_TARGET_PER_ZONE * idx];
			}
		}
	}

	vl53l5_data_ready = 0;
}

void process_frame_data() {
	// Buffer is full, process all frames
	uint16_t ai_data_buffer_idx = 0;
	for (uint8_t frame = 0; frame < FRAMES; frame++) {
		for (uint8_t i = 0; i < ROWS; i++) {
			for (uint8_t j = 0; j < COLS; j++) {
//              printf("cming\n");
				if (frame_buffer.distance[frame][i][j] != 0) {
//					if (frame_buffer.distance[frame][i][j] <= 1100) {
					if (pixel_distance_background[i][j] > 1330){
						aiInData[ai_data_buffer_idx++] = 3;
//					} else if (frame_buffer.distance[frame][i][j] > 1100
//							&& frame_buffer.distance[frame][i][j] <= 1600) {
					} else if (pixel_distance_background[i][j] > 830 && pixel_distance_background[i][j] <= 1330){
						aiInData[ai_data_buffer_idx++] = 2;
					} else {
						aiInData[ai_data_buffer_idx++] = 1;
					}
				} else {
					aiInData[ai_data_buffer_idx++] = 0;
				}
			}
		}
	}
	AI_Run(aiInData, aiOutData);
	// Reset frame counter
	frame_buffer.frame_count = 0;
}

void push_if_valid(int (*stack)[2], int *top, int nx, int ny, uint16_t visited_pixels[ROWS][COLS])
{

    if (*top >= ROWS * COLS)
    {
      return;  // ✅ Prevent stack overflow
    }
    if (nx >= 0 && nx < ROWS && ny >= 0 && ny < COLS && visited_pixels[nx][ny] == 0)
    {
        visited_pixels[nx][ny] = 1;
      stack[*top][0] = nx;
        stack[*top][1] = ny;
        (*top)++;
    }
}

void label_components(uint16_t current_frame[ROWS][COLS]) {
	uint8_t current_label = 0;
	uint16_t visited_pixels[ROWS][COLS] = {0};
  int stack[ROWS * COLS][2];
	for (int i = 0; i < ROWS; i++) {
		for (int j = 0; j < COLS; j++) {
			if (current_frame[i][j] != 0 && labeled_frame[i][j] == 0) {
				current_label++;
				int top = 0;
				stack[top][0] = i;
				stack[top][1] = j;
				top++;

				components[component_count].label = current_label;
				components[component_count].size = 0;
				components[component_count].box.x1 = j;
				components[component_count].box.y1 = i;
				components[component_count].box.x2 = j;
				components[component_count].box.y2 = i;
				components[component_count].min_distance = 4000;
				components[component_count].max_distance = 0;
				components[component_count].second_max_distance = 0;
				memset(components[component_count].depth_profile, 0,
						sizeof(components[component_count].depth_profile));

				while (top > 0) {
					top--;
					int x = stack[top][0], y = stack[top][1];

					if (x < 0 || x >= ROWS || y < 0 || y >= COLS
							|| current_frame[x][y] == 0
							|| labeled_frame[x][y] != 0)
						continue;
					visited_pixels[x][y] = 1;
					labeled_frame[x][y] = current_label;
					components[component_count].size++;
					if (y < components[component_count].box.x1)
						components[component_count].box.x1 = y;
					if (y > components[component_count].box.x2)
						components[component_count].box.x2 = y;
					if (x < components[component_count].box.y1)
						components[component_count].box.y1 = x;
					if (x > components[component_count].box.y2)
						components[component_count].box.y2 = x;

					if (current_frame[x][y]
							< components[component_count].min_distance) {
						components[component_count].min_distance =
								current_frame[x][y];
					}
					if (current_frame[x][y]
							> components[component_count].max_distance) {
						// Shift the current max to second max
						components[component_count].second_max_distance =
								components[component_count].max_distance;
						components[component_count].max_distance =
								current_frame[x][y];
					} else if (current_frame[x][y]
							> components[component_count].second_max_distance
							&& current_frame[x][y]
									< components[component_count].max_distance) {
						// Update second max if current value is between max and second max
						components[component_count].second_max_distance =
								current_frame[x][y];
					}

          push_if_valid(stack, &top, x - 1, y, visited_pixels);
          push_if_valid(stack, &top, x + 1, y, visited_pixels);
          push_if_valid(stack, &top, x, y - 1, visited_pixels);
          push_if_valid(stack, &top, x, y + 1, visited_pixels);
          push_if_valid(stack, &top, x - 1, y - 1, visited_pixels);
          push_if_valid(stack, &top, x + 1, y + 1, visited_pixels);
          push_if_valid(stack, &top, x + 1, y - 1, visited_pixels);
          push_if_valid(stack, &top, x - 1, y + 1, visited_pixels);
				}
				if (components[component_count].size >= 2) {
					component_count++;
				}
			}
		}
	}
}

void update_tracks(void) {
	// 2) Mark all tracks as unmatched (increment inactive counter)
	for (int i = 0; i < track_count; i++) {
		tracks[i].inactive_frames++;
	}

	// 3) Build a list of all (component, track) pairs with distances
	MatchPair match_pairs[MAX_COMPONENTS * MAX_TRACKS];
	int pair_count = 0;
	memset(match_pairs, 0, sizeof(match_pairs));

	for (int c = 0; c < component_count; c++) {
		// center of component bounding box
		float comp_center_row = (float) (components[c].box.y1
				+ components[c].box.y2) / 2.0f;
		float comp_center_col = (float) (components[c].box.x1
				+ components[c].box.x2) / 2.0f;

		for (int t = 0; t < track_count; t++) {
			if (tracks[t].active) {
				float row_diff = comp_center_row
						- (float) tracks[t].current_row;
				float col_diff = comp_center_col
						- (float) tracks[t].current_col;
				float distance = sqrtf(
						row_diff * row_diff + col_diff * col_diff);

				match_pairs[pair_count].comp_idx = c;
				match_pairs[pair_count].track_idx = t;
				match_pairs[pair_count].distance = distance;
				pair_count++;
			}
		}
	}

	// 4) Sort the pairs by ascending distance
	for (int i = 0; i < pair_count - 1; i++) {
		for (int j = i + 1; j < pair_count; j++) {
			if (match_pairs[i].distance > match_pairs[j].distance) {
				MatchPair tmp = match_pairs[i];
				match_pairs[i] = match_pairs[j];
				match_pairs[j] = tmp;
			}
		}
	}

	// 5) Arrays to mark if a component or a track has been matched
	bool comp_matched[MAX_COMPONENTS];
	bool track_matched[MAX_TRACKS];
	memset(comp_matched, false, sizeof(comp_matched));
	memset(track_matched, false, sizeof(track_matched));

	// 6) Greedily match component & track in ascending distance
	for (int i = 0; i < pair_count; i++) {
		int c = match_pairs[i].comp_idx;
		int t = match_pairs[i].track_idx;
		float dist = match_pairs[i].distance;

		if (dist <= MATCH_DISTANCE_THRESHOLD && !comp_matched[c]
				&& !track_matched[t]) {
			// We have a match
			comp_matched[c] = true;
			track_matched[t] = true;

			// Update existing track
			tracks[t].previous_row = tracks[t].current_row;
			tracks[t].previous_col = tracks[t].current_col;

			// recast to float -> int
			tracks[t].current_row =
					(components[c].box.y1 + components[c].box.y2) / 2;
			tracks[t].current_col =
					(components[c].box.x1 + components[c].box.x2) / 2;
			tracks[t].inactive_frames = 0;
			tracks[t].duration_frames++;
		}
	}

	// 7) Create new tracks for unmatched components
	for (int c = 0; c < component_count; c++) {
		if (!comp_matched[c] && track_count < MAX_TRACKS) {
			float comp_center_row = (float) (components[c].box.y1
					+ components[c].box.y2) / 2.0f;
			float comp_center_col = (float) (components[c].box.x1
					+ components[c].box.x2) / 2.0f;

			tracks[track_count].id = next_id++;
			tracks[track_count].current_row = (int) (comp_center_row);
			tracks[track_count].current_col = (int) (comp_center_col);
			tracks[track_count].previous_row = tracks[track_count].current_row;
			tracks[track_count].previous_col = tracks[track_count].current_col;

			tracks[track_count].active = 1;
			tracks[track_count].inactive_frames = 0;
//            tracks[track_count].counted_in = false;
//            tracks[track_count].counted_out = false;
//            tracks[track_count].duration_frames = 1;

			track_count++;
		}
	}

	// 8) Check for track timeouts or counting logic
	int active_count = 0;
	for (int t = 0; t < track_count; t++) {
		if (tracks[t].active) {
			// Inactive for too long => remove
			if (tracks[t].inactive_frames > MAX_INACTIVE_FRAMES) {
				// Mark track as inactive
				tracks[t].active = 0;
				tracks[t].duration_frames = 1;
				if (tracks[t].counted_in) {
					// Mark that one left
					People_Data.people_out++;
				}
				tracks[t].counted_in = false;
				tracks[t].counted_out = false;
			} else {
				// Example: if track has not yet been "counted_in" after X frames
				if (!tracks[t].counted_in && tracks[t].duration_frames > 8) {
					People_Data.people_in++;
					tracks[t].counted_in = true;
				}

				// Keep this track in the active list
				if (tracks[t].active) {
					if (t != active_count) {
						tracks[active_count] = tracks[t];
					}
					active_count++;
				}
			}
		}
	}
	track_count = active_count;

	// 9) Update People_Data.people_count based on which tr
	person_info_count = 0;
	uint8_t stable_count = 0;
	for (int t = 0; t < component_count; t++) {
		if (tracks[t].duration_frames > 4) {
			person_info[person_info_count].id = tracks[t].id;
			person_info[person_info_count].x = tracks[t].current_col;
			person_info[person_info_count].y = tracks[t].current_row;
			person_info[person_info_count].duration_frames =
					tracks[t].duration_frames;
			person_info_count++;
			stable_count++;
		}
	}
	People_Data.people_count = stable_count;
	if (People_Data.people_count >= 2)
		People_Data.people_count = 2;
}

void create_depth_profiles() {
	// First pass: Create initial depth profile
	memset(combined_depth_profile, 0, sizeof(combined_depth_profile));
	for (int c = 0; c < component_count; c++) {
		for (int i = components[c].box.y1; i <= components[c].box.y2; i++) {
			for (int j = components[c].box.x1; j <= components[c].box.x2; j++) {
				if (i == 0 || i == 1 || i == 2 || i == 3) {
					scale = 300.0f
							/ ((float) components[c].second_max_distance
									- (float) components[c].min_distance);

					// Limit scale to a maximum of 1.0f
					if (scale > 1.0f) {
						scale = 1.0f;
					}
				} else {
					scale = 1.0f;
				}
				uint16_t distance = (int) roundf(
						(float) frame_buffer.distance[0][i][j] * scale);
				uint16_t local_max = (int) roundf(
						(float) components[c].second_max_distance * scale);
				if (labeled_frame[i][j] == components[c].label) {
					if (frame_buffer.distance[0][i][j]
							<= components[c].min_distance + DEPTH_THRESHOLD) {
						combined_depth_profile[i][j] = 2; // Head or close to head
					} else {
						combined_depth_profile[i][j] = 1; // Body or shoulder
					}
					// Second pass: refine categorization based on second_max_distance if currently marked as 1
					if (combined_depth_profile[i][j] == 1) {
						if (distance >= local_max - 200) {
//                        if (distance >= 4000) {
							combined_depth_profile[i][j] = 3; // Remain body or shoulder
						} else {
							combined_depth_profile[i][j] = 1; // Closer to head region
						}
					}
				}
			}
		}
	}

	if (component_count == 1) {
		memset(labeled_frame, 0, sizeof(labeled_frame));
		component_count = 0;
		uint16_t head_frame[ROWS][COLS] = { 0 };
		uint16_t head_frame_2[ROWS][COLS] = { 0 };

		for (int i = 0; i < ROWS; i++) {
			for (int j = 0; j < COLS; j++) {
				if (combined_depth_profile[i][j] == 2) {
					head_frame[i][j] = 1;
				}
			}
		}
		label_components(head_frame);
		if (component_count <= 1) {
			for (int i = 0; i < ROWS; i++) {
				for (int j = 0; j < COLS; j++) {
					if (combined_depth_profile[i][j] == 1) {
						head_frame_2[i][j] = 1;
					}
				}
			}
			label_components(head_frame_2);
		}
		if (component_count == 0)
			component_count++;
	}
	//    for (int i = 0; i < ROWS; i++) {
	//        for (int j = 0; j < COLS; j++) {
	//            printf("%d ", combined_depth_profile[i][j]);
	//        }
	//    }
	//    printf("\n");
}

void update_people_out(uint8_t *current_people_out) {
	static uint8_t people_out_history[HISTORY_SIZE] = { 0 }; // History buffer
	static uint8_t history_buffer_idx = 0;
	// Update the history buffer
	people_out_history[history_buffer_idx] = *current_people_out;
	history_buffer_idx = (history_buffer_idx + 1) % HISTORY_SIZE;

	// Count occurrences of each class in the buffer
	uint8_t count[256] = { 0 }; // Assuming 256 possible classes (uint8_t range)
	for (int i = 0; i < HISTORY_SIZE; i++) {
		count[people_out_history[i]]++;
	}

	// Find the most frequent class
	uint8_t max_count = 0;
	uint8_t most_frequent_class = 0;
	for (int i = 0; i < 256; i++) {
		if (count[i] > max_count) {
			max_count = count[i];
			most_frequent_class = i;
		}
	}

	// Update the current value with the most frequent class
	*current_people_out = most_frequent_class;
}

void ai_output_moving_average(float *aiOutData) {
	static uint8_t previous_class = 0;
	static uint8_t is_falling_down = 0;
	static uint8_t fall_counter = 0;
	// Add the current aiOutData to the buffer
	for (int i = 0; i < NUM_CLASSES; i++) {
		output_buffer[buffer_buffer_idx][i] = aiOutData[i];
	}

	// Update the buffer buffer_idx and count
	buffer_buffer_idx = (buffer_buffer_idx + 1) % HISTORY_SIZE;
	if (count < HISTORY_SIZE)
		count++;

	// Calculate the moving average
	float smoothed_output[NUM_CLASSES] = { 0 };
	for (int i = 0; i < count; i++) {
		for (int j = 0; j < NUM_CLASSES; j++) {
			smoothed_output[j] += output_buffer[i][j];
		}
	}

	// Average the sum and store back into aiOutData
	for (int j = 0; j < NUM_CLASSES; j++) {
		aiOutData[j] = smoothed_output[j] / count;
	}
	People_Data.class = argmax(aiOutData, AI_NETWORK_OUT_1_SIZE) + 1;
	// Check for falling down state
	if ((previous_class == 2 || previous_class == 3)
			&& People_Data.class == 1) {
		// Falling down: Standing/Sitting -> Lying floor
		is_falling_down = 1; // Set falling down state
		fall_counter = 1;    // Initialize fall counter
		People_Data.class = 4;
	} else if (is_falling_down && People_Data.class == 1) {
		// Continue falling down state for consecutive frames
		People_Data.class = 4;
		fall_counter++;

		if (fall_counter >= 6) {
			// Stabilize as "lying floor" after 4 frames
			is_falling_down = 0;
			fall_counter = 0;
		}
	} else {
		// Reset falling state if conditions are not met
		is_falling_down = 0;
		fall_counter = 0;
	}
	previous_class = People_Data.class;
}

void send_data_uart() {

	if (!background_initialized) {
		// ============================
		// Distance Data
		// ============================
		if (distance_flag == true) {
			buffer_idx = 0;
			checksum = 0;
			// Header: 'FUT0'
			buffer[buffer_idx++] = 'F';
			buffer[buffer_idx++] = 'U';
			buffer[buffer_idx++] = 'T';
			buffer[buffer_idx++] = '0';
			buffer[buffer_idx++] = 0xA3; // type for Distance Data
			buffer[buffer_idx++] = 128; // Byte length (128 bytes for 64 pixels)
			// Add 64 distance values (each 2 bytes)
			//		for (int i = 0; i < ROWS; i++) {
			//			for (int j = 0; j < COLS; j++) {
			//			  buffer[buffer_idx++] = (uint8_t)(combined_depth_profile[i][j] & 0xFF);    // Low byte
			//			  buffer[buffer_idx++] = (uint8_t)((combined_depth_profile[i][j] >> 8) & 0xFF); // High byte
			//			}
			//		}
			for (uint8_t k = 0; k < 64; k++) {
				uint16_t distance =
						Results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * k];
				buffer[buffer_idx++] = (uint8_t) ((distance >> 8) & 0xFF); // High byte
				buffer[buffer_idx++] = (uint8_t) (distance & 0xFF);       // Low byte
			}
			// Calculate checksum
			for (uint8_t i = 4; i < buffer_idx; i++) {
				checksum ^= buffer[i];
			}

			buffer[buffer_idx++] = checksum; // Add checksum
			// Footer: 'FUT0'
			buffer[buffer_idx++] = 'E';
			buffer[buffer_idx++] = 'N';
			buffer[buffer_idx++] = 'D';
			buffer[buffer_idx++] = '0';
			buffer[buffer_idx++] = '\n';
			cdc_send();
			HAL_UART_Transmit(&huart1, buffer, buffer_idx, HAL_MAX_DELAY);
		}
		// ============================
		// In/Out Data
		// ============================
		buffer_idx = 0;
		checksum = 0;
		// Header: 'FUT0'
		buffer[buffer_idx++] = 'F';
		buffer[buffer_idx++] = 'U';
		buffer[buffer_idx++] = 'T';
		buffer[buffer_idx++] = '0';
		buffer[buffer_idx++] = 0xA4; // Header for In/Out Data
		buffer[buffer_idx++] = 4;    // Byte length (2 bytes for In, 2 bytes for Out)

		// Add People In and Out Data
		buffer[buffer_idx++] = (uint8_t) ((People_Data.people_in >> 8) & 0xFF);
		buffer[buffer_idx++] = (uint8_t) (People_Data.people_in & 0xFF);
		buffer[buffer_idx++] = (uint8_t) ((People_Data.people_out >> 8) & 0xFF);
		buffer[buffer_idx++] = (uint8_t) (People_Data.people_out & 0xFF);

		// Calculate checksum
		for (uint8_t i = 4; i < buffer_idx; i++) {
			checksum ^= buffer[i];
		}

		buffer[buffer_idx++] = checksum; // Add checksum
		// Footer: 'FUT0'
		buffer[buffer_idx++] = 'E';
		buffer[buffer_idx++] = 'N';
		buffer[buffer_idx++] = 'D';
		buffer[buffer_idx++] = '0';
		buffer[buffer_idx++] = '\n';
		cdc_send();
		HAL_UART_Transmit(&huart1, buffer, buffer_idx, HAL_MAX_DELAY);
		// ============================
		// Person Info Data
		// ============================
		buffer_idx = 0;
		checksum = 0;
		// Header: 'FUT0'
		buffer[buffer_idx++] = 'F';
		buffer[buffer_idx++] = 'U';
		buffer[buffer_idx++] = 'T';
		buffer[buffer_idx++] = '0';
		buffer[buffer_idx++] = 0xA5; // Header for Person Info
		buffer[buffer_idx++] = 1 + (People_Data.people_count * 5); // Byte length (8 bytes per person)
		//  buffer[buffer_idx++] = component_count;
		buffer[buffer_idx++] = People_Data.people_count;          // Person ID
		for (int i = 0; i < People_Data.people_count && i < 2; i++) {
			uint32_t total_seconds = ((uint32_t) person_info[i].duration_frames
					/ DISTANCE_ODR);
			buffer[buffer_idx++] = (uint8_t) People_Data.class; // State
			buffer[buffer_idx++] = (uint8_t) person_info[i].x; // X Position
			buffer[buffer_idx++] = (uint8_t) person_info[i].y; // Y Position
			buffer[buffer_idx++] = (uint8_t) ((total_seconds >> 8) & 0xFF); // Byte 1
			buffer[buffer_idx++] = (uint8_t) (total_seconds & 0xFF); // Byte 0 (Least Significant Byte)
		}
		for (uint8_t i = 4; i < buffer_idx; i++) {
			checksum ^= buffer[i];
		}
		buffer[buffer_idx++] = checksum; // Add checksum
		// Footer: 'FUT0'
		buffer[buffer_idx++] = 'E';
		buffer[buffer_idx++] = 'N';
		buffer[buffer_idx++] = 'D';
		buffer[buffer_idx++] = '0';
		buffer[buffer_idx++] = '\n';
		cdc_send();
		HAL_UART_Transmit(&huart1, buffer, buffer_idx, HAL_MAX_DELAY);
	}

	else {
		// ============================
		// Background Data
		// ============================
		buffer_idx = 0;
		checksum = 0;
		// Header: 'FUT0'
		buffer[buffer_idx++] = 'F';
		buffer[buffer_idx++] = 'U';
		buffer[buffer_idx++] = 'T';
		buffer[buffer_idx++] = '0';
		buffer[buffer_idx++] = 0xA6; // Header for In/Out Data
		buffer[buffer_idx++] = 1;
		buffer[buffer_idx++] = background_initialized;
		for (uint8_t i = 4; i < buffer_idx; i++) {
			checksum ^= buffer[i];
		}
		buffer[buffer_idx++] = checksum; // Add checksum
		// Footer: 'FUT0'
		buffer[buffer_idx++] = 'E';
		buffer[buffer_idx++] = 'N';
		buffer[buffer_idx++] = 'D';
		buffer[buffer_idx++] = '0';
		buffer[buffer_idx++] = '\n';
		cdc_send();
		HAL_UART_Transmit(&huart1, buffer, buffer_idx, HAL_MAX_DELAY);
	}
}

void cdc_send(void)
{
	uint8_t status;
	uint32_t actual_length = 0;
	static uint32_t dataflag = 0;
//	status = ux_device_class_cdc_acm_write_with_callback(
//	    cdc_acm,
//	    (UCHAR *)buffer,
//	    (ULONG) buffer_idx);
//	if(status != UX_SUCCESS)
//	    while(1);
	if (NULL == cdc_acm) return;
	if (0 == cdc_acm->ux_slave_class_cdc_acm_data_rts_state) return;

	status = ux_device_class_cdc_acm_write(cdc_acm, (UCHAR *)buffer,
			(ULONG)buffer_idx, &actual_length);

//	if(status != UX_SUCCESS)
//		while(1);
//	tx_event_flags_set(&CDC_EVENT_FLAG, CDC_TX_REQ, TX_OR);
//	tx_event_flags_get(&CDC_EVENT_FLAG, CDC_TX_CPLT, TX_OR_CLEAR,
//	                           &dataflag, TX_WAIT_FOREVER);
}

VOID usbx_cdc_acm_read_thread_entry(ULONG thread_input)
{
  ULONG actual_length;
  UX_SLAVE_DEVICE *device;

  UX_PARAMETER_NOT_USED(thread_input);

  device = &_ux_system_slave->ux_system_slave_device;

  while (1)
  {
    /* Check if device is configured */
    if ((device->ux_slave_device_state == UX_DEVICE_CONFIGURED) && (cdc_acm != UX_NULL))
    {

#ifndef UX_DEVICE_CLASS_CDC_ACM_TRANSMISSION_DISABLE

      /* Set transmission_status to UX_FALSE for the first time */
      cdc_acm -> ux_slave_class_cdc_acm_transmission_status = UX_FALSE;

#endif /* UX_DEVICE_CLASS_CDC_ACM_TRANSMISSION_DISABLE */

      /* Read the received data in blocking mode */
      ux_device_class_cdc_acm_read(cdc_acm, (UCHAR *)received_data, COMMAND_LENGTH,
                                   &actual_length);

      uint8_t byte_length = received_data[5]; // Extract byte length
      uint8_t calculated_checksum = received_data[4]; // Start with the Type field

      if ((strncmp((char*) received_data, "FUT0", 4) != 0) ||
    	  (strncmp((char*) &received_data[6 + byte_length + 1], "END0", 4) != 0))
      {
    	  // wrong header and footer. Do nothing
	  }
      else
      {

		calculated_checksum ^= byte_length;
		calculated_checksum ^= received_data[6];

		// Extract received checksum
		uint8_t received_checksum = received_data[6 + byte_length];

		// Verify the checksum
		if (calculated_checksum != received_checksum) {
			// invalid checksum. Do nothing
		}
		else
		{
			// Handle the packet based on the type
		if (received_data[4] == 0xA1 && received_data[6] == 0x01)
				background_initialized = true;
      else if (received_data[4] == 0xA2 && received_data[6] == 0x02)
        distance_flag = false;
      else if (received_data[4] == 0xA2 && received_data[6] == 0x01)
        distance_flag = true;
      else if (received_data[4] == 0xA8 && received_data[6] == 0x01)
      {
        boot_flag = 0xDEADBEEF;
        NVIC_SystemReset();
      }
		}
      }

    }
    else
    {
      /* Sleep thread for 10ms */
      tx_thread_sleep(MS_TO_TICK(10));
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		// Validate the header ("FUT0")
		if (strncmp((char*) received_data, "FUT0", 4) != 0) {
			// Invalid header, discard the packet
			HAL_UART_Receive_IT(&huart1, (uint8_t*) received_data,
					COMMAND_LENGTH);
			return;
		}

		uint8_t byte_length = received_data[5]; // Extract byte length
		uint8_t calculated_checksum = received_data[4]; // Start with the Type field

		// Validate the footer ("END0")
		if (strncmp((char*) &received_data[6 + byte_length + 1], "END0", 4)
				!= 0) {
			// Invalid footer, discard the packet
			HAL_UART_Receive_IT(&huart1, (uint8_t*) received_data,
					COMMAND_LENGTH);
			return;
		}

		calculated_checksum ^= byte_length;
		// Validate checksum
//        for (uint8_t i = 6; i < 6 + byte_length; i++) {   // Start from the first data byte
		calculated_checksum ^= received_data[6];
//        }

		// Extract received checksum
		uint8_t received_checksum = received_data[6 + byte_length];

		// Verify the checksum
		if (calculated_checksum != received_checksum) {
			// Checksum mismatch, discard the packet
			HAL_UART_Receive_IT(&huart1, (uint8_t*) received_data,
					COMMAND_LENGTH);
			return;
		}

		// Handle the packet based on the type
		if (received_data[4] == 0xA1 && received_data[6] == 0x01) {
			// Handle Start Calibration packet
			background_initialized = true;
		} else if (received_data[4] == 0xA2 && received_data[6] == 0x02) {
			distance_flag = false;
//			SaveDistanceFlagToFlash(distance_flag);

		} else if (received_data[4] == 0xA2 && received_data[6] == 0x01) {
			distance_flag = true;
//		  SaveDistanceFlagToFlash(distance_flag);
		}

		// Restart UART reception for the next packet
		HAL_UART_Receive_IT(&huart1, (uint8_t*) received_data, COMMAND_LENGTH);
	}
}

void User_Btn_ElapsedCallback(TIM_HandleTypeDef *htim)
{
  uint8_t pin_state = (HAL_GPIO_ReadPin(USER_BTN_GPIO_Port, USER_BTN_Pin) == GPIO_PIN_SET);

  switch (btn_state)
  {
  case RISING_EDGE:
      if (pin_state == 0)
      {
        btn_state = DEBOUNCING;
      }
      break;
  case DEBOUNCING:
      if (pin_state == 0)
      {
        btn_state = FALLING_EDGE;
      }
      break;
  case FALLING_EDGE:
      if (pin_state == 1)
      {
        btn_state = RISING_EDGE;
        boot_flag = 0xDEADBEEF;
        NVIC_SystemReset();
      }
      break;
  }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
	switch (GPIO_Pin) {
	case VL53_INT_Pin:
	    vl53l5_data_ready = 1;
		tx_event_flags_set(&TOF_EVENT_FLAG, TOF_DATA_RDY, TX_OR);
		break;
	}
}

