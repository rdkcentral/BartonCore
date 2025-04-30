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
// Created by tlea on 2/13/19.
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

#include "zigbeeClusters/onOffCluster.h"

#define LOG_TAG                         "onOffCluster"

#define ON_OFF_CLUSTER_DISABLE_BIND_KEY "onOffClusterDisableBind"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

typedef struct
{
    ZigbeeCluster cluster;

    const OnOffClusterCallbacks *callbacks;
    const void *callbackContext;
} OnOffCluster;

ZigbeeCluster *onOffClusterCreate(const OnOffClusterCallbacks *callbacks, void *callbackContext)
{
    OnOffCluster *result = (OnOffCluster *) calloc(1, sizeof(OnOffCluster));

    result->cluster.clusterId = ON_OFF_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

void onOffClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, ON_OFF_CLUSTER_DISABLE_BIND_KEY, bind);
}

bool onOffClusterIsOn(uint64_t eui64, uint8_t endpointId, bool *isOn)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, ON_OFF_CLUSTER_ID, true, ON_OFF_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read on off attribute value", __FUNCTION__);
    }
    else
    {
        *isOn = val != 0;
        result = true;
    }

    return result;
}

bool onOffClusterSetOn(uint64_t eui64, uint8_t endpointId, bool isOn)
{
    return (zigbeeSubsystemSendCommand(eui64,
                                       endpointId,
                                       ON_OFF_CLUSTER_ID,
                                       true,
                                       (uint8_t) (isOn ? ON_OFF_TURN_ON_COMMAND_ID : ON_OFF_TURN_OFF_COMMAND_ID),
                                       NULL,
                                       0) == 0);
}

bool onOffClusterSetAttributeReporting(uint64_t eui64, uint8_t endpointId)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalAttributeReportingConfig onOffConfigs[1];
    uint8_t numConfigs = 1;
    memset(&onOffConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
    onOffConfigs[0].attributeInfo.id = ON_OFF_ATTRIBUTE_ID;
    onOffConfigs[0].attributeInfo.type = ZCL_BOOLEAN_ATTRIBUTE_TYPE;
    onOffConfigs[0].minInterval = 1;
    onOffConfigs[0].maxInterval = 1620; // every 27 minutes at least.  we need this for comm fail, but only 1 attr.
    onOffConfigs[0].reportableChange = 1;

    if (zigbeeSubsystemAttributesSetReporting(eui64, endpointId, ON_OFF_CLUSTER_ID, onOffConfigs, numConfigs) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting for on off", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, ON_OFF_CLUSTER_DISABLE_BIND_KEY, true))
    {
        if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, ON_OFF_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind on off", __FUNCTION__);
            result = false;
        }
    }

    if (result && onOffClusterSetAttributeReporting(configContext->eui64, configContext->endpointId) == false)
    {
        result = false;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    OnOffCluster *onOffCluster = (OnOffCluster *) ctx;

    if (onOffCluster->callbacks->onOffStateChanged != NULL)
    {
        if (report->reportDataLen == 4)
        {
            onOffCluster->callbacks->onOffStateChanged(
                report->eui64, report->sourceEndpoint, report->reportData[3] != 0, onOffCluster->callbackContext);
        }
    }

    return true;
}

#endif // BARTON_CONFIG_ZIGBEE
