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