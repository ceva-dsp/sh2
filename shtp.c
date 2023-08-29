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

#include "shtp.h"
#include "sh2_err.h"

#include <string.h>

// ------------------------------------------------------------------------
// Private types

#define SHTP_INSTANCES (1)  // Number of SHTP devices supported
#define SHTP_MAX_CHANS (8)  // Max channels per SHTP device
#define SHTP_HDR_LEN (4)

typedef struct shtp_Channel_s {
    uint8_t nextOutSeq;
    uint8_t nextInSeq;
    shtp_Callback_t *callback;
    void *cookie;
} shtp_Channel_t;

// Per-instance data for SHTP
typedef struct shtp_s {
    // Associated SHTP HAL
    // If 0, this indicates the SHTP instance is available for new opens
    sh2_Hal_t *pHal;

    // Asynchronous Event callback and it's cookie
    shtp_EventCallback_t *eventCallback;
    void * eventCookie;

    // Transmit support
    uint8_t outTransfer[SH2_HAL_MAX_TRANSFER_OUT];

    // Receive support
    uint16_t inRemaining;
    uint8_t  inChan;
    uint8_t  inPayload[SH2_HAL_MAX_PAYLOAD_IN];
    uint16_t inCursor;
    uint32_t inTimestamp;
    uint8_t inTransfer[SH2_HAL_MAX_TRANSFER_IN];

    // SHTP Channels
    shtp_Channel_t      chan[SHTP_MAX_CHANS];

    // Stats
    uint32_t rxBadChan;
    uint32_t rxShortFragments;
    uint32_t rxTooLargePayloads;
    uint32_t rxInterruptedPayloads;
    
    uint32_t badTxChan;
    uint32_t txDiscards;
    uint32_t txTooLargePayloads;

} shtp_t;


// ------------------------------------------------------------------------
// Private data

static shtp_t instances[SHTP_INSTANCES];

static bool shtp_initialized = false;

// ------------------------------------------------------------------------
// Private functions

static void shtp_init(void)
{
    // Clear pHal pointer in every instance.  This marks them as unallocated.
    for (int n = 0; n < SHTP_INSTANCES; n++) {
        instances[n].pHal = 0;
    }

    // Set the initialized flag so this doesn't happen again.
    shtp_initialized = true;
}

static shtp_t *getInstance(void)
{
    for (int n = 0; n < SHTP_INSTANCES; n++) {
        if (instances[n].pHal == 0) {
            // This instance is free
            return &instances[n];
        }
    }

    // Can't give an instance, none are free
    return 0;
}


static inline uint16_t min_u16(uint16_t a, uint16_t b)
{
    if (a < b) {
        return a;
    }
    else {
        return b;
    }
}

// Send a cargo as a sequence of transports
static int txProcess(shtp_t *pShtp, uint8_t chan, const uint8_t* pData, uint32_t len)
{
    int status = SH2_OK;
    
    bool continuation = false;
    uint16_t cursor = 0;
    uint16_t remaining;
    uint16_t transferLen;  // length of transfer, minus the header
    uint16_t lenField;

    cursor = 0;
    remaining = len;
    while (remaining > 0) {
        // How much data (not header) can we send in next transfer
        transferLen = min_u16(remaining, SH2_HAL_MAX_TRANSFER_OUT-SHTP_HDR_LEN);
        
        // Length field will be transferLen + SHTP_HDR_LEN
        lenField = transferLen + SHTP_HDR_LEN;

        // Put the header in the out buffer
        pShtp->outTransfer[0] = lenField & 0xFF;
        pShtp->outTransfer[1] = (lenField >> 8) & 0x7F;
        if (continuation) {
            pShtp->outTransfer[1] |= 0x80;
        }
        pShtp->outTransfer[2] = chan;
        pShtp->outTransfer[3] = pShtp->chan[chan].nextOutSeq++;

        // Stage one tranfer in the out buffer
        memcpy(pShtp->outTransfer+SHTP_HDR_LEN, pData+cursor, transferLen);
        remaining -= transferLen;
        cursor += transferLen;

        // Transmit (try repeatedly while HAL write returns 0)
        status = pShtp->pHal->write(pShtp->pHal, pShtp->outTransfer, lenField);
        while (status == 0)
        {
            shtp_service(pShtp);
            status = pShtp->pHal->write(pShtp->pHal, pShtp->outTransfer, lenField);
        }
        
        if (status < 0)
        {
            // Error, throw away this cargo
            pShtp->txDiscards++;
            return status;
        }

        // For the rest of this transmission, packets are continuations.
        continuation = true;
    }

    return SH2_OK;
}

static void rxAssemble(shtp_t *pShtp, uint8_t *in, uint16_t len, uint32_t t_us)
{
    uint16_t payloadLen;
    bool continuation;
    uint8_t chan = 0;
    uint8_t seq = 0;

    // discard invalid short fragments
    if (len < SHTP_HDR_LEN) {
        pShtp->rxShortFragments++;
        if (pShtp->eventCallback) {
            pShtp->eventCallback(pShtp->eventCookie, SHTP_SHORT_FRAGMENT);
        }
        return;
    }
    
    // Interpret header fields
    payloadLen = (in[0] + (in[1] << 8)) & (~0x8000);
    continuation = ((in[1] & 0x80) != 0);
    chan = in[2];
    seq = in[3];

    if (seq != pShtp->chan[chan].nextInSeq){
        if (pShtp->eventCallback) {
            pShtp->eventCallback(pShtp->eventCookie,
                                 SHTP_BAD_SN);
        }
    }
    
    if (payloadLen < SHTP_HDR_LEN) {
        pShtp->rxShortFragments++;
        if (pShtp->eventCallback) {
            pShtp->eventCallback(pShtp->eventCookie, SHTP_SHORT_FRAGMENT);
        }
        return;
    }
        
    if (chan >= SHTP_MAX_CHANS) {
        // Invalid channel id.
        pShtp->rxBadChan++;

        if (pShtp->eventCallback) {
            pShtp->eventCallback(pShtp->eventCookie, SHTP_BAD_RX_CHAN);
        }
        return;
    }

    // Discard earlier assembly in progress if the received data doesn't match it.
    if (pShtp->inRemaining) {
        // Check this against previously received data.
        if (!continuation ||
            (chan != pShtp->inChan) ||
            (seq != pShtp->chan[chan].nextInSeq) ||
            (payloadLen-SHTP_HDR_LEN != pShtp->inRemaining)) {
            
            if (pShtp->eventCallback) {
                pShtp->eventCallback(pShtp->eventCookie,
                                     SHTP_BAD_FRAGMENT);
            }
            
            // This fragment doesn't fit with previous one, discard earlier data
            pShtp->inRemaining = 0;

            pShtp->rxInterruptedPayloads++;
            if (pShtp->eventCallback) {
                pShtp->eventCallback(pShtp->eventCookie, SHTP_INTERRUPTED_PAYLOAD);
            }
        }
    }
    
    // Remember next sequence number we expect for this channel.
    pShtp->chan[chan].nextInSeq = seq + 1;

    if (pShtp->inRemaining == 0) {
        if (payloadLen > sizeof(pShtp->inPayload)) {
            // Error: This payload won't fit! Discard it.
            pShtp->rxTooLargePayloads++;
            
            if (pShtp->eventCallback) {
                pShtp->eventCallback(pShtp->eventCookie, SHTP_TOO_LARGE_PAYLOADS);
            }

            return;
        }

        // This represents a new payload

        // Store timestamp
        pShtp->inTimestamp = t_us;

        // Start a new assembly.
        pShtp->inCursor = 0;
        pShtp->inChan = chan;
    }

    // Append the new fragment to the payload under construction.
    if (len > payloadLen) {
        // Only use the valid portion of the transfer
        len = payloadLen;
    }
    memcpy(pShtp->inPayload + pShtp->inCursor, in+SHTP_HDR_LEN, len-SHTP_HDR_LEN);
    pShtp->inCursor += len-SHTP_HDR_LEN;
    pShtp->inRemaining = payloadLen - len;

    // If whole payload received, deliver it to channel listener.
    if (pShtp->inRemaining == 0) {

        // Call callback if there is one.
        if (pShtp->chan[chan].callback != 0) {
            pShtp->chan[chan].callback(pShtp->chan[chan].cookie,
                                       pShtp->inPayload, pShtp->inCursor,
                                       pShtp->inTimestamp);
        }
    }
}

// ------------------------------------------------------------------------
// Public functions

// Takes HAL pointer, returns shtp ID for use in future calls.
// HAL will be opened by this call.
void *shtp_open(sh2_Hal_t *pHal)
{
    if (!shtp_initialized) {
        // Perform one-time module initialization
        shtp_init();
    }
    
    // Validate params
    if (pHal == 0) {
        // Error
        return 0;
    }

    // Find an available instance for this open
    shtp_t *pShtp = getInstance();
    if (pShtp == 0) {
        // No instances available, return error
        return 0;
    }

    // Clear the SHTP instance as a shortcut to initializing all fields
    memset(pShtp, 0, sizeof(shtp_t));
    
    // Open HAL
    int status = pHal->open(pHal);
    if (status != SH2_OK) {
        return 0;
    }

    // Store reference to the HAL
    pShtp->pHal = pHal;

    return pShtp;
}

// Releases resources associated with this SHTP instance.
// HAL will not be closed.
void shtp_close(void *pInstance)
{
    shtp_t *pShtp = (shtp_t *)pInstance;

    pShtp->pHal->close(pShtp->pHal);
    
    // Deallocate the SHTP instance.
    // (Setting pHal to 0 marks it as free.)
    pShtp->pHal = 0;
}

// Register the pointer of the callback function for reporting asynchronous events
void shtp_setEventCallback(void *pInstance, 
                           shtp_EventCallback_t * eventCallback, 
                           void *eventCookie) {
    shtp_t *pShtp = (shtp_t *)pInstance;

    pShtp->eventCallback = eventCallback;
    pShtp->eventCookie = eventCookie;
}

// Register a listener for an SHTP channel
int shtp_listenChan(void *pInstance,
                    uint8_t channel,
                    shtp_Callback_t *callback, void * cookie)
{
    shtp_t *pShtp = (shtp_t *)pInstance;
    
    // Balk if channel is invalid
    if ((channel == 0) || (channel >= SHTP_MAX_CHANS)) {
        return SH2_ERR_BAD_PARAM;
    }

    pShtp->chan[channel].callback = callback;
    pShtp->chan[channel].cookie = cookie;

    return SH2_OK;
}

// Send an SHTP payload on a particular channel
int shtp_send(void *pInstance,
              uint8_t channel,
              const uint8_t *payload, uint16_t len)
{
    shtp_t *pShtp = (shtp_t *)pInstance;
    
    if (len > SH2_HAL_MAX_PAYLOAD_OUT) {
        pShtp->txTooLargePayloads++;
        return SH2_ERR_BAD_PARAM;
    }
    if (channel >= SHTP_MAX_CHANS) {
        pShtp->badTxChan++;
        return SH2_ERR_BAD_PARAM;
    }

    return txProcess(pShtp, channel, payload, len);
}

// Check for received data and process it.
void shtp_service(void *pInstance)
{
    shtp_t *pShtp = (shtp_t *)pInstance;
    uint32_t t_us = 0;
    
    int len = pShtp->pHal->read(pShtp->pHal, pShtp->inTransfer, sizeof(pShtp->inTransfer), &t_us);
    if (len > 0) {
        rxAssemble(pShtp, pShtp->inTransfer, len, t_us);
    }
}
