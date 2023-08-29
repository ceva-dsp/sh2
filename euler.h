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

#ifndef EULER_H
#define EULER_H

// Extract yaw value from quaternion.
float q_to_yaw(float r, float i, float j, float k);

// Extract pitch value from quaternion.
float q_to_pitch(float r, float i, float j, float k);

// Extract roll value from quaternion.    
float q_to_roll(float r, float i, float j, float k);

// Get Yaw, Pitch and Roll from quaternion
void q_to_ypr(float r, float i, float j, float k,
              float *pRoll, float *pPitch, float *pYaw);

#endif
