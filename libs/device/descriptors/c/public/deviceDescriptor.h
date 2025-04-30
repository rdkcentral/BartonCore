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
// Created by Thomas Lea on 7/31/15.
//

#ifndef FLEXCORE_DEVICEDESCRIPTOR_H
#define FLEXCORE_DEVICEDESCRIPTOR_H

#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>

typedef enum
{
    DEVICE_DESCRIPTOR_TYPE_UNKNOWN,
    DEVICE_DESCRIPTOR_TYPE_LEGACY_ZIGBEE,
    DEVICE_DESCRIPTOR_TYPE_ZIGBEE,
    DEVICE_DESCRIPTOR_TYPE_CAMERA
} DeviceDescriptorType;

typedef enum
{
    CAMERA_PROTOCOL_UNKNOWN,
    CAMERA_PROTOCOL_LEGACY,
    CAMERA_PROTOCOL_OPENHOME
} CameraProtocol;

typedef enum
{
    DEVICE_FIRMWARE_TYPE_UNKNOWN,
    DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA,
    DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY,
    DEVICE_FIRMWARE_TYPE_CAMERA
} DeviceFirmwareType;

typedef enum
{
    POST_UPGRADE_ACTION_NONE,
    POST_UPGRADE_ACTION_RECONFIGURE
} PostUpgradeAction;

typedef struct
{
    char *version;
    icLinkedList *fileInfos;
    DeviceFirmwareType type;
    PostUpgradeAction upgradeAction;
} DeviceFirmware;

typedef struct
{
    char *fileName;
    char *checksum; // optional, could be NULL
} DeviceFirmwareFileInfo;

typedef enum
{
    DEVICE_VERSION_LIST_TYPE_UNKNOWN,
    DEVICE_VERSION_LIST_TYPE_LIST,
    DEVICE_VERSION_LIST_TYPE_WILDCARD,
    DEVICE_VERSION_LIST_TYPE_RANGE,
    DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE
} DeviceVersionListType;

typedef struct
{
    DeviceVersionListType listType;
    char *format; // optional
    struct
    {
        icLinkedList *versionList;
        bool versionIsWildcarded;
        struct
        {
            char *from;
            char *to;
        } versionRange;
    } list;

} DeviceVersionList;

typedef enum
{
    CATEGORY_TYPE_UNKNOWN = 0,
    CATEGORY_TYPE_ZIGBEE,
    CATEGORY_TYPE_CAMERA,
    CATEGORY_TYPE_PHILIPS_HUE,
    CATEGORY_TYPE_MATTER,
} CategoryType;

typedef struct
{
    uint16_t vendorId;
    uint16_t productId;
} MatterTechnology;

typedef struct
{
    DeviceDescriptorType deviceDescriptorType;
    char *uuid;
    char *description; // optional, could be NULL
    char *manufacturer;
    char *model;
    DeviceVersionList *hardwareVersions; // could be complex list, wildcard, or range.  Use version functions to access
    DeviceVersionList *firmwareVersions; // could be complex list, wildcard, or range.  Use version functions to access
    char *minSupportedFirmwareVersion;   // optional, could be NULL
    icStringHashMap *metadata;           // optional name/value pairs of strings
    DeviceFirmware *latestFirmware;      // optional
    bool cascadeAddDeleteEndpoints;      // optional (only for multi-endpoint devices). Defaults to false
    CategoryType category;
    MatterTechnology *matter; // optional (only for matter devices). Can be NULL
} DeviceDescriptor;

typedef struct
{
    bool enabled;

    struct
    {
        int low;
        int medium;
        int high;
    } sensitivity;

    struct
    {
        int low;
        int medium;
        int high;
    } detectionThreshold;

    struct
    {
        int width;
        int height;
        int bottom;
        int top;
        int left;
        int right;
    } regionOfInterest;
} CameraMotionSettings;

typedef struct
{
    DeviceDescriptor baseDescriptor; // must be first to support casting
    CameraProtocol protocol;
    CameraMotionSettings defaultMotionSettings; // optional
} CameraDeviceDescriptor;

typedef struct
{
    DeviceDescriptor baseDescriptor;

} ZigbeeDeviceDescriptor;

/*
 * Free any memory allocated for this device descriptor.  Caller may still need to free the DeviceDescriptor
 * pointer if it is in heap.
 */
void deviceDescriptorFree(DeviceDescriptor *dd);
inline void deviceDescriptorFree__auto(DeviceDescriptor **dd)
{
    deviceDescriptorFree(*dd);
}

#define scoped_DeviceDescriptor AUTO_CLEAN(deviceDescriptorFree__auto) DeviceDescriptor

/*
 * Display a device descriptor to the info log.  arg is provided for compatibility with the iterator callback func
 * and is ignored.
 */
bool deviceDescriptorPrint(DeviceDescriptor *dd, void *arg);

/*
 * Clone a device descriptor.
 */
DeviceDescriptor *deviceDescriptorClone(const DeviceDescriptor *dd);

/*
 * Cleanup DeviceFirmwareFileInfo node from the linked list
 */
void destroyDeviceFirmwareFileInfo(void *item);

/**
 * @brief Get a metadata value from a device descriptor, by key
 *
 * @param dd
 * @param key
 * @return const char* The value. This is owned by 'dd'
 */
const char *deviceDescriptorGetMetadataValue(const DeviceDescriptor *dd, const char *key);

#endif // FLEXCORE_DEVICEDESCRIPTOR_H
