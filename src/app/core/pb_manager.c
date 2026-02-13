#include "pb_manager.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp_uart.h"
#include "pb_encode.h"
#include "tof.pb.h"

#define PB_MAX_PROTO_PAYLOAD_SIZE tof_result_size

static uint8_t s_pb_payload_buffer[PB_MAX_PROTO_PAYLOAD_SIZE];

static void pb_fill_people_info(const tof_pipeline_output_t *pipeline_output, tof_result *result)
{
    uint16_t people_in;
    uint16_t people_out;
    uint8_t people_count;

    people_count = pipeline_output->smoothed_people_count;
    people_in = pipeline_output->people.people_in;
    people_out = pipeline_output->people.people_out;

    result->people.people_count.size = 1U;
    result->people.people_count.bytes[0] = people_count;

    result->people.people_in.size = 2U;
    result->people.people_in.bytes[0] = (uint8_t)((people_in >> 8) & 0xFFU);
    result->people.people_in.bytes[1] = (uint8_t)(people_in & 0xFFU);

    result->people.people_out.size = 2U;
    result->people.people_out.bytes[0] = (uint8_t)((people_out >> 8) & 0xFFU);
    result->people.people_out.bytes[1] = (uint8_t)(people_out & 0xFFU);
}

static void pb_fill_person_info(const tof_pipeline_output_t *pipeline_output, tof_result *result)
{
    uint8_t tx_count = pipeline_output->person_info_count;

    if (tx_count > TOF_MAX_PEOPLE_COUNT)
    {
        tx_count = TOF_MAX_PEOPLE_COUNT;
    }

    result->person_count = tx_count;
    for (uint8_t i = 0U; i < tx_count; i++)
    {
        result->person[i].id = pipeline_output->person_info[i].id;
        result->person[i].x = pipeline_output->person_info[i].x;
        result->person[i].y = pipeline_output->person_info[i].y;
        result->person[i].duration_frames = pipeline_output->person_info[i].duration_frames;
        result->person[i].class_id.size = 1U;
        result->person[i].class_id.bytes[0] = pipeline_output->people.class_id;
    }
}

static bool pb_encode_result_message(const tof_pipeline_output_t *pipeline_output, uint8_t *encoded_buf,
                                     uint16_t encoded_buf_size, uint16_t *encoded_len)
{
    pb_ostream_t stream;
    tof_result result = tof_result_init_zero;

    if ((pipeline_output == NULL) || (encoded_buf == NULL) || (encoded_len == NULL))
    {
        return false;
    }

    pb_fill_people_info(pipeline_output, &result);
    pb_fill_person_info(pipeline_output, &result);

    stream = pb_ostream_from_buffer(encoded_buf, encoded_buf_size);
    if (!pb_encode(&stream, tof_result_fields, &result))
    {
        return false;
    }

    *encoded_len = (uint16_t)stream.bytes_written;
    return true;
}

void send_pb_result(const VL53L5CX_ResultsData *raw_frame, const tof_pipeline_output_t *pipeline_output)
{
    uint16_t encoded_len = 0U;

    (void)raw_frame;

    if (pipeline_output == NULL)
    {
        return;
    }
    if (pipeline_output->background_collecting)
    {
        return;
    }

    if (!pb_encode_result_message(pipeline_output, s_pb_payload_buffer, sizeof(s_pb_payload_buffer), &encoded_len))
    {
        return;
    }

    bsp_uart_send(s_pb_payload_buffer, encoded_len);
}
