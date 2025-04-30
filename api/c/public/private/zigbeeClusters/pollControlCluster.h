//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by tlea on 2/15/19.
//

#pragma once


#ifdef BARTON_CONFIG_ZIGBEE

#include "comcastBatterySaving.h"
#include "zigbeeCluster.h"

#define SHORT_POLL_INTERVAL_QS_METADATA "pollControl.shortPollIntervalQS"
#define LONG_POLL_INTERVAL_QS_METADATA  "pollControl.longPollIntervalQS"
#define FAST_POLL_TIMEOUT_QS_METADATA   "pollControl.fastPollTimeoutQS"
#define CHECK_IN_INTERVAL_QS_METADATA   "pollControl.checkInIntervalQS"

#define POLL_CONTROL_CHECKIN_COMMAND_ID 0x00

typedef struct
{
    void (*checkin)(uint64_t eui64,
                    uint8_t endpointId,
                    const ComcastBatterySavingData *batterySavingData,
                    const void *ctx);
} PollControlClusterCallbacks;


ZigbeeCluster *pollControlClusterCreate(const PollControlClusterCallbacks *callbacks, const void *callbackContext);

/**
 * Add callback handler (PollControlClusterCallbacks) for passed ZigbeeCluster reference
 *
 * @param baseCluster
 * @param callbacks
 * @param callbackContext
 * @return True on success
 */
bool pollControlClusterAddCallback(ZigbeeCluster *baseCluster,
                                   const PollControlClusterCallbacks *callback,
                                   const void *callbackContext);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void pollControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

bool pollControlClusterSendCustomCheckInResponse(uint64_t eui64, uint8_t endpointId);

bool pollControlClusterSendCheckInResponse(uint64_t eui64, uint8_t endpointId, bool startFastPoll);

bool pollControlClusterStopFastPoll(uint64_t eui64, uint8_t endpointId);

/**
 * Set the long poll interval (in quarter seconds).  This can be used to speed up data requests on sleepy devices
 * during pairing or reconfiguration when we dont receive a checkin command (where the mechanism to speed up is
 * different).
 *
 * @param eui64
 * @param endpointId
 * @param newIntervalQs the new long poll interval in quarter seconds
 * @return true on success
 */
bool pollControlClusterSetLongPollInterval(uint64_t eui64, uint8_t endpointId, uint32_t newIntervalQs);

#endif // BARTON_CONFIG_ZIGBEE
