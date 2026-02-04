#include "connection_manager.h"

#include <stddef.h>
#include <string.h>

#include "bsp_serial.h"
#include "vl53l5cx.h"

#define CONN_PACKET_MAX_SIZE 220U
#define CONN_FRAME_PIXELS (TOF_ROWS * TOF_COLS)

#define CONN_TYPE_BUNDLE 0xAFU
#define CONN_TYPE_DISTANCE_DATA 0xA3U
#define CONN_TYPE_IN_OUT_DATA 0xA4U
#define CONN_TYPE_PERSON_INFO 0xA5U
#define CONN_TYPE_BG_STATUS 0xA6U

#define CONN_CMD_BG_REINIT 0xA1U
#define CONN_CMD_DISTANCE_STREAM 0xA2U

static bool s_distance_stream_enabled = true;
static volatile bool s_request_bg_reinit = false;
static uint8_t s_tx_buffer[CONN_PACKET_MAX_SIZE];

static void conn_rx_callback(uint8_t *packet, uint32_t packet_len)
{
    bool request_bg_reinit = false;

    if (conn_handle_command(packet, (uint16_t)packet_len, &request_bg_reinit) &&
        request_bg_reinit) {
        s_request_bg_reinit = true;
    }
}

static void conn_send_packet(uint8_t type, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t idx = 0U;
    uint8_t checksum = 0U;

    if ((payload_len + 11U) > CONN_PACKET_MAX_SIZE) {
        return;
    }

    s_tx_buffer[idx++] = 'F';
    s_tx_buffer[idx++] = 'U';
    s_tx_buffer[idx++] = 'T';
    s_tx_buffer[idx++] = '0';
    s_tx_buffer[idx++] = type;
    s_tx_buffer[idx++] = payload_len;

    for (uint8_t i = 0U; i < payload_len; i++) {
        s_tx_buffer[idx++] = payload[i];
    }

    for (uint8_t i = 4U; i < idx; i++) {
        checksum ^= s_tx_buffer[i];
    }
    s_tx_buffer[idx++] = checksum;
    s_tx_buffer[idx++] = 'E';
    s_tx_buffer[idx++] = 'N';
    s_tx_buffer[idx++] = 'D';
    s_tx_buffer[idx++] = '0';
    s_tx_buffer[idx++] = '\n';

    bsp_serial_tx_data(s_tx_buffer, idx);
}

static bool conn_append_section(uint8_t *payload,
                                uint8_t *payload_idx,
                                uint8_t payload_max,
                                uint8_t section_type,
                                const uint8_t *section_data,
                                uint8_t section_len)
{
    if ((payload == NULL) || (payload_idx == NULL)) {
        return false;
    }
    if ((section_len > 0U) && (section_data == NULL)) {
        return false;
    }
    if ((uint16_t)(*payload_idx + section_len + 3U) > payload_max) {
        return false;
    }

    payload[(*payload_idx)++] = section_type;
    payload[(*payload_idx)++] = section_len;
    for (uint8_t i = 0U; i < section_len; i++) {
        payload[(*payload_idx)++] = section_data[i];
    }
    payload[(*payload_idx)++] = section_type;
    return true;
}

static bool conn_append_distance_section(const VL53L5CX_ResultsData *raw_frame,
                                         uint8_t *payload,
                                         uint8_t *payload_idx,
                                         uint8_t payload_max)
{
    uint8_t distance_payload[CONN_FRAME_PIXELS * 2U] = {0};
    uint8_t idx = 0U;

    if (raw_frame == NULL) {
        return false;
    }

    for (uint8_t k = 0U; k < CONN_FRAME_PIXELS; k++) {
        uint16_t distance = raw_frame->distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * k];
        distance_payload[idx++] = (uint8_t)((distance >> 8) & 0xFFU);
        distance_payload[idx++] = (uint8_t)(distance & 0xFFU);
    }

    return conn_append_section(payload,
                               payload_idx,
                               payload_max,
                               CONN_TYPE_DISTANCE_DATA,
                               distance_payload,
                               sizeof(distance_payload));
}

static bool conn_append_in_out_section(const tof_people_data_t *people,
                                       uint8_t *payload,
                                       uint8_t *payload_idx,
                                       uint8_t payload_max)
{
    uint8_t inout_payload[4];

    if (people == NULL) {
        return false;
    }

    inout_payload[0] = (uint8_t)((people->people_in >> 8) & 0xFFU);
    inout_payload[1] = (uint8_t)(people->people_in & 0xFFU);
    inout_payload[2] = (uint8_t)((people->people_out >> 8) & 0xFFU);
    inout_payload[3] = (uint8_t)(people->people_out & 0xFFU);

    return conn_append_section(payload,
                               payload_idx,
                               payload_max,
                               CONN_TYPE_IN_OUT_DATA,
                               inout_payload,
                               sizeof(inout_payload));
}

static bool conn_append_person_section(const tof_people_data_t *people,
                                       const tof_person_info_t *person_info,
                                       uint8_t person_count,
                                       uint8_t *payload,
                                       uint8_t *payload_idx,
                                       uint8_t payload_max)
{
    uint8_t person_payload[1U + (2U * 5U)] = {0};
    uint8_t tx_count = person_count;
    uint8_t idx = 0U;

    if ((people == NULL) || (person_info == NULL)) {
        return false;
    }

    if (tx_count > 2U) {
        tx_count = 2U;
    }

    person_payload[idx++] = tx_count;
    for (uint8_t i = 0U; i < tx_count; i++) {
        uint32_t total_seconds = person_info[i].duration_frames / DISTANCE_ODR;
        person_payload[idx++] = people->class_id;
        person_payload[idx++] = (uint8_t)person_info[i].x;
        person_payload[idx++] = (uint8_t)person_info[i].y;
        person_payload[idx++] = (uint8_t)((total_seconds >> 8) & 0xFFU);
        person_payload[idx++] = (uint8_t)(total_seconds & 0xFFU);
    }

    return conn_append_section(payload,
                               payload_idx,
                               payload_max,
                               CONN_TYPE_PERSON_INFO,
                               person_payload,
                               idx);
}

void conn_init(void)
{
    s_distance_stream_enabled = true;
    s_request_bg_reinit = false;
    bsp_serial_set_rx_cb(conn_rx_callback);
}

void conn_set_distance_stream_enabled(bool enabled)
{
    s_distance_stream_enabled = enabled;
}

bool conn_is_distance_stream_enabled(void)
{
    return s_distance_stream_enabled;
}

bool conn_consume_bg_reinit_request(void)
{
    bool request = s_request_bg_reinit;
    s_request_bg_reinit = false;
    return request;
}

void conn_send_background_status(bool background_collecting)
{
    uint8_t payload[8] = {0};
    uint8_t payload_idx = 0U;
    uint8_t bg_payload[1];

    bg_payload[0] = background_collecting ? 1U : 0U;
    if (!conn_append_section(payload,
                             &payload_idx,
                             sizeof(payload),
                             CONN_TYPE_BG_STATUS,
                             bg_payload,
                             sizeof(bg_payload))) {
        return;
    }

    conn_send_packet(CONN_TYPE_BUNDLE, payload, payload_idx);
}

void conn_send_runtime_data(const VL53L5CX_ResultsData *raw_frame,
                            const tof_people_data_t *people,
                            const tof_person_info_t *person_info,
                            uint8_t person_count)
{
    uint8_t payload[200] = {0};
    uint8_t payload_idx = 0U;

    if (s_distance_stream_enabled) {
        if (!conn_append_distance_section(raw_frame,
                                          payload,
                                          &payload_idx,
                                          sizeof(payload))) {
            return;
        }
    }
    if (!conn_append_in_out_section(people,
                                    payload,
                                    &payload_idx,
                                    sizeof(payload))) {
        return;
    }
    if (!conn_append_person_section(people,
                                    person_info,
                                    person_count,
                                    payload,
                                    &payload_idx,
                                    sizeof(payload))) {
        return;
    }

    conn_send_packet(CONN_TYPE_BUNDLE, payload, payload_idx);
}

bool conn_handle_command(const uint8_t *packet, uint16_t packet_len, bool *request_bg_reinit)
{
    uint8_t byte_len;
    uint8_t expected_checksum = 0U;
    uint8_t received_checksum;
    uint16_t min_packet_len;
    uint16_t footer_idx;

    if ((packet == NULL) || (packet_len < 11U)) {
        return false;
    }

    if (memcmp(packet, "FUT0", 4) != 0) {
        return false;
    }

    byte_len = packet[5];
    min_packet_len = (uint16_t)(byte_len + 11U);
    if ((packet_len != min_packet_len) &&
        !((packet_len == (uint16_t)(min_packet_len + 1U)) && (packet[min_packet_len] == '\n'))) {
        return false;
    }

    footer_idx = (uint16_t)(7U + byte_len);
    if (memcmp(&packet[footer_idx], "END0", 4) != 0) {
        return false;
    }

    for (uint16_t i = 4U; i < (uint16_t)(6U + byte_len); i++) {
        expected_checksum ^= packet[i];
    }
    received_checksum = packet[6U + byte_len];
    if (expected_checksum != received_checksum) {
        return false;
    }

    if ((packet[4] == CONN_CMD_BG_REINIT) && (byte_len >= 1U) && (packet[6] == 0x01U)) {
        if (request_bg_reinit != NULL) {
            *request_bg_reinit = true;
        }
        return true;
    }

    if ((packet[4] == CONN_CMD_DISTANCE_STREAM) && (byte_len >= 1U)) {
        if (packet[6] == 0x01U) {
            conn_set_distance_stream_enabled(true);
            return true;
        }
        if (packet[6] == 0x02U) {
            conn_set_distance_stream_enabled(false);
            return true;
        }
    }

    return false;
}
