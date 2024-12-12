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
 * An endpoint is a logical device encapsulated in the physical icDevice.  Examples of endpoints
 * on an open home camera would be 'camera' and 'sensor'.
 *
 * Created by Thomas Lea on 8/11/15.
 */

#pragma once
#include <cjson/cJSON.h>
#include <glib-object.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/stringUtils.h>
#include <serial/icSerDesContext.h>

typedef struct
{
    char *id;
    char *uri;
    char *profile;
    uint8_t profileVersion;
    char *deviceUuid;
    bool enabled;
    icLinkedList *resources;
    icLinkedList *metadata;
} icDeviceEndpoint;

void endpointDestroy(icDeviceEndpoint *endpoint);
inline void endpointDestroy__auto(icDeviceEndpoint **endpoint)
{
    endpointDestroy(*endpoint);
}

#define scoped_icDeviceEndpoint AUTO_CLEAN(endpointDestroy__auto) icDeviceEndpoint

void endpointPrint(icDeviceEndpoint *endpoint, const char *prefix);

/**
 * Clone a endpoint
 *
 * @param endpoint the endpoint to clone
 * @return the cloned endpoint object
 */
icDeviceEndpoint *endpointClone(const icDeviceEndpoint *endpoint);

/**
 * Convert endpoint object to JSON
 *
 * @param endpoint the endpoint to convert
 * @return the JSON object
 */
cJSON *endpointToJSON(const icDeviceEndpoint *endpoint, const icSerDesContext *context);

/**
 * Convert a list of endpoint objects to a JSON object with id as key
 *
 * @param endpoints the list of endpoints
 * @return the JSON object
 */
cJSON *endpointsToJSON(icLinkedList *endpoints, const icSerDesContext *context);

/**
 * Load a device endpoint into memory from JSON
 *
 * @param deviceUUID the device UUID for which we are loading the metadata
 * @param endpointJSON the JSON to load
 * @return the endpoint object or NULL if there is an error
 */
icDeviceEndpoint *endpointFromJSON(const char *deviceUUID, cJSON *endpointJSON, const icSerDesContext *context);

/**
 * Load the endpoints for a device from JSON
 *
 * @param deviceUUID  the device UUID
 * @param endpointsJSON the JSON to load
 * @param permissive when true, ignore and drop bad endpoints
 * @return linked list of endpoint structures, caller is responsible for destroying result
 *
 * @see linkedListDestroy
 * @see endpointDestroy
 */
icLinkedList *
endpointsFromJSON(const char *deviceUUID, cJSON *endpointsJSON, const icSerDesContext *context, bool permissive);

/**
 * Create an endpoint URI
 * @param deviceUuid
 * @param endpointId
 * @return [non-null] a URI representing the endpoint
 */
inline char *endpointUriCreate(const char *deviceUuid, const char *endpointId)
{
    return stringBuilder("/%s/ep/%s", deviceUuid, endpointId);
}

/**
 * verify endpoint uri pattern is valid or not
 * @param deviceEndpointUri
 * @param deviceUuid
 * @param endpointId
 * @return true if valid endpoint uri pattern, otherwise false
 */
bool endpointUriIsValid(const char *deviceEndpointUri, const char *deviceUuid, const char *endpointId);

/**
 * Set endpoint profile version
 * @param endpoints
 * @param profileVersion
 * @return true if profile version is set, otherwise false
 */
bool endpointsSetProfileVersion(icLinkedList *endpoints, uint8_t profileVersion);
