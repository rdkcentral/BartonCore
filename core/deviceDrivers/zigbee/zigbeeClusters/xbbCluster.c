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

/*
 * Created by Thomas Lea on 4/2/21.
 */

#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <memory.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/xbbCluster.h"

#define LOG_TAG "xbbCluster"

typedef struct
{
    ZigbeeCluster cluster;

    const XbbClusterCallbacks *callbacks;
    XbbBatteryType batteryType;
    void *callbackContext;
} XbbCluster;

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);

ZigbeeCluster *xbbClusterCreate(const XbbClusterCallbacks *callbacks, void *callbackContext)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);
    XbbCluster *result = (XbbCluster *) calloc(1, sizeof(XbbCluster));

    result->cluster.clusterId = XBB_CLUSTER_ID;
    result->callbacks = callbacks;
    result->callbackContext = callbackContext;
    result->cluster.handleClusterCommand = handleClusterCommand;

    return (ZigbeeCluster *) result;
}

void xbbClusterSetBatteryType(const ZigbeeCluster *cluster, XbbBatteryType batteryType)
{
    icLogDebug(LOG_TAG, "%s: setting type to %d", __func__, batteryType);

    XbbCluster *xbbCluster = (XbbCluster *) cluster;
    if (xbbCluster != NULL)
    {
        xbbCluster->batteryType = batteryType;
    }
}

bool xbbClusterGetStatus(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint16_t *status)
{
    bool result = false;

    if (status == NULL)
    {
        return false;
    }

    uint64_t value = 0;
    if (zigbeeSubsystemReadNumberMfgSpecific(
            eui64, 1, XBB_CLUSTER_ID, COMCAST_MFG_ID, true, XBB_CLUSTER_STATUS_ATTRIBUTE_ID, &value) == 0 &&
        value <= UINT16_MAX)
    {
        *status = (uint16_t) value;
        result = true;
    }

    return result;
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    bool result = true;

    icLogDebug(LOG_TAG, __func__);

    XbbCluster *xbbCluster = (XbbCluster *) ctx;

    if (command->mfgSpecific && command->mfgCode == COMCAST_MFG_ID && command->clusterId == XBB_CLUSTER_ID &&
        command->commandId == XBB_CLUSTER_STATUS_COMMAND_ID)
    {
        sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);
        zigbeeIOGetUint16(zio); // unused cluster revision
        uint16_t status = zigbeeIOGetUint16(zio);

        if (xbbCluster->callbacks->xbbStatusChanged != NULL)
        {
            xbbCluster->callbacks->xbbStatusChanged(
                xbbCluster->callbackContext, command->eui64, command->sourceEndpoint, status);
        }
    }
    else
    {
        icLogDebug(LOG_TAG, "%s: ignoring unexpected command for cluster 0x%04x", __func__, command->clusterId);
        result = false;
    }

    return result;
}

#endif // BARTON_CONFIG_ZIGBEE
