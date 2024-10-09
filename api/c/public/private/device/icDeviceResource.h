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
 *
 * Created by Thomas Lea on 7/22/15.
 */

#pragma once

#include <cjson/cJSON.h>
#include <deviceService/resourceModes.h>
#include <glib-object.h>
#include <icTypes/icLinkedList.h>
#include <serial/icSerDesContext.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CACHING_POLICY_NEVER,  // Never cache this attribute and always call the device driver to retrieve the value
    CACHING_POLICY_ALWAYS, // Always cache this attribute and never call the device driver to retrieve the value
} ResourceCachingPolicy;

typedef struct
{
    char *id;
    char *uri;
    char *endpointId; // NULL if on the root device
    char *deviceUuid;
    char *value;
    char *type;
    uint8_t mode; // bitmask of RESOURCE_MODE_*
    ResourceCachingPolicy cachingPolicy;
    uint64_t dateOfLastSyncMillis; // GMT epoch based time when this resource was last read from or written to the
                                   // device.  0 for never.

} icDeviceResource;

void resourceDestroy(icDeviceResource *resource);
inline void resourceDestroy__auto(icDeviceResource **resource)
{
    resourceDestroy(*resource);
}

#define scoped_icDeviceResource AUTO_CLEAN(resourceDestroy__auto) icDeviceResource

void resourcePrint(icDeviceResource *resource, const char *prefix);

/**
 * Clone a device resource object
 *
 * @param resource the resource to clone
 * @return the cloned resource object
 */
icDeviceResource *resourceClone(const icDeviceResource *resource);

/**
 * Convert resource object to JSON
 *
 * @param resource the resource to convert
 * @return the JSON object
 */
cJSON *resourceToJSON(const icDeviceResource *resource, const icSerDesContext *context);

/**
 * Convert a list of resources objects to a JSON object with id as key
 *
 * @param resources the list of resources
 * @return the JSON object
 */
cJSON *resourcesToJSON(icLinkedList *resources, const icSerDesContext *context);

/**
 * Load a device resource into memory from JSON
 *
 * @param deviceUUID the deviceUUID for which we are loading the metadata
 * @param endpointId the endpointId for which we are loading the metdata
 * @param resourceJSON the JSON to load
 * @return the resource object or NULL if there is an error
 */
icDeviceResource *
resourceFromJSON(const char *deviceUUID, const char *endpointId, cJSON *resourceJSON, const icSerDesContext *context);

/**
 * Load the resources for a device and endpoint from JSON
 *
 * @param device  the device
 * @param endpoint the endpoint
 * @param resourcesJson the JSON to load
 * @param permissive when true, ignore and drop invalid resources
 * @return linked list of resource structures, caller is responsible for destroying result
 *
 * @see linkedListDestroy
 * @see resourceDestroy
 */
icLinkedList *resourcesFromJSON(const char *deviceUUID,
                                const char *endpointId,
                                cJSON *resourcesJSON,
                                const icSerDesContext *context,
                                bool permissive);

/**
 * Find a resource by ID in a resource list
 * @param device
 * @param resourceId
 * @return The resource value; NULL if not found, or not set.
 */
const char *icDeviceResourceGetValueById(const icLinkedList *resources, const char *resourceId);

/**
 * Create a resource URI
 * @param deviceUuid
 * @param [nullable] endpointId use NULL to create a resource URI for the root device
 * @param resourceId A name/key for the resource
 * @return [non-null] A URI representing the resource
 */
char *resourceUriCreate(const char *deviceUuid, const char *endpointId, const char *resourceId);

/**
 * verify resource uri pattern is valid or not
 * @param deviceResourceUri
 * @param deviceUuid
 * @param endpointId
 * @param resourceId
 * @return true if valid resource uri pattern, otherwise false
 */
bool resourceUriIsValid(const char *deviceResourceUri,
                        const char *deviceUuid,
                        const char *endpointId,
                        const char *resourceId);
