/*
 * Copyright 2023 CEVA, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License and 
 * any applicable agreements you may have with CEVA, Inc.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#define M_PI (3.14159265358979323846264338327950288419716939937510)
#endif

#include "euler.h"

// Functions to convert quaternion into Roll, Pitch and Yaw values.

float q_to_yaw(float r, float i, float j, float k)
{
    // convert to Euler Angles
    float num = 2.0f * i * j - 2.0f * r * k;
    float den = 2.0f * r * r + 2.0f * j * j - 1.0f;

    float yaw = (float)atan2((double)num, (double)den);
    
    return yaw;
}

float q_to_pitch(float r, float i, float j, float k)
{
    // convert to Euler Angles
    float arg = 2.0f * j * k + 2.0f * r * i;
    if (arg > 1.0f) arg = 1.0f;
    if (arg < -1.0f) arg = -1.0f;
    float pitch = (float)asin((double)arg);

    return pitch;
}

float q_to_roll(float r, float i, float j, float k)
{
    // convert to Euler Angles
    float num = -2.0f * i * k + 2.0f * r * j;
    float den = 2.0f * r * r + 2.0f * k * k - 1.0f;
    float roll = (float)atan2((double)num, (double)den);
    
    return roll;
}

void q_to_ypr(float r, float i, float j, float k, float *pYaw, float *pPitch, float *pRoll)
{
    // convert to Euler Angles
    float num = 2.0f * i * j - 2.0f * r * k;
    float den = 2.0f * r * r + 2.0f * j * j - 1.0f;
    *pYaw = (float)atan2((double)num, (double)den);

    float arg = 2.0f * j * k + 2.0f * r * i;
    if (arg > 1.0f) arg = 1.0f;
    if (arg < -1.0f) arg = -1.0f;
    *pPitch = (float)asin((double)arg);

    num = -2.0f * i * k + 2.0f * r * j;
    den = 2.0f * r * r + 2.0f * k * k - 1.0f;
    *pRoll = (float)atan2((double)num, (double)den);
}

