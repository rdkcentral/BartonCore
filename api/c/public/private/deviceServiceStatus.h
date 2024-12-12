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
// Created by tlea on 12/1/22.
//
#pragma once

#include "icTypes/icLinkedList.h"

#define SUBSYSTEM_STATUS_COMMON_READY   "ready"

//"matter" subsystem well known status keys
#define SUBSYSTEM_STATUS_MATTER_NODE_ID "nodeId"

#define SUBSYSTEM_STATUS_MATTER_IPK     "ipk"

typedef struct
{
    icLinkedList *supportedDeviceClasses;   // the list of currently supported device classes
    bool discoveryRunning;                  // true if discovery is actively running
    icLinkedList *discoveringDeviceClasses; // the list of device classes currently being discovered (or empty for all)
    uint32_t discoveryTimeoutSeconds; // the number of seconds the discovery was requested to run (or 0 for forever)
    bool findingOrphanedDevices; // true if discovery is actively running but we are just looking for orphaned devices
    bool isReadyForDeviceOperation; // true if device service is ready to process paired devices' related operations
    bool isReadyForPairing;         // true if device service is ready and allowlist and denylist are up to date

    // maps of subsystem names to cJSON * values which contain name/value pairs of subsystem specific status
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
