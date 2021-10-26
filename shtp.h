/*
 * Copyright 2015-21 CEVA, Inc.
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

/*
 * Sensor Hub Transport Protocol (SHTP) API
 */

#ifndef SHTP_H
#define SHTP_H

#include <stdint.h>
#include <stdbool.h>

#include "sh2_hal.h"

typedef enum shtp_Event_e {
    SHTP_SHORT_FRAGMENT = 1,
    SHTP_TOO_LARGE_PAYLOADS = 2,
    SHTP_BAD_RX_CHAN = 3,
    SHTP_BAD_TX_CHAN = 4,
    SHTP_BAD_FRAGMENT = 5,
    SHTP_BAD_SN = 6,
    SHTP_INTERRUPTED_PAYLOAD = 7,
} shtp_Event_t;

typedef void shtp_Callback_t(void * cookie, uint8_t *payload, uint16_t len, uint32_t timestamp);
typedef void shtp_EventCallback_t(void *cookie, shtp_Event_t shtpEvent);

// Open the SHTP communications session.
// Takes a pointer to a HAL, which will be opened by this function.
// Returns a pointer referencing the open SHTP session.  (Pass this as pInstance to later calls.)
void * shtp_open(sh2_Hal_t *pHal);

// Closes and SHTP session.
// The associated HAL will be closed.
void shtp_close(void *pShtp);

// Set the callback function for reporting SHTP asynchronous events
void shtp_setEventCallback(void *pInstance,
                           shtp_EventCallback_t * eventCallback, 
                           void *eventCookie);

// Register a listener for an SHTP channel
int shtp_listenChan(void *pShtp,
                    uint8_t channel,
                    shtp_Callback_t *callback, void * cookie);

// Send an SHTP payload on a particular channel
int shtp_send(void *pShtp,
              uint8_t channel, const uint8_t *payload, uint16_t len);

// Check for received data and process it.
void shtp_service(void *pShtp);

// #ifdef SHTP_H
#endif
