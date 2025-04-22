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
// Created by Thomas Lea on 4/17/25.
//

#define LOG_TAG "occupancySensingCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

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

#include "zigbeeClusters/occupancySensingCluster.h"


typedef struct
{
    ZigbeeCluster cluster;

    const OccupancySensingClusterCallbacks *callbacks;
    void *callbackContext;
} OccupancySensingCluster;

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);
static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

ZigbeeCluster *occupancySensingClusterCreate(const OccupancySensingClusterCallbacks *callbacks, void *callbackContext)
{
    OccupancySensingCluster *result =
        (OccupancySensingCluster *) calloc(1, sizeof(OccupancySensingCluster));

    result->cluster.clusterId = OCCUPANCY_SENSING_CLUSTER_ID;

    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool occupancySensingClusterGetIsOccupied(uint64_t eui64, uint8_t endpointId, bool *isOccupied)
{
    bool result = false;

    if (isOccupied == NULL)
    {
        icError("invalid arguments");
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, OCCUPANCY_SENSING_CLUSTER_ID, true, OCCUPANCY_SENSING_OCCUPANCY_ATTRIBUTE_ID, &val) == 0)
    {
        *isOccupied = val != 0;
        result = true;
    }
    else
    {
        icError("failed to read occupancy");
    }

    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    // configure attribute reporting on measured value
    zhalAttributeReportingConfig valueConfigs[1];
    uint8_t numConfigs = 1;
    memset(&valueConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
    valueConfigs[0].attributeInfo.id = OCCUPANCY_SENSING_OCCUPANCY_ATTRIBUTE_ID;
    valueConfigs[0].attributeInfo.type = ZCL_BITMAP8_ATTRIBUTE_TYPE;
    valueConfigs[0].minInterval = 1;
    valueConfigs[0].maxInterval = TWENTY_SEVEN_MINUTES_IN_SECS;
    valueConfigs[0].reportableChange = 1;

    if (zigbeeSubsystemBindingSet(
            configContext->eui64, configContext->endpointId, OCCUPANCY_SENSING_CLUSTER_ID) != 0)
    {
        icError("failed to bind occupancy sensing");
        result = false;
    }
    else if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                   configContext->endpointId,
                                                   OCCUPANCY_SENSING_CLUSTER_ID,
                                                   valueConfigs,
                                                   numConfigs) != 0)
    {
        icError("failed to set reporting for occupancy attribute");
        result = false;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icDebug();

    OccupancySensingCluster *cluster = (OccupancySensingCluster *) ctx;

    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);
    uint16_t attributeId = zigbeeIOGetUint16(zio);
    // Pull out the type and throw it away
    zigbeeIOGetUint8(zio);

    if (attributeId == OCCUPANCY_SENSING_OCCUPANCY_ATTRIBUTE_ID)
    {
        if (cluster->callbacks->occupancyUpdated != NULL)
        {
            uint8_t occupancy = zigbeeIOGetUint8(zio);
            icDebug("occupancy=%" PRIx8, occupancy);
            cluster->callbacks->occupancyUpdated(cluster->callbackContext, report->eui64, report->sourceEndpoint, occupancy != 0);
        }
    }

    return true;
}

#endif // BARTON_CONFIG_ZIGBEE
