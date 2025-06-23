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
 * Created by Munilakshmi Velampati on 29/01/25.
 */

#include "utils.h"
#include "barton-core-reference-io.h"
#include <icUtil/stringUtils.h>
#include <private/deviceService/resourceModes.h>

#define SENSITIVE_RESOURCE_VALUE_STRING "(encrypted)"

const gchar *stringifyMode(guint mode)
{
    gchar buf[8];

    buf[0] = (mode & RESOURCE_MODE_READABLE) ? ('r') : ('-');
    buf[1] = (mode & RESOURCE_MODE_WRITEABLE) ? ('w') : ('-');
    buf[2] = (mode & RESOURCE_MODE_EXECUTABLE) ? ('x') : ('-');
    buf[3] = (mode & RESOURCE_MODE_DYNAMIC) || (mode & RESOURCE_MODE_DYNAMIC_CAPABLE) ? ('d') : ('-');
    buf[4] = (mode & RESOURCE_MODE_EMIT_EVENTS) ? ('e') : ('-');
    buf[5] = (mode & RESOURCE_MODE_LAZY_SAVE_NEXT) ? ('l') : ('-');
    buf[6] = (mode & RESOURCE_MODE_SENSITIVE) ? ('s') : ('-');
    buf[7] = '\0';

    return g_strdup(buf);
}

gchar *getResourceValue(guint resourceMode, gchar *value)
{
    return (resourceMode & RESOURCE_MODE_SENSITIVE) ? (SENSITIVE_RESOURCE_VALUE_STRING) : (value);
}

void printDeviceFoundDetails(const char *printPrefix, BCoreDeviceFoundDetails *deviceFoundDetails)
{
    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *manufacturer = NULL;
    g_autofree gchar *model = NULL;
    g_autofree gchar *hardwareVersion = NULL;
    g_autofree gchar *firmwareVersion = NULL;
    g_object_get(
        G_OBJECT(deviceFoundDetails),
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID],
        &deviceId,
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        &manufacturer,
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        &model,
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_CORE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        &hardwareVersion,
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_CORE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        &firmwareVersion,
        NULL);

    emitOutput("%s uuid=%s, manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s\n",
               printPrefix,
               deviceId,
               manufacturer,
               model,
               hardwareVersion,
               firmwareVersion);
}

gchar *getResourceDump(BCoreResource *resource)
{
    guint resourceMode;
    g_autofree gchar *id = NULL;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *value = NULL;
    g_autofree gchar *ownerId = NULL;
    g_autofree gchar *type = NULL;

    g_object_get(resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_MODE],
                 &resourceMode,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID],
                 &id,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_URI],
                 &uri,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_VALUE],
                 &value,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DEVICE_UUID],
                 &ownerId,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_TYPE],
                 &type,
                 NULL);

    g_autofree const gchar *modeStr = stringifyMode(resourceMode);

    return stringBuilder("id=%s, uri=%s, ownerId=%s, value=%s, type=%s, mode=0x%x (%s)",
                         id,
                         uri,
                         ownerId,
                         getResourceValue(resourceMode, value),
                         type,
                         resourceMode,
                         modeStr);
}
