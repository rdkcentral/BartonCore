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


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/iasZoneCluster.h"
#include <errno.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <string.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <zigbeeClusters/helpers/comcastBatterySavingHelper.h>

#define LOG_TAG "iasZoneCluster"

typedef struct
{
    ZigbeeCluster cluster;
    const IASZoneClusterCallbacks *callbacks;
    const void *callbackContext;
} IASZoneCluster;

/* Callback implementations */
static bool handleClusterCommand(ZigbeeCluster *cluster, ReceivedClusterCommand *command);
static bool configureCluster(ZigbeeCluster *cluster, const DeviceConfigurationContext *configContext);

/* Private functions */
static int readZoneStatusPayload(IASZoneStatusChangedNotification *payload, const ReceivedClusterCommand *command);

ZigbeeCluster *iasZoneClusterCreate(const IASZoneClusterCallbacks *callbacks, const void *callbackContext)
{
    IASZoneCluster *_this = calloc(1, sizeof(IASZoneCluster));

    _this->cluster.clusterId = IAS_ZONE_CLUSTER_ID;
    _this->callbackContext = callbackContext;
    _this->cluster.handleClusterCommand = handleClusterCommand;
    _this->cluster.configureCluster = configureCluster;
    _this->callbacks = callbacks;
    _this->cluster.priority = CLUSTER_PRIORITY_HIGHEST;

    return (ZigbeeCluster *) _this;
}

static bool handleClusterCommand(ZigbeeCluster *cluster, ReceivedClusterCommand *command)
{
    bool handled = false;
    IASZoneCluster *_this = (IASZoneCluster *) cluster;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (command->clusterId == IAS_ZONE_CLUSTER_ID && command->fromServer == true)
    {
        switch (command->commandId)
        {
            case IAS_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID:
            {
                IASZoneStatusChangedNotification payload;
                ComcastBatterySavingData *batterySavingData = NULL; // This is optional

                if (readZoneStatusPayload(&payload, command) != 0)
                {
                    return false;
                }

                if (command->mfgSpecific &&
                    (command->mfgCode == COMCAST_MFG_ID_INCORRECT || command->mfgCode == COMCAST_MFG_ID))
                {
                    // skip the common 6 bytes of the standard zone status message
                    batterySavingData =
                        comcastBatterySavingDataParse(command->commandData + 6, command->commandDataLen - 6);
                }

                if (_this->callbacks->onZoneStatusChanged)
                {
                    _this->callbacks->onZoneStatusChanged(
                        command->eui64, command->sourceEndpoint, &payload, batterySavingData, _this->callbackContext);
                    handled = true;
                }

                if (batterySavingData && _this->callbacks->processComcastBatterySavingData)
                {
                    _this->callbacks->processComcastBatterySavingData(
                        command->eui64, batterySavingData, _this->callbackContext);
                    handled = true;
                }

                free(batterySavingData);

                break;
            }
            case IAS_ZONE_ENROLL_REQUEST_COMMAND_ID:
            {
                if (_this->callbacks->onZoneEnrollRequested)
                {
                    errno = 0;
                    sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);
                    IASZoneType zoneType = zigbeeIOGetUint16(zio);
                    uint16_t mfgCode = zigbeeIOGetUint16(zio);
                    if (errno)
                    {
                        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
                        icLogError(LOG_TAG, "Unable to read zigbee enroll request command payload: %s", errStr);
                        return false;
                    }
                    _this->callbacks->onZoneEnrollRequested(
                        command->eui64, command->sourceEndpoint, zoneType, mfgCode, _this->callbackContext);
                    handled = true;
                }
                break;
            }
            default:
                icLogWarn(LOG_TAG, "IAS Zone command id 0x%02" PRIx8 "not supported", command->commandId);
                break;
        }
    }

    return handled;
}

static int readZoneStatusPayload(IASZoneStatusChangedNotification *payload, const ReceivedClusterCommand *command)
{
    int rc = 0;

    memset(payload, 0, sizeof(*payload));
    sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);

    // Standard and Comcast versions start off with common data
    payload->zoneStatus = zigbeeIOGetUint16(zio);
    payload->extendedStatus = zigbeeIOGetUint8(zio);
    payload->zoneId = zigbeeIOGetUint8(zio);
    payload->delayQS = zigbeeIOGetUint16(zio);

    return rc;
}

bool iasZoneClusterExtract(IASZoneStatusChangedNotification *payload, const ReceivedClusterCommand *command)
{
    return readZoneStatusPayload(payload, command) == 0;
}

static bool configureCluster(ZigbeeCluster *cluster, const DeviceConfigurationContext *configContext)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    // Write CIE address
    if (zigbeeSubsystemWriteNumber(configContext->eui64,
                                   configContext->endpointId,
                                   IAS_ZONE_CLUSTER_ID,
                                   true,
                                   IAS_ZONE_CIE_ADDRESS_ATTRIBUTE_ID,
                                   ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE,
                                   getLocalEui64(),
                                   sizeof(uint64_t)) == 0)
    {
        // enroll the endpoint
        uint8_t payload[2];
        sbZigbeeIOContext *zio = zigbeeIOInit(payload, 2, ZIO_WRITE);
        zigbeeIOPutUint8(zio, ZCL_STATUS_SUCCESS);
        // Zone ID
        zigbeeIOPutUint8(zio, 0);
        if (zigbeeSubsystemSendCommand(configContext->eui64,
                                       configContext->endpointId,
                                       IAS_ZONE_CLUSTER_ID,
                                       true,
                                       IAS_ZONE_CLIENT_ENROLL_RESPONSE_COMMAND_ID,
                                       payload,
                                       sizeof(payload)) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to enroll endpoint", __FUNCTION__);
            result = false;
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to write CIE Address", __FUNCTION__);
        result = false;
    }

    return result;
}

#endif // BARTON_CONFIG_ZIGBEE
