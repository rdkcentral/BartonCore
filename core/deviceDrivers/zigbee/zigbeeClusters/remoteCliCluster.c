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
// Created by tlea on 11/8/2021
//

#include <icLog/logging.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/remoteCliCluster.h"

#define LOG_TAG "remoteCliCluster"

typedef struct
{
    ZigbeeCluster cluster;

    const RemoteCliClusterCallbacks *callbacks;
    const void *callbackContext;
} RemoteCliCluster;

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);

ZigbeeCluster *remoteCliClusterCreate(const RemoteCliClusterCallbacks *callbacks, void *callbackContext)
{
    RemoteCliCluster *result = (RemoteCliCluster *) calloc(1, sizeof(RemoteCliCluster));

    result->cluster.clusterId = REMOTE_CLI_CLUSTER_ID;
    result->cluster.handleClusterCommand = handleClusterCommand;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool remoteCliClusterEnable(uint64_t eui64, uint8_t endpointId, uint8_t enable, uint16_t longPollInterval)
{
    uint8_t payload[] = {enable, (longPollInterval & 0xff), (longPollInterval & 0xff00) >> 8};

    return (zigbeeSubsystemSendMfgCommandWithEncryption(eui64,
                                                        endpointId,
                                                        REMOTE_CLI_CLUSTER_ID,
                                                        true,
                                                        REMOTE_CLI_ENABLE_REMOTE_CLI_COMMAND_ID,
                                                        COMCAST_MFG_ID,
                                                        payload,
                                                        sizeof(payload)) == 0);
}

bool remoteCliClusterSendCommand(uint64_t eui64, uint8_t endpointId, const char *command)
{
    uint16_t payloadLen = strlen(command) + 1; //+1 for length prefix, not null
    scoped_generic char *payload = malloc(payloadLen);
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, payloadLen, ZIO_WRITE);
    zigbeeIOPutString(zio, (char *) command);

    return (zigbeeSubsystemSendMfgCommandWithEncryption(eui64,
                                                        endpointId,
                                                        REMOTE_CLI_CLUSTER_ID,
                                                        true,
                                                        REMOTE_CLI_CLI_COMMAND_COMMAND_ID,
                                                        COMCAST_MFG_ID,
                                                        payload,
                                                        payloadLen) == 0);
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __func__);

    RemoteCliCluster *cluster = (RemoteCliCluster *) ctx;

    if (command->mfgSpecific == true && command->mfgCode == COMCAST_MFG_ID &&
        command->commandId == REMOTE_CLI_CLI_COMMAND_RESPONSE_COMMAND_ID)
    {
        if (cluster->callbacks->handleCliCommandResponse != NULL)
        {
            sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);
            scoped_generic char *response = zigbeeIOGetString(zio);

            cluster->callbacks->handleCliCommandResponse(
                command->eui64, command->sourceEndpoint, response, cluster->callbackContext);
        }
    }

    return true;
}

#endif // BARTON_CONFIG_ZIGBEE
