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

/*
 * Created by Thomas Lea on 7/22/15.
 */
#pragma once

#include <cjson/cJSON.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <glib-object.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>
#include <serial/icSerDesContext.h>
#include <stddef.h>

// NOTE: the contents of icDevice is all exposed here for now until we are confident in the data model.
//  Later we may want to hide behind an opaque type.
typedef struct
{
    char *uuid;
    char *deviceClass;
    uint8_t deviceClassVersion;
    char *uri; // this is likely just "/[device class]/[uuid]"
    char *managingDeviceDriver;
    icLinkedList *endpoints;
    icLinkedList *resources;
    icLinkedList *metadata;
} icDevice;

void deviceDestroy(icDevice *device);
inline void deviceDestroy__auto(icDevice **device)
{
    deviceDestroy(*device);
}

inline void deviceListDestroy(icLinkedList *deviceList)
{
    linkedListDestroy(deviceList, (linkedListItemFreeFunc) deviceDestroy);
}

inline void deviceListDestroy__auto(icLinkedList **deviceList)
{
    deviceListDestroy(*deviceList);
}

#define scoped_icDevice     AUTO_CLEAN(deviceDestroy__auto) icDevice
#define scoped_icDeviceList AUTO_CLEAN(deviceListDestroy__auto) icLinkedList

void devicePrint(icDevice *device, const char *prefix);

/**
 * Clone a device
 *
 * @param device the device to clone
 * @return the cloned device object
 */
icDevice *deviceClone(const icDevice *device);

/**
 * Convert device object to JSON
 *
 * @param device the device to convert
 * @return the JSON object
 */
cJSON *deviceToJSON(const icDevice *device, const icSerDesContext *context);

/**
 * Load a device into memory from JSON
 *
 * @param json the JSON to load
 * @param context A collection of k-v string:string properties
 * @param permissive When true, ignore any invalid data, where json would otherwise produce a valid device.
 * @return the device object or NULL if there is an error
 */
icDevice *deviceFromJSON(cJSON *json, const icSerDesContext *context, bool permissive);

/**
 * Retrieve a metadata item from the provided device, if it exists.
 *
 * @param device
 * @param key
 * @return the metadata value or NULL if not found
 */
const char *deviceGetMetadata(const icDevice *device, const char *key);

/**
 * Retrieve a resource value from a device, by ID
 * @param device
 * @param resourceId
 * @return the resource value or NULL if unset or nonexistent.
 */
const char *deviceGetResourceValueById(const icDevice *device, const char *resourceId);

/**
 * verify device uri pattern is valid or not
 * @param deviceUri
 * @param deviceUuid
 * @return true if valid device uri pattern, otherwise false
 */
bool deviceUriIsValid(const char *deviceUri, const char *deviceUuid);
