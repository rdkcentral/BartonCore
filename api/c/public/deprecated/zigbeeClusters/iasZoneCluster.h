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

#ifndef ZILKER_IASZONECLUSTER_H
#define ZILKER_IASZONECLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "comcastBatterySaving.h"
#include "device-driver/device-driver.h"
#include "zigbeeCluster.h"
#include <inttypes.h>
#include <zhal/zhal.h>

#define IAS_ZONE_CIE_ADDRESS_ATTRIBUTE_ID              0x0010
#define IAS_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID 0x00
#define IAS_ZONE_ENROLL_REQUEST_COMMAND_ID             0x01
#define IAS_ZONE_TYPE_ATTRIBUTE_ID                     0x0001
#define IAS_ZONE_STATUS_ATTRIBUTE_ID                   0x0002
#define IAS_ZONE_CLIENT_ENROLL_RESPONSE_COMMAND_ID     0x00

typedef struct
{
    uint16_t zoneStatus;
    uint8_t extendedStatus;
    uint8_t zoneId;
    uint16_t delayQS;

} IASZoneStatusChangedNotification;

typedef enum
{
    IAS_ZONE_TYPE_STANDARD_CIE = 0x0000,
    IAS_ZONE_TYPE_MOTION_SENSOR = 0x000d,
    IAS_ZONE_TYPE_CONTACT_SWITCH = 0x0015,
    IAS_ZONE_TYPE_FIRE_SENSOR = 0x0028,
    IAS_ZONE_TYPE_WATER_SENSOR = 0x002a,
    IAS_ZONE_TYPE_CO_SENSOR = 0x002b,
    IAS_ZONE_TYPE_PERSONAL_EMERGENCY_DEVICE = 0x002c,
    IAS_ZONE_TYPE_VIBRATION_MOVEMENT_SENSOR = 0x002d,
    IAS_ZONE_TYPE_REMOTE_CONTROL = 0x010f,
    IAS_ZONE_TYPE_KEYFOB = 0x0115,
    IAS_ZONE_TYPE_KEYPAD = 0x021d,
    IAS_ZONE_TYPE_STANDARD_WARNING_DEVICE = 0x0225,
    IAS_ZONE_TYPE_GLASS_BREAK_SENSOR = 0x0226,
    IAS_ZONE_TYPE_INVALID = 0xffff
} IASZoneType;

typedef enum
{
    IAS_ZONE_STATUS_ALARM1 = 1u << 0u,
    IAS_ZONE_STATUS_ALARM2 = 1u << 1u,
    IAS_ZONE_STATUS_TAMPER = 1u << 2u,
    IAS_ZONE_STATUS_BATTERY_LOW = 1u << 3u,
    IAS_ZONE_STATUS_SUPERVISION_NOTIF = 1u << 4u,
    IAS_ZONE_STATUS_RESTORE_NOTIF = 1u << 5u,
    IAS_ZONE_STATUS_TROUBLE = 1u << 6u,
    IAS_ZONE_STATUS_MAINS_FAULT = 1u << 7u,
    IAS_ZONE_STATUS_TEST = 1u << 8u,
    IAS_ZONE_STATUS_BATTERY_DEFECT = 1u << 9u
} IASZoneStatusField;

typedef struct
{
    /**
     * Handle a zone status changed notification
     * @param eui64
     * @param endpointId
     * @param notification the IAS Zone Status Notification data
     * @param batterySavingData the Comcast extension of battery saving data (or NULL if not present)
     * @param ctx The private context given to iasZoneClusterCreate
     */
    void (*onZoneStatusChanged)(const uint64_t eui64,
                                const uint8_t endpointId,
                                const IASZoneStatusChangedNotification *notification,
                                const ComcastBatterySavingData *batterySavingData,
                                const void *ctx);
    /**
     * Handle an enroll request
     * @param eui64
     * @param endpointId
     * @param type
     * @param mfgCode
     * @param ctx The private context given to iasZoneClusterCreate
     */
    void (*onZoneEnrollRequested)(const uint64_t eui64,
                                  const uint8_t endpointId,
                                  const IASZoneType type,
                                  const uint16_t mfgCode,
                                  const void *ctx);

    /**
     * Process comcast battery saving data
     * @param eui64
     * @param batterySavingData
     * @param ctx The private context given to iasZoneClusterCreate
     */
    void (*processComcastBatterySavingData)(const uint64_t eui64,
                                            const ComcastBatterySavingData *batterySavingData,
                                            const void *ctx);

} IASZoneClusterCallbacks;

/**
 * Create an IAS zone cluster instance.
 * @param callbacks
 * @param callbackContext This will be passed when IASZoneClusterCallbacks are invoked
 * @return
 */
ZigbeeCluster *iasZoneClusterCreate(const IASZoneClusterCallbacks *callbacks, const void *callbackContext);

/**
 * Internally uses the readZoneStatusPayload and extracts the payload details.
 * @param payload - the IAS Zone Status Notification data
 * @param command - the cluster command
 */
bool iasZoneClusterExtract(IASZoneStatusChangedNotification *payload, const ReceivedClusterCommand *command);

#endif // BARTON_CONFIG_ZIGBEE
#endif // ZILKER_IASZONECLUSTER_H
