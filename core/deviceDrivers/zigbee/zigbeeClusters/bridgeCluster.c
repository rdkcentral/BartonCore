//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 3/18/19.
//

#include <icLog/logging.h>
#include <memory.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/bridgeCluster.h"

#define LOG_TAG                         "bridgeCluster"

// Alarm codes
#define BRIDGE_TAMPER_ALARM_CODE        0x00

#define BRIDGE_REFRESH                  0x00
#define BRIDGE_RESET                    0x01
#define BRIDGE_START_CONFIGURATION      0x02
#define BRIDGE_STOP_CONFIGURATION       0x03

#define BRIDGE_REFRESH_REQUESTED        0x00
#define BRIDGE_REFRESH_COMPLETED        0x01

#define BRIDGE_ALARM_MASK_ATTRIBUTE_ID  0x00
#define BRIDGE_ALARM_STATE_ATTRIBUTE_ID 0x0001

#define BRIDGE_TAMPER_ALARM             0x01

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);
static bool
handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry);
static bool handleAlarmCleared(ZigbeeCluster *ctx,
                               uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *alarmTableEntry);

typedef struct
{
    ZigbeeCluster cluster;

    const BridgeClusterCallbacks *callbacks;
    const void *callbackContext;
} BridgeCluster;

ZigbeeCluster *bridgeClusterCreate(const BridgeClusterCallbacks *callbacks, const void *callbackContext)
{
    BridgeCluster *result = (BridgeCluster *) calloc(1, sizeof(BridgeCluster));

    result->cluster.clusterId = BRIDGE_CLUSTER_ID;

    result->cluster.configureCluster = configureCluster;
    result->cluster.handleClusterCommand = handleClusterCommand;
    result->cluster.handleAlarm = handleAlarm;
    result->cluster.handleAlarmCleared = handleAlarmCleared;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool bridgeClusterRefresh(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(
               eui64, endpointId, BRIDGE_CLUSTER_ID, true, BRIDGE_REFRESH, ICONTROL_MFG_ID, NULL, 0) == 0;
}

bool bridgeClusterStartConfiguration(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(
               eui64, endpointId, BRIDGE_CLUSTER_ID, true, BRIDGE_START_CONFIGURATION, ICONTROL_MFG_ID, NULL, 0) == 0;
}

bool bridgeClusterStopConfiguration(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(
               eui64, endpointId, BRIDGE_CLUSTER_ID, true, BRIDGE_STOP_CONFIGURATION, ICONTROL_MFG_ID, NULL, 0) == 0;
}

bool bridgeClusterReset(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(
               eui64, endpointId, BRIDGE_CLUSTER_ID, true, BRIDGE_RESET, ICONTROL_MFG_ID, NULL, 0) == 0;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, BRIDGE_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind bridge cluster", __FUNCTION__);
        return false;
    }

    // Set up to get a tamper alarm
    if (zigbeeSubsystemWriteNumberMfgSpecific(configContext->eui64,
                                              configContext->endpointId,
                                              BRIDGE_CLUSTER_ID,
                                              ICONTROL_MFG_ID,
                                              true,
                                              BRIDGE_ALARM_MASK_ATTRIBUTE_ID,
                                              ZCL_BITMAP8_ATTRIBUTE_TYPE,
                                              BRIDGE_TAMPER_ALARM,
                                              1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set battery alarm mask", __FUNCTION__);
        return false;
    }

    return true;
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    BridgeCluster *bridgeCluster = (BridgeCluster *) ctx;

    switch (command->commandId)
    {
        case BRIDGE_REFRESH_REQUESTED:
        {
            if (bridgeCluster->callbacks->refreshRequested != NULL)
            {

                bridgeCluster->callbacks->refreshRequested(
                    command->eui64, command->sourceEndpoint, bridgeCluster->callbackContext);
            }
            result = true;
            break;
        }

        case BRIDGE_REFRESH_COMPLETED:
        {
            if (bridgeCluster->callbacks->refreshCompleted != NULL)
            {

                bridgeCluster->callbacks->refreshCompleted(
                    command->eui64, command->sourceEndpoint, bridgeCluster->callbackContext);
            }
            result = true;
            break;
        }

        default:
            icLogError(LOG_TAG, "%s: unexpected command id 0x%02x", __FUNCTION__, command->commandId);
            break;
    }

    return result;
}

static bool
handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    BridgeCluster *cluster = (BridgeCluster *) ctx;

    switch (alarmTableEntry->alarmCode)
    {
        case BRIDGE_TAMPER_ALARM_CODE:
            icLogWarn(LOG_TAG, "%s: Tamper alarm", __FUNCTION__);
            if (cluster->callbacks->bridgeTamperStatusChanged != NULL)
            {
                cluster->callbacks->bridgeTamperStatusChanged(eui64, endpointId, cluster->callbackContext, true);
            }
            break;

        default:
            icLogWarn(
                LOG_TAG, "%s: Unsupported bridge cluster alarm code 0x%02x", __FUNCTION__, alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

static bool
handleAlarmCleared(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    BridgeCluster *cluster = (BridgeCluster *) ctx;

    switch (alarmTableEntry->alarmCode)
    {
        case BRIDGE_TAMPER_ALARM_CODE:
            icLogWarn(LOG_TAG, "%s: Tamper alarm cleared", __FUNCTION__);
            if (cluster->callbacks->bridgeTamperStatusChanged != NULL)
            {
                cluster->callbacks->bridgeTamperStatusChanged(eui64, endpointId, cluster->callbackContext, false);
            }
            break;

        default:
            icLogWarn(
                LOG_TAG, "%s: Unsupported bridge cluster alarm code 0x%02x", __FUNCTION__, alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

bool bridgeClusterGetTamperStatus(const ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, bool *result)
{
    icLogDebug(LOG_TAG, "%s: %" PRIx64 " endpoint %d", __FUNCTION__, eui64, endpointId);

    if (result == NULL)
    {
        icLogWarn(LOG_TAG, "invalid parameter: result");
        return false;
    }

    uint64_t value = 0;
    if (zigbeeSubsystemReadNumberMfgSpecific(
            eui64, endpointId, BRIDGE_CLUSTER_ID, ICONTROL_MFG_ID, true, BRIDGE_ALARM_STATE_ATTRIBUTE_ID, &value) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read tamper status attribute", __FUNCTION__);
        return false;
    }

    *result = (value > 0);
    return true;
}

#endif // BARTON_CONFIG_ZIGBEE