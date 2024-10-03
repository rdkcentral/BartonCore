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

#ifndef ZILKER_IASACECLUSTER_H
#define ZILKER_IASACECLUSTER_H

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
#endif // ZILKER_IASACECLUSTER_H
