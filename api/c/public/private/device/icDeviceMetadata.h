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
/*
 * Created by Thomas Lea on 9/15/15.
 */

#pragma once

#include "icDeviceEndpoint.h"
#include <cjson/cJSON.h>
#include <glib-object.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <serial/icSerDesContext.h>

typedef struct
{
    char *id;
    char *uri;
    char *endpointId; // or NULL if on root device
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
bool metadataUriIsValid(const char *deviceMetadataUri,
                        const char *deviceUuid,
                        const char *endpointId,
                        const char *metadataId);
