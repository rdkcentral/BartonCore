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

#pragma once

#include <deviceService/zoneChanged.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "device-driver/device-driver.h"
#include "zigbeeCluster.h"

// End devices implement ACE client
#define IAS_ACE_ARM_COMMAND_ID                       0x00
#define IAS_ACE_BYPASS_COMMAND_ID                    0x01
#define IAS_ACE_EMERGENCY_COMMAND_ID                 0x02
#define IAS_ACE_FIRE_COMMAND_ID                      0x03
#define IAS_ACE_PANIC_COMMAND_ID                     0x04
#define IAS_ACE_GET_ZONE_ID_MAP_COMMAND_ID           0x05
#define IAS_ACE_GET_ZONE_INFO_COMMAND_ID             0x06
#define IAS_ACE_GET_PANEL_STATUS_COMMAND_ID          0x07
#define IAS_ACE_GET_BYPASSED_ZONE_LIST_COMMAND_ID    0x08
#define IAS_ACE_GET_ZONE_STATUS_COMMAND_ID           0x09
// ACE server commands
#define IAS_ACE_ARM_RESPONSE_COMMAND_ID              0x00
#define IAS_ACE_GET_ZONE_ID_MAP_RESPONSE_COMMAND_ID  0x01
#define IAS_ACE_GET_ZONE_INFO_RESPONSE_COMMAND_ID    0x02
#define IAS_ACE_ZONE_STATUS_CHANGED_COMMAND_ID       0x03
#define IAS_ACE_PANEL_STATUS_CHANGED_COMMAND_ID      0x04
#define IAS_ACE_GET_PANEL_STATUS_RESPONSE_COMMAND_ID 0x05
#define IAS_ACE_SET_BYPASSED_ZONE_LIST_COMMAND_ID    0x06
#define IAS_ACE_BYPASS_RESPONSE_COMMAND_ID           0x07
#define IAS_ACE_GET_ZONE_STATUS_RESPONSE_COMMAND_ID  0x08

typedef struct IASACECluster IASACECluster;

typedef struct
{
    const char *accessCode;
    PanelStatus requestedStatus;
} IASACEArmRequest;

typedef struct
{
    ArmDisarmNotification (*onArmRequestReceived)(const uint64_t eui64,
                                                  const uint8_t endpointId,
                                                  const IASACEArmRequest *request,
                                                  const void *ctx);

    void (*onPanicRequestReceived)(const uint64_t eui64,
                                   const uint8_t endpointId,
                                   const PanelStatus requestedPanic,
                                   const void *ctx);

    void (*onGetPanelStatusReceived)(const uint64_t eui64, const uint8_t endpointId, const void *ctx);
} IASACEClusterCallbacks;

ZigbeeCluster *iasACEClusterCreate(const IASACEClusterCallbacks *callbacks, const void *driver);

/**
 * Send a panel status change message.
 * @param eui64 device address
 * @param destEndpoint ACE Zigbee endpoint number
 * @param state Security state to send
 * @param isResponse Set to true to send status as a response to a 'Get Panel Status' command from the client. Else,
 *                   the command is sent as a gratuitous event.
 */
void iasACEClusterSendPanelStatus(uint64_t eui64, uint8_t destEndpoint, const SecurityState *state, bool isResponse);

/**
 * Send a zone status change message.
 * @param eui64 device address
 * @param destEndpoint ACE Zigbee endpoint number
 * @param zoneChanged zone change event to send
 */
void iasACEClusterSendZoneStatus(uint64_t eui64, uint8_t destEndpoint, const ZoneChanged *zoneChanged);


#endif // BARTON_CONFIG_ZIGBEE
