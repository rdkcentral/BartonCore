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
 * Created by Thomas Lea on 9/30/15.
 */
#include <deviceHelper.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READ_RESOURCE_TIMEOUT_SECONDS 60
#define LOG_TAG                       "deviceHelper"

char *createDeviceUri(const char *uuid)
{
    // URI format: /[uuid]
    char *uri = (char *) malloc(1 + strlen(uuid) + 1);
    if (uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s", uuid);

    return uri;
}

char *createEndpointUri(const char *uuid, const char *endpointId)
{
    // URI format: /[uuid]/ep/[endpoint id]
    char *uri = (char *) malloc(1 + strlen(uuid) + 4 + strlen(endpointId) + 1);
    if (uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s/ep/%s", uuid, endpointId);

    return uri;
}

char *createDeviceResourceUri(const char *uuid, const char *resourceId)
{
    // URI format: /[uuid]/r/[resource id]
    char *uri = (char *) malloc(1 + strlen(uuid) + 3 + strlen(resourceId) + 1);
    if (uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s/r/%s", uuid, resourceId);

    return uri;
}

char *createDeviceMetadataUri(const char *uuid, const char *metadataId)
{
    // URI format: /[uuid]/m/[resource id]
    char *uri = (char *) malloc(1 + strlen(uuid) + 3 + strlen(metadataId) + 1);
    if (uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s/m/%s", uuid, metadataId);

    return uri;
}

char *createEndpointResourceUri(const char *uuid, const char *endpointId, const char *resourceId)
{
    char *epUri = createEndpointUri(uuid, endpointId);

    // URI Format: [endpointURI]/r/[resourceId]
    // The compiler will optimize the constants addition
    char *uri = (char *) malloc(strlen(epUri) + 3 + strlen(resourceId) + 1);

    sprintf(uri, "%s/r/%s", epUri, resourceId);

    free(epUri);

    return uri;
}

char *createEndpointMetadataUri(const char *uuid, const char *endpointId, const char *metadataId)
{
    char *epUri = createEndpointUri(uuid, endpointId);

    // URI Format: /[endpointUri]/m/[metadataId]
    // The compiler will optimize the constants addition
    char *uri = (char *) malloc(strlen(epUri) + 3 + strlen(metadataId) + 1);

    sprintf(uri, "%s/m/%s", epUri, metadataId);

    free(epUri);

    return uri;
}

char *createResourceUri(const char *ownerUri, const char *resourceId)
{
    // URI format: ownerUri/r/[resource id]

    return stringBuilder("%s/r/%s", ownerUri, resourceId);
}

/*
 * Extract the device UUID from the given URI, caller must free result
 */
char *getDeviceUuidFromUri(const char *uri)
{
    char *retVal = NULL;
    // Format should /deviceUuid/... or just /deviceUuid
    if (uri != NULL)
    {
        size_t len = strlen(uri);
        if (len > 1)
        {
            // If its just a device URI, then it won't have an ending /, in which case we will use the len
            char *end = strchr(uri + 1, '/');
            if (end != NULL)
            {
                len = end - uri;
            }

            retVal = (char *) malloc(len + 1);
            strncpy(retVal, uri + 1, len - 1);
            // Since we were copying part of a longer string, need to NULL terminate
            retVal[len - 1] = '\0';
        }
    }
    return retVal;
}

/*
 * Extract the endpoint ID from the given URI, caller must free result
 */
char *getEndpointIdFromUri(const char *uri)
{
    char *retVal = NULL;
    // Format should /deviceUuid/ep/endpointId/... or /deviceUuid/ep/endpointId
    if (uri != NULL)
    {
        size_t len = strlen(uri);
        if (len > 1)
        {
            // Find the start of the endpoint part
            char *start = strstr(uri + 1, "/ep/");
            if (start != NULL)
            {
                start = start + 4;
                char *end = strchr(start, '/');
                if (end != NULL)
                {
                    len = end - start;
                }
                else
                {
                    len = len - (start - uri);
                }
            }

            if (start != NULL)
            {
                retVal = (char *) malloc(len + 1);
                strncpy(retVal, start, len);
                // Since we were copying part of a longer string, need to NULL terminate
                retVal[len] = '\0';
            }
        }
    }
    return retVal;
}
