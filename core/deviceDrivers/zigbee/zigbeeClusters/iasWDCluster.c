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
// Created by mkoch201 on 3/25/19.
//

#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/iasWDCluster.h"

#define LOG_TAG                        "iasWDCluster"
#define IASWD_CLUSTER_ID               0x0502
#define IASWD_START_WARNING_COMMAND_ID 0x00

/* Private Data */
typedef struct
{
    ZigbeeCluster cluster;
    const IASWDClusterCallbacks *callbacks;
    const void *callbackContext;
} IASWDCluster;

ZigbeeCluster *iasWDClusterCreate(const IASWDClusterCallbacks *callbacks, void *callbackContext)
{
    IASWDCluster *result = (IASWDCluster *) calloc(1, sizeof(IASWDCluster));

    result->cluster.clusterId = IASWD_CLUSTER_ID;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool iasWDClusterStartWarning(uint64_t eui64,
                              uint8_t endpointId,
                              IASWDWarningMode warningMode,
                              IASWDSirenLevel sirenLevel,
                              bool enableStrobe,
                              uint16_t warningDuration,
                              uint8_t strobeDutyCycle,
                              IASWDStrobeLevel strobeLevel)
{
    uint8_t payload[5];
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, 5, ZIO_WRITE);
    uint8_t warningInfo = 0;
    warningInfo |= (warningMode << 4);
    if (enableStrobe)
    {
        warningInfo |= (1 << 2);
    }
    warningInfo |= sirenLevel;

    zigbeeIOPutUint8(zio, warningInfo);
    zigbeeIOPutUint16(zio, warningDuration);
    zigbeeIOPutUint8(zio, strobeDutyCycle);
    zigbeeIOPutUint8(zio, strobeLevel);


    return (zigbeeSubsystemSendCommand(eui64,
                                       endpointId,
                                       IASWD_CLUSTER_ID,
                                       true,
                                       IASWD_START_WARNING_COMMAND_ID,
                                       payload,
                                       ARRAY_LENGTH(payload)) == 0);
}

#endif
