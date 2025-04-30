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
// Created by tlea on 3/1/19
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

#include "zigbeeClusters/fanControlCluster.h"

#define LOG_TAG                              "fanControlCluster"

#define FAN_CONTROL_CLUSTER_DISABLE_BIND_KEY "fanConClusterDisableBind"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

typedef struct
{
    ZigbeeCluster cluster;

    const FanControlClusterCallbacks *callbacks;
    const void *callbackContext;
} FanControlCluster;

ZigbeeCluster *fanControlClusterCreate(const FanControlClusterCallbacks *callbacks, void *callbackContext)
{
    FanControlCluster *result = (FanControlCluster *) calloc(1, sizeof(FanControlCluster));

    result->cluster.clusterId = FAN_CONTROL_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

void fanControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, FAN_CONTROL_CLUSTER_DISABLE_BIND_KEY, bind);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalAttributeReportingConfig *fanReportingConfigs =
        (zhalAttributeReportingConfig *) calloc(1, sizeof(zhalAttributeReportingConfig));
    fanReportingConfigs[0].attributeInfo.id = FAN_CONTROL_FAN_MODE_ATTRIBUTE_ID;
    fanReportingConfigs[0].attributeInfo.type = ZCL_ENUM8_ATTRIBUTE_TYPE;
    fanReportingConfigs[0].minInterval = 1;
    fanReportingConfigs[0].maxInterval = 1620; // 27 minutes
    fanReportingConfigs[0].reportableChange = 1;

    // If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, FAN_CONTROL_CLUSTER_DISABLE_BIND_KEY, true))
    {
        if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, FAN_CONTROL_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind fan cluster", __FUNCTION__);
            goto exit;
        }
    }

    if (zigbeeSubsystemAttributesSetReporting(
            configContext->eui64, configContext->endpointId, FAN_CONTROL_CLUSTER_ID, fanReportingConfigs, 1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting on fan cluster", __FUNCTION__);
        goto exit;
    }

    result = true;

exit:
    free(fanReportingConfigs);

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    FanControlCluster *fanControlCluster = (FanControlCluster *) ctx;

    if (fanControlCluster->callbacks->fanModeChanged != NULL)
    {
        if (report->reportDataLen == 4)
        {
            fanControlCluster->callbacks->fanModeChanged(
                report->eui64, report->sourceEndpoint, report->reportData[3], fanControlCluster->callbackContext);
        }
    }

    return true;
}

bool fanControlClusterGetFanMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t *mode)
{
    if (mode == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, FAN_CONTROL_CLUSTER_ID, true, FAN_CONTROL_FAN_MODE_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read fan mode attribute", __FUNCTION__);
        return false;
    }

    *mode = (uint8_t) tmp;
    return true;
}

bool fanControlClusterSetFanMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t mode)
{
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   FAN_CONTROL_CLUSTER_ID,
                                   true,
                                   FAN_CONTROL_FAN_MODE_ATTRIBUTE_ID,
                                   ZCL_ENUM8_ATTRIBUTE_TYPE,
                                   mode,
                                   1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write fan mode attribute", __FUNCTION__);
        return false;
    }

    return true;
}

// return a constant string representing the provided fan mode value
const char *fanControlClusterGetFanModeString(uint8_t fanMode)
{
    // this only returns the values we support setting, not the entire enum
    // 0 = off
    // 1 = low
    // 2 = medium
    // 3 = high
    // 4 = on
    // 5 = auto
    // 6 = smart

    switch (fanMode)
    {
        case 0x00:
            return THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_OFF;

        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
            return THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_ON;

        case 0x05:
            return THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_AUTO;

        default:
            return THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_UNKNOWN;
    }
}

#endif // BARTON_CONFIG_ZIGBEE
