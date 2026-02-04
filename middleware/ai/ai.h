/*
 * ai_func.h
 *
 *  Created on: Nov 25, 2024
 *      Author: BingHui.Lai
 */

#ifndef AI_FUNC_AI_FUNC_H_
#define AI_FUNC_AI_FUNC_H_

#include "ai_platform.h"
#include "network.h"
#include "network_data.h"
void AI_Init(void);
void AI_Run(float *pIn, float *pOut);
int argmax(const float *values, uint32_t len);
#endif /* _AI_FUNC_H_ */
