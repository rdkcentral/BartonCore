//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by Thomas Lea on 7/31/15.
//

#include <deviceDescriptor.h>
#include <icLog/logging.h>
#include <stddef.h>
#include <icTypes/icLinkedList.h>
#include <stdlib.h>
#include <inttypes.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>

#include <string.h>

#define LOG_TAG "deviceDescriptor"

extern inline void deviceDescriptorFree__auto(DeviceDescriptor **dd);

static void freeDeviceVersionList(DeviceVersionList *list)
{
    if(list != NULL)
    {
        free(list->format);
        linkedListDestroy(list->list.versionList, NULL);
        free(list->list.versionRange.from);
        free(list->list.versionRange.to);
        free(list);
    }
}

static void *cloneString(void *item, void *context)
{
    (void)(context); // prevent unused parameter warning
    return strdupOpt((char *) item);
}

static void *cloneDeviceFirmwareFileInfo(void *item, void *context)
{
    (void)(context); // prevent unused parameter warning

    DeviceFirmwareFileInfo *info = (DeviceFirmwareFileInfo *)calloc(1, sizeof(DeviceFirmwareFileInfo));
    DeviceFirmwareFileInfo *tempInfo = (DeviceFirmwareFileInfo *)item;

    info->fileName = strdupOpt(tempInfo->fileName);
    info->checksum = strdupOpt(tempInfo->checksum);

    return info;
}

static DeviceVersionList *cloneDeviceVersionList(DeviceVersionList *list)
{
    DeviceVersionList *newList = NULL;
    if (list != NULL)
    {
        newList = (DeviceVersionList *)calloc(1, sizeof(DeviceVersionList));
        newList->format = strdupOpt(list->format);
        newList->listType = list->listType;
        newList->list.versionList = linkedListDeepClone(list->list.versionList, cloneString, NULL);
        newList->list.versionRange.from = strdupOpt(list->list.versionRange.from);
        newList->list.versionRange.to = strdupOpt(list->list.versionRange.to);
    }

    return newList;
}

static DeviceFirmware *cloneDeviceFirmware(DeviceFirmware *deviceFirmware)
{
    DeviceFirmware *newDeviceFirmware = NULL;
    if (deviceFirmware != NULL)
    {
        newDeviceFirmware = (DeviceFirmware *)calloc(1, sizeof(DeviceFirmware));
        newDeviceFirmware->type = deviceFirmware->type;
        newDeviceFirmware->version = strdup(deviceFirmware->version);
    }

    return newDeviceFirmware;
}

/*
 * Free any memory allocated for this device descriptor.  Caller may still need to free the DeviceDescriptor
 * pointer if it is in heap.
 */
void deviceDescriptorFree(DeviceDescriptor *dd)
{
    if(dd != NULL)
    {
        free(dd->uuid);
        free(dd->description);
        free(dd->manufacturer);
        free(dd->model);
        freeDeviceVersionList(dd->hardwareVersions);
        freeDeviceVersionList(dd->firmwareVersions);
        free(dd->minSupportedFirmwareVersion);
        stringHashMapDestroy(dd->metadata, NULL);
        if(dd->latestFirmware != NULL)
        {
            free(dd->latestFirmware->version);
            if (dd->latestFirmware->fileInfos != NULL)
            {
                linkedListDestroy(dd->latestFirmware->fileInfos, destroyDeviceFirmwareFileInfo);
            }
            free(dd->latestFirmware);
        }
        free(dd->matter);

        free(dd);
    }
}

/*
 * Cleanup DeviceFirmwareFileInfo node from the linked list
 */
void destroyDeviceFirmwareFileInfo(void *item)
{
    if (item != NULL)
    {
        DeviceFirmwareFileInfo *info = (DeviceFirmwareFileInfo *)item;
        free(info->fileName);
        free(info->checksum);
        free(info);
    }
}

static void printVersionList(const char *prefix, DeviceVersionList *versionList)
{
    if (versionList != NULL)
    {
        if (versionList->listType == DEVICE_VERSION_LIST_TYPE_WILDCARD)
        {
            icLogInfo(LOG_TAG, "%s: any", prefix);
        }
        else
        {
            if (versionList->list.versionList != NULL)
            {
                icLogInfo(LOG_TAG, "%s: (format=%s) version list:", prefix, versionList->format);
                scoped_icLinkedListIterator *it = linkedListIteratorCreate(versionList->list.versionList);
                while (linkedListIteratorHasNext(it))
                {
                    char *next = (char *) linkedListIteratorGetNext(it);
                    icLogInfo(LOG_TAG, "\t\t%s", next);
                }
            }

            if (versionList->list.versionRange.from != NULL ||
                versionList->list.versionRange.to != NULL)
            {
                icLogInfo(LOG_TAG, "%s: range from=%s, to=%s",
                          prefix,
                          stringCoalesce(versionList->list.versionRange.from),
                          stringCoalesce(versionList->list.versionRange.to));
            }

        }
    }
}

/*
 * Display a device descriptor to the info log.  arg is provided for compatibility with the interator callback func
 * and is ignored.
 */
bool deviceDescriptorPrint(DeviceDescriptor *dd, void *arg)
{
    if(dd == NULL)
    {
        icLogInfo(LOG_TAG, "NULL DeviceDescriptor");
        return true;
    }

    switch(dd->deviceDescriptorType)
    {
        case DEVICE_DESCRIPTOR_TYPE_CAMERA:
            icLogInfo(LOG_TAG, "DeviceDescriptor (Camera)");
            break;

        case DEVICE_DESCRIPTOR_TYPE_ZIGBEE:
            icLogInfo(LOG_TAG, "DeviceDescriptor (ZigBee)");
            break;

        case DEVICE_DESCRIPTOR_TYPE_LEGACY_ZIGBEE:
            icLogInfo(LOG_TAG, "DeviceDescriptor (Legacy ZigBee)");
            break;

        default:
            icLogInfo(LOG_TAG, "Unsupported DeviceDescriptor type (%d)!", dd->deviceDescriptorType);
            return true;
    }

    icLogInfo(LOG_TAG, "\tuuid: %s", dd->uuid);
    icLogInfo(LOG_TAG, "\tdescription: %s", dd->description);
    icLogInfo(LOG_TAG, "\tmanufacturer: %s", dd->manufacturer);
    icLogInfo(LOG_TAG, "\tmodel: %s", dd->model);
    printVersionList("\thardwareVersions", dd->hardwareVersions);
    printVersionList("\tfirmwareVersions", dd->firmwareVersions);
    icLogInfo(LOG_TAG, "\tminSupportedFirmwareVersion: %s", dd->minSupportedFirmwareVersion);

    switch(dd->category)
    {
        case CATEGORY_TYPE_UNKNOWN:
            icLogInfo(LOG_TAG, "Category: Unknown");
            break;

        case CATEGORY_TYPE_ZIGBEE:
            icLogInfo(LOG_TAG, "Category: Zigbee");
            break;

        case CATEGORY_TYPE_CAMERA:
            icLogInfo(LOG_TAG, "Category: Camera");
            break;

        case CATEGORY_TYPE_PHILIPS_HUE:
            icLogInfo(LOG_TAG, "Category: Philips Hue");
            break;

        case CATEGORY_TYPE_MATTER:
            icLogInfo(LOG_TAG, "Category: Matter");
            break;

        default:
            icLogInfo(LOG_TAG, "Unexpected Category: %d!", dd->category);
            return true;
    }

    if(dd->metadata != NULL)
    {
        icLogInfo(LOG_TAG, "\tmetadata:");
        icStringHashMapIterator *it = stringHashMapIteratorCreate(dd->metadata);
        while(stringHashMapIteratorHasNext(it))
        {
            char *name;
            char *value;

            stringHashMapIteratorGetNext(it, &name, &value);
            icLogInfo(LOG_TAG, "\t\t%s = %s", name, value);
        }

        stringHashMapIteratorDestroy(it);
    }

    if (dd->latestFirmware != NULL)
    {
        icLogInfo(LOG_TAG, "\tlatestFirmware: version %s", dd->latestFirmware->version);
        if (dd->latestFirmware->fileInfos != NULL)
        {
            icLinkedListIterator *it = linkedListIteratorCreate(dd->latestFirmware->fileInfos);
            while (linkedListIteratorHasNext(it) == true)
            {
                DeviceFirmwareFileInfo *next = (DeviceFirmwareFileInfo *)linkedListIteratorGetNext(it);
                icLogInfo(LOG_TAG, "\t\tfilename: %s checksum: %s",
                          stringCoalesce(next->fileName), stringCoalesceAlt(next->checksum, "(none)"));
            }
            linkedListIteratorDestroy(it);
        }
    }

    if (dd->matter != NULL)
    {
        MatterTechnology *matter = dd->matter;

        icLogInfo(LOG_TAG, "\tMatter Technology:");
        icLogInfo(LOG_TAG, "\t\tvendorId: 0x%04" PRIx16, matter->vendorId);
        icLogInfo(LOG_TAG, "\t\tproductId: 0x%04" PRIx16, matter->productId);
    }

    if(dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
    {
        CameraDeviceDescriptor *cdd = (CameraDeviceDescriptor*)dd;
        switch(cdd->protocol)
        {
            case CAMERA_PROTOCOL_LEGACY:
                icLogInfo(LOG_TAG, "\tprotocol: legacy");
                break;

            case CAMERA_PROTOCOL_OPENHOME:
                icLogInfo(LOG_TAG, "\tprotocol: openHome");
                break;

            default:
                icLogInfo(LOG_TAG, "\tprotocol: UNKNOWN!");
                break;
        }

        if(cdd->defaultMotionSettings.enabled == true)
        {
            icLogInfo(LOG_TAG, "\tmotion enabled:");
            icLogInfo(LOG_TAG, "\t\tsensitivity (low,medium,high): %d,%d,%d",
                      cdd->defaultMotionSettings.sensitivity.low,
                      cdd->defaultMotionSettings.sensitivity.medium,
                      cdd->defaultMotionSettings.sensitivity.high);

            icLogInfo(LOG_TAG, "\t\tdetectionThreshold (low,medium,high): %d,%d,%d",
                      cdd->defaultMotionSettings.detectionThreshold.low,
                      cdd->defaultMotionSettings.detectionThreshold.medium,
                      cdd->defaultMotionSettings.detectionThreshold.high);

            icLogInfo(LOG_TAG, "\t\tregionOfInterest (width, height, bottom, top, left, right): %d,%d,%d,%d,%d,%d",
                      cdd->defaultMotionSettings.regionOfInterest.width,
                      cdd->defaultMotionSettings.regionOfInterest.height,
                      cdd->defaultMotionSettings.regionOfInterest.bottom,
                      cdd->defaultMotionSettings.regionOfInterest.top,
                      cdd->defaultMotionSettings.regionOfInterest.left,
                      cdd->defaultMotionSettings.regionOfInterest.right);
        }
        else
        {
            icLogInfo(LOG_TAG, "\tmotion disabled");
        }
    }

    return true;
}


/*
 * Clone a device descriptor.
 */
DeviceDescriptor *deviceDescriptorClone(const DeviceDescriptor *dd)
{
    DeviceDescriptor *newDD = NULL;
    if(dd != NULL)
    {
        // Create it of the right size based on the type
        if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
        {
            newDD = (DeviceDescriptor *) calloc(1, sizeof(CameraDeviceDescriptor));
        }
        else
        {
            newDD = (DeviceDescriptor *) calloc(1, sizeof(ZigbeeDeviceDescriptor));
        }
        newDD->uuid = strdupOpt(dd->uuid);
        newDD->deviceDescriptorType = dd->deviceDescriptorType;
        newDD->description = strdupOpt(dd->description);
        newDD->manufacturer = strdupOpt(dd->manufacturer);
        newDD->model = strdupOpt(dd->model);
        // Deep clone device version list
        newDD->hardwareVersions = cloneDeviceVersionList(dd->hardwareVersions);
        newDD->firmwareVersions = cloneDeviceVersionList(dd->firmwareVersions);
        newDD->minSupportedFirmwareVersion = strdupOpt(dd->minSupportedFirmwareVersion);
        if (dd->metadata != NULL)
        {
            newDD->metadata = stringHashMapDeepClone(dd->metadata);
        }
        if(dd->latestFirmware != NULL)
        {
            // Deep clone latestFirmware
            newDD->latestFirmware = (DeviceFirmware *)calloc(1, sizeof(DeviceFirmware));
            newDD->latestFirmware->version = strdupOpt(dd->latestFirmware->version);
            if (dd->latestFirmware->fileInfos != NULL)
            {
                newDD->latestFirmware->fileInfos = linkedListDeepClone(dd->latestFirmware->fileInfos, cloneDeviceFirmwareFileInfo, NULL);
            }
            newDD->latestFirmware->type = dd->latestFirmware->type;
            newDD->latestFirmware->upgradeAction = dd->latestFirmware->upgradeAction;
        }
        newDD->cascadeAddDeleteEndpoints = dd->cascadeAddDeleteEndpoints;

        newDD->category = dd->category;

        if (dd->matter != NULL)
        {
            newDD->matter = (MatterTechnology *) calloc(1, sizeof(MatterTechnology));
            newDD->matter->vendorId = dd->matter->vendorId;
            newDD->matter->productId = dd->matter->productId;
        }

        // Make sure to do the extra camera bits if necessary
        if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
        {
            CameraDeviceDescriptor *cameraDD = (CameraDeviceDescriptor *)dd;
            CameraDeviceDescriptor *newCameraDD = (CameraDeviceDescriptor *)newDD;
            newCameraDD->protocol = cameraDD->protocol;
            newCameraDD->defaultMotionSettings = cameraDD->defaultMotionSettings;
        }
    }

    return newDD;
}

const char *deviceDescriptorGetMetadataValue(const DeviceDescriptor *dd,
                                             const char *key) {
    if (dd == NULL || dd->metadata == NULL) {
        return NULL;
    }

    return stringHashMapGet(dd->metadata, key);
}
