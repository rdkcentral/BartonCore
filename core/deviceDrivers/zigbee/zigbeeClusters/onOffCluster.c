//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by tlea on 2/13/19.
//

#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <stdio.h>
#include <commonDeviceDefs.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/onOffCluster.h"

#define LOG_TAG "onOffCluster"

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
    addBoolConfigurationMetadata(deviceConfigurationContext->configurationMetadata, ON_OFF_CLUSTER_DISABLE_BIND_KEY,
            bind);
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
    onOffConfigs[0].maxInterval = 1620; //every 27 minutes at least.  we need this for comm fail, but only 1 attr.
    onOffConfigs[0].reportableChange = 1;

    if (zigbeeSubsystemAttributesSetReporting(eui64,
                                              endpointId,
                                              ON_OFF_CLUSTER_ID,
                                              onOffConfigs,
                                              numConfigs) != 0)
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

    //If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
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
            onOffCluster->callbacks->onOffStateChanged(report->eui64,
                                                       report->sourceEndpoint,
                                                       report->reportData[3] != 0,
                                                       onOffCluster->callbackContext);
        }
    }

    return true;
}

#endif //BARTON_CONFIG_ZIGBEE
