/*
 * AI_FUNC.c
 *
 *  Created on: Nov 25, 2024
 *      Author: BingHui.Lai
 */
#include "ai.h"
#include <stdio.h>

ai_handle network;
ai_u8 activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
ai_buffer * ai_input;
ai_buffer * ai_output;

void AI_Init(void)
{
  ai_error err;

  /* Create a local array with the addresses of the activations buffers */
  const ai_handle act_addr[] = { activations };
  /* Create an instance of the model */
  err = ai_network_create_and_init(&network, act_addr, NULL);
  if (err.type != AI_ERROR_NONE) {
    printf("ai_network_create error - type=%d code=%d\r\n", err.type, err.code);
    while (1);
  }
  ai_input = ai_network_inputs_get(network, NULL);
  ai_output = ai_network_outputs_get(network, NULL);
}

void AI_Run(float *pIn, float *pOut)
{
  ai_i32 batch;
  ai_error err;

  /* Update IO handlers with the data payload */
  ai_input[0].data = AI_HANDLE_PTR(pIn);
  ai_output[0].data = AI_HANDLE_PTR(pOut);

  batch = ai_network_run(network, ai_input, ai_output);
  if (batch != 1) {
    err = ai_network_get_error(network);
    printf("AI ai_network_run error - type=%d code=%d\r\n", err.type, err.code);
    while (1);
  }
}

int argmax(const float *values, uint32_t len)
{
  float max_value = values[0];
  uint32_t max_index = 0;
  for (uint32_t i = 1; i < len; i++)
  {
    if (values[i] > max_value)
    {
      max_value = values[i];
      max_index = i;
    }
  }
  return(max_index);
}
