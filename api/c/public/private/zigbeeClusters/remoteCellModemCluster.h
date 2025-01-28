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

#pragma once

#include "zigbeeCluster.h"

#ifdef BARTON_CONFIG_ZIGBEE

typedef struct
{
    void (*onOffStateChanged)(uint64_t eui64, uint8_t endpointId, bool isOn);
} RemoteCellModemClusterCallbacks;

ZigbeeCluster *remoteCellModemClusterCreate(const RemoteCellModemClusterCallbacks *callbacks,
                                            void *callbackContext,
                                            uint16_t manufacturerId);

/**
 * Determines whether or not the remote cell modem is powered on or not.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @param result
 * @return
 */
bool remoteCellModemClusterIsPoweredOn(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, bool *result);

/**
 * Powers the remote cell modem on.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @return
 */
bool remoteCellModemClusterPowerOn(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

/**
 * Powers the remote cell modem off.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @return
 */
bool remoteCellModemClusterPowerOff(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

/**
 * Forces the remote cell modem to perform an emergency reset.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @return
 */
bool remoteCellModemClusterEmergencyReset(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

#endif // BARTON_CONFIG_ZIGBEE
