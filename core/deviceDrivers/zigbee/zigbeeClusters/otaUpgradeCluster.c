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
// Created by tlea on 9/23/19.
//


#include <commonDeviceDefs.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <memory.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/otaUpgradeCluster.h"

#define LOG_TAG                   "otaUpgradeCluster"

#define PAYLOAD_TYPE_QUERY_JITTER 0
#define JITTER_MAX                100

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

    return zigbeeSubsystemSendCommand(
               eui64, endpointId, OTA_UPGRADE_CLUSTER_ID, false, OTA_IMAGE_NOTIFY_COMMAND_ID, payload, 2) == 0;
}

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *firmwareUpgradeState =
        deviceServiceGetResourceById(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS);

    if (firmwareUpgradeState != NULL &&
        stringCompare(firmwareUpgradeState->value, FIRMWARE_UPDATE_STATUS_PENDING, true) == 0)
    {
        // this device has a pending firmware upgrade, send an image notify while it is polling
        otaUpgradeClusterImageNotify(eui64, endpointId);
    }
}
#endif // BARTON_CONFIG_ZIGBEE
