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
// Created by tlea on 2/19/19.
//

#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/levelControlCluster.h"

#define LOG_TAG                                "levelControlCluster"

#define LEVEL_CONTROL_CLUSTER_DISABLE_BIND_KEY "levelControlClusterDisableBind"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

typedef struct
{
    ZigbeeCluster cluster;

    const LevelControlClusterCallbacks *callbacks;
    const void *callbackContext;
} LevelControlCluster;

ZigbeeCluster *levelControlClusterCreate(const LevelControlClusterCallbacks *callbacks, void *callbackContext)
{
    LevelControlCluster *result = (LevelControlCluster *) calloc(1, sizeof(LevelControlCluster));

    result->cluster.clusterId = LEVEL_CONTROL_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool levelControlClusterGetLevel(uint64_t eui64, uint8_t endpointId, uint8_t *level)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, LEVEL_CONTROL_CLUSTER_ID, true, LEVEL_CONTROL_CURRENT_LEVEL_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read level attribute value", __FUNCTION__);
    }
    else
    {
        *level = (uint8_t) (val & 0xff);
        result = true;
    }

    return result;
}

bool levelControlClusterSetLevel(uint64_t eui64, uint8_t endpointId, uint8_t level)
{
    if (level == 0xff)
    {
        icLogError(LOG_TAG, "%s: invalid level 0x%" PRIx8, __FUNCTION__, level);
        return false;
    }

    uint8_t msg[3];
    msg[0] = level;
    msg[1] = 0; // transition time byte 1
    msg[2] = 0; // transition time byte 2

    if (zigbeeSubsystemSendCommand(eui64,
                                   endpointId,
                                   LEVEL_CONTROL_CLUSTER_ID,
                                   true,
                                   LEVEL_CONTROL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID,
                                   msg,
                                   3) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send move to level with on off command", __FUNCTION__);
        return false;
    }

    // Set onLevel after moveToLevelWithOnOff
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   LEVEL_CONTROL_CLUSTER_ID,
                                   true,
                                   LEVEL_CONTROL_ON_LEVEL_ATTRIBUTE_ID,
                                   ZCL_INT8U_ATTRIBUTE_TYPE,
                                   level,
                                   1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set on level", __FUNCTION__);
        return false;
    }

    return true;
}

void levelControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, LEVEL_CONTROL_CLUSTER_DISABLE_BIND_KEY, bind);
}

// caller frees
char *levelControlClusterGetLevelString(uint8_t level)
{
    if (level == 254 || level == 255)
    {
        return strdup("100");
    }
    else
    {
        char *result = (char *) malloc(3); // 2 digits \0

        // Solve for an integer math problem where we aren't accounting for decimals and therefore end up a percentage
        // point off when converting the attribute value to a percentage.
        //
        int convertedLevel = level * 100;
        if (convertedLevel % 255 >= 127)
        {
            convertedLevel = (convertedLevel / 255) + 1;
        }
        else
        {
            convertedLevel = convertedLevel / 255;
        }

        snprintf(result, 3, "%d", convertedLevel);

        return result;
    }
}

uint8_t levelControlClusterGetLevelFromString(const char *level)
{
    uint8_t result = 0xff; // invalid

    if (stringToUint8(level, &result) == true && result <= 100)
    {
        result = (uint8_t) ((result * 255) / 100);
        if (result == 255) // 255 is invalid, 254 is max
        {
            result = 254;
        }
    }

    return result;
}

bool levelControlClusterSetAttributeReporting(uint64_t eui64, uint8_t endpointId)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalAttributeReportingConfig levelConfigs[1];
    uint8_t numConfigs = 1;
    memset(&levelConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
    levelConfigs[0].attributeInfo.id = LEVEL_CONTROL_CURRENT_LEVEL_ATTRIBUTE_ID;
    levelConfigs[0].attributeInfo.type = ZCL_INT8U_ATTRIBUTE_TYPE;
    levelConfigs[0].minInterval = 1;
    levelConfigs[0].maxInterval = 1620; // every 27 minutes at least.  we need this for comm fail, but only 1 attr.
    levelConfigs[0].reportableChange = 1;

    if (zigbeeSubsystemAttributesSetReporting(eui64, endpointId, LEVEL_CONTROL_CLUSTER_ID, levelConfigs, numConfigs) !=
        0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting for level control", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
    if (getBoolConfigurationMetadata(
            configContext->configurationMetadata, LEVEL_CONTROL_CLUSTER_DISABLE_BIND_KEY, true))
    {
        if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, LEVEL_CONTROL_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind level control", __FUNCTION__);
            result = false;
        }
    }

    if (result && levelControlClusterSetAttributeReporting(configContext->eui64, configContext->endpointId) == false)
    {
        result = false;
    }

    if (icDiscoveredDeviceDetailsClusterHasAttribute(configContext->discoveredDeviceDetails,
                                                     configContext->endpointId,
                                                     LEVEL_CONTROL_CLUSTER_ID,
                                                     true,
                                                     LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID) == true)
    {
        if (zigbeeSubsystemWriteNumber(configContext->eui64,
                                       configContext->endpointId,
                                       LEVEL_CONTROL_CLUSTER_ID,
                                       true,
                                       LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID,
                                       ZCL_INT16U_ATTRIBUTE_TYPE,
                                       0, // instant
                                       2) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to set on off transition time", __FUNCTION__);
            return false;
        }
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    LevelControlCluster *levelControlCluster = (LevelControlCluster *) ctx;

    if (levelControlCluster->callbacks->levelChanged != NULL)
    {
        char *uuid = zigbeeSubsystemEui64ToId(report->eui64);
        char epName[4]; // max uint8_t + \0
        sprintf(epName, "%" PRIu8, report->sourceEndpoint);

        if (report->reportDataLen == 4)
        {
            levelControlCluster->callbacks->levelChanged(
                report->eui64, report->sourceEndpoint, report->reportData[3], levelControlCluster->callbackContext);
        }

        free(uuid);
    }

    return true;
}

#endif // BARTON_CONFIG_ZIGBEE
