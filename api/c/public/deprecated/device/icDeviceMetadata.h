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
 * Created by Thomas Lea on 9/15/15.
 */

#pragma once

#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <cjson/cJSON.h>
#include <serial/icSerDesContext.h>
#include "icDeviceEndpoint.h"
#include <glib-object.h>

typedef struct
{
    char *id;
    char *uri;
    char *endpointId; //or NULL if on root device
    char *deviceUuid;
    char *value;
} icDeviceMetadata;

void metadataDestroy(icDeviceMetadata *metadata);
inline void metadataDestroy__auto(icDeviceMetadata **metadata)
{
    metadataDestroy(*metadata);
}

#define scoped_icDeviceMetadata AUTO_CLEAN(metadataDestroy__auto) icDeviceMetadata

void metadataPrint(icDeviceMetadata *metadata, const char *prefix);

/**
 * Clone a metadata object
 *
 * @param metadata the metadata to clone
 * @return the metadata object
 */
icDeviceMetadata *metadataClone(const icDeviceMetadata *metadata);

/**
 * Convert metadata object to JSON
 *
 * @param metadata the metadata to convert
 * @return the JSON object
 */
cJSON *metadataToJSON(const icDeviceMetadata *metadata, const icSerDesContext *context);

/**
 * Convert a list of metadata objects to a JSON object with id as key
 *
 * @param metadatas the list of metadata
 * @return the JSON object
 */
cJSON *metadatasToJSON(icLinkedList *metadatas, const icSerDesContext *context);

/**
 * Load a device metadata into memory from JSON
 *
 * @param deviceUUID the device UUID for which we are loading the metadata
 * @param endpointId the endpoint ID for which we are loading the metdata
 * @param metadataJson the JSON to load
 * @return the metadata object or NULL if there is an error
 */
icDeviceMetadata *metadataFromJSON(const char *deviceUUID, const char *endpointId, cJSON *metadataJSON);

/**
 * Load the metadata for a device and endpoint from JSON
 *
 * @param deviceUUID  the device UUID
 * @param endpointId the endpoint ID
 * @param metadatasJson the JSON to load
 * @param permissive when true, ignore and drop invalid metadatas
 * @return linked list of metadata structures, caller is responsible for destroying result
 *
 * @see linkedListDestroy
 * @see metadataDestroy
 */
icLinkedList *metadatasFromJSON(const char *deviceUUID, const char *endpointId, cJSON *metadatasJSON, bool permissive);

/**
 * Create a metadata URI
 * @param deviceUuid
 * @param [nullable] endpointId Use NULL to create a metadata URI on the root device
 * @param name The metadata entry's name/key
 * @return [non-null] A URI representing the metadata
 */
char *metadataUriCreate(const char *deviceUuid, const char *endpointId, const char *name);

/**
 * verify metadata uri pattern is valid or not
 * @param deviceMetadataUri
 * @param deviceUuid
 * @param endpointId
 * @param metadataId
 * @return true if valid metadata uri pattern, otherwise false
 */
bool metadataUriIsValid(const char* deviceMetadataUri,
                        const char* deviceUuid,
                        const char *endpointId,
                        const char *metadataId);

