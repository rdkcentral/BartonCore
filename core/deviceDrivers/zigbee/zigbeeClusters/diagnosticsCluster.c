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
// Created by tlea on 2/18/19.
//

#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/diagnosticsCluster.h"

#define LOG_TAG "diagnosticsCluster"

typedef struct
{
    ZigbeeCluster cluster;

    const DiagnosticsClusterCallbacks *callbacks;
    void *callbackContext;
} DiagnosticsCluster;

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

ZigbeeCluster *diagnosticsClusterCreate(const DiagnosticsClusterCallbacks *callbacks, void *callbackContext)
{
    DiagnosticsCluster *result = (DiagnosticsCluster *) calloc(1, sizeof(DiagnosticsCluster));

    result->cluster.clusterId = DIAGNOSTICS_CLUSTER_ID;

    result->cluster.handlePollControlCheckin = handlePollControlCheckin;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool diagnosticsClusterGetLastMessageLqi(uint64_t eui64, uint8_t endpointId, uint8_t *lqi)
{
    bool result = false;

    if (lqi == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, DIAGNOSTICS_CLUSTER_ID, true, DIAGNOSTICS_LAST_MESSAGE_LQI_ATTRIBUTE_ID, &val) == 0)
    {
        *lqi = (uint8_t) (val & 0xFF);
        result = true;
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to read LQI", __FUNCTION__);
    }

    return result;
}

bool diagnosticsClusterGetLastMessageRssi(uint64_t eui64, uint8_t endpointId, int8_t *rssi)
{
    bool result = false;

    if (rssi == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, DIAGNOSTICS_CLUSTER_ID, true, DIAGNOSTICS_LAST_MESSAGE_RSSI_ATTRIBUTE_ID, &val) == 0)
    {
        *rssi = (int8_t) (val & 0xFF);
        result = true;
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to read RSSI", __FUNCTION__);
    }

    return result;
}

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    // read RSSI and LQI then invoke the callbacks
    // TODO enhance with a multi-attribute read once zigbee subsystem supports that

    DiagnosticsCluster *diagnosticsCluster = (DiagnosticsCluster *) ctx;

    if (diagnosticsCluster->callbacks->lastMessageRssiLqiUpdated != NULL)
    {
        int8_t rssi;
        uint8_t lqi;

        if (diagnosticsClusterGetLastMessageRssi(eui64, endpointId, &rssi) == false)
        {
            return;
        }

        if (diagnosticsClusterGetLastMessageLqi(eui64, endpointId, &lqi) == false)
        {
            return;
        }

        diagnosticsCluster->callbacks->lastMessageRssiLqiUpdated(
            diagnosticsCluster->callbackContext, eui64, endpointId, rssi, lqi);
    }
}

#endif // BARTON_CONFIG_ZIGBEE
