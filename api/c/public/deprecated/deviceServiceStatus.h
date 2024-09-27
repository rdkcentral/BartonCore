//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2022 Comcast
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
// Created by tlea on 12/1/22.
//
#pragma once

#include "icTypes/icLinkedList.h"

#define SUBSYSTEM_STATUS_COMMON_READY "ready"

//"matter" subsystem well known status keys
#define SUBSYSTEM_STATUS_MATTER_NODE_ID "nodeId"

#define SUBSYSTEM_STATUS_MATTER_IPK "ipk"

typedef struct
{
    icLinkedList *supportedDeviceClasses; // the list of currently supported device classes
    bool discoveryRunning; // true if discovery is actively running
    icLinkedList *discoveringDeviceClasses; // the list of device classes currently being discovered (or empty for all)
    uint32_t discoveryTimeoutSeconds; // the number of seconds the discovery was requested to run (or 0 for forever)
    bool findingOrphanedDevices; // true if discovery is actively running but we are just looking for orphaned devices
    bool isReadyForDeviceOperation; // true if device service is ready to process paired devices' related operations
    bool isReadyForPairing; // true if device service is ready and allowlist and denylist are up to date

    //maps of subsystem names to cJSON * values which contain name/value pairs of subsystem specific status
    icHashMap *subsystemsJsonStatus;
} DeviceServiceStatus;

/**
 * The IPC layer can deliver a DeviceServiceStatus instance in an event which can be triggered
 * by various reasons identified by a supplied DeviceServiceStatusChangedReason value.
 */
typedef enum DeviceServiceStatusChangedReason
{
    DeviceServiceStatusChangedReasonInvalid = 0,
    DeviceServiceStatusChangedReasonReadyForPairing,
    DeviceServiceStatusChangedReasonReadyForDeviceOperation,
    DeviceServiceStatusChangedReasonSubsystemStatus
} DeviceServiceStatusChangedReason;

/**
 * Destroy a DeviceServiceStatus object
 *
 * @param status
 */
void deviceServiceStatusDestroy(DeviceServiceStatus *status);

DEFINE_DESTRUCTOR(DeviceServiceStatus, deviceServiceStatusDestroy)

#define scoped_DeviceServiceStatus DECLARE_SCOPED(DeviceServiceStatus, deviceServiceStatusDestroy)

/**
 * Unmarshall DeviceServiceStatus from a JSON string.
 *
 * @param json JSON representing a DeviceServiceStatus
 * @return a DeviceServiceStatus object or NULL on failure.  Caller frees.
 */
DeviceServiceStatus *deviceServiceStatusFromJson(const char *json);

/**
 * Marshall a DeviceServiceStatus to a JSON string.
 *
 * @param status the object to marshall
 * @return the JSON string or NULL on failure.  Caller frees.
 */
char *deviceServiceStatusToJson(const DeviceServiceStatus *status);
