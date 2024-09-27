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
 * An endpoint is a logical device encapsulated in the physical icDevice.  Examples of endpoints
 * on an open home camera would be 'camera' and 'sensor'.
 *
 * Created by Thomas Lea on 8/11/15.
 */

#pragma once
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>
#include <cjson/cJSON.h>
#include <serial/icSerDesContext.h>
#include <icUtil/stringUtils.h>
#include <glib-object.h>

typedef struct
{
    char            *id;
    char            *uri;
    char            *profile;
    uint8_t         profileVersion;
    char            *deviceUuid;
    bool            enabled;
    icLinkedList    *resources;
    icLinkedList    *metadata;
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
cJSON *endpointsToJSON(icLinkedList *endpoints, const icSerDesContext *context );

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
icLinkedList *endpointsFromJSON(const char *deviceUUID, cJSON *endpointsJSON, const icSerDesContext *context, bool permissive);

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
bool endpointUriIsValid(const char* deviceEndpointUri, const char* deviceUuid, const char *endpointId);

/**
 * Set endpoint profile version
 * @param endpoints
 * @param profileVersion
 * @return true if profile version is set, otherwise false
 */
bool endpointsSetProfileVersion(icLinkedList *endpoints, uint8_t profileVersion);

