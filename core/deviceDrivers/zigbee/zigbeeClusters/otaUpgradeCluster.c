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
// Created by tlea on 9/23/19.
//


#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <deviceServicePrivate.h>
#include <commonDeviceDefs.h>
#include <icUtil/stringUtils.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/otaUpgradeCluster.h"

#define LOG_TAG "otaUpgradeCluster"

#define PAYLOAD_TYPE_QUERY_JITTER 0
#define JITTER_MAX 100

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

typedef struct
{
    ZigbeeCluster cluster;
} OtaUpgradeCluster;

ZigbeeCluster *otaUpgradeClusterCreate(void)
{
    OtaUpgradeCluster *result = (OtaUpgradeCluster *) calloc(1, sizeof(OtaUpgradeCluster));

    result->cluster.clusterId = OTA_UPGRADE_CLUSTER_ID;
    result->cluster.handlePollControlCheckin = handlePollControlCheckin;

    return (ZigbeeCluster *) result;
}

bool otaUpgradeClusterImageNotify(uint64_t eui64, uint8_t endpointId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint8_t payload[2];
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, 2, ZIO_WRITE);
    zigbeeIOPutUint8(zio, PAYLOAD_TYPE_QUERY_JITTER);
    zigbeeIOPutUint8(zio, JITTER_MAX);

    return zigbeeSubsystemSendCommand(eui64,
                                      endpointId,
                                      OTA_UPGRADE_CLUSTER_ID,
                                      false,
                                      OTA_IMAGE_NOTIFY_COMMAND_ID,
                                      payload,
                                      2) == 0;
}

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *firmwareUpgradeState =
            deviceServiceGetResourceById(deviceUuid,
                                       NULL,
                                       COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS);

    if (firmwareUpgradeState != NULL &&
        stringCompare(firmwareUpgradeState->value, FIRMWARE_UPDATE_STATUS_PENDING, true) == 0)
    {
        //this device has a pending firmware upgrade, send an image notify while it is polling
        otaUpgradeClusterImageNotify(eui64, endpointId);
    }
}
#endif //BARTON_CONFIG_ZIGBEE