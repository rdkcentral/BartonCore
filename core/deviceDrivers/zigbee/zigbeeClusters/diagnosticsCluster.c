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