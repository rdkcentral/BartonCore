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
// Created by tlea on 3/18/19
//

#include "resourceContainer.h"
#include <deviceService/zoneChanged.h>
#include <errno.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <jsonHelper/jsonHelper.h>
#include <memory.h>

#define LOG_TAG "ZoneChanged"

/* Public functions */

extern inline void zoneChangedDestroy__auto(ZoneChanged **event);

ZoneChanged *zoneChangedCreate(uint8_t displayIndex,
                               const char *label,
                               bool faulted,
                               bool bypassed,
                               bool bypassActive,
                               SecurityIndication indication,
                               uint64_t eventId,
                               ZoneChangedReason reason)
{
    if (indication == SECURITY_INDICATION_INVALID || label == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return NULL;
    }

    ZoneChanged *retVal = (ZoneChanged *) calloc(1, sizeof(ZoneChanged));
    retVal->displayIndex = displayIndex;
    retVal->label = strdup(label);
    retVal->faulted = faulted;
    retVal->bypassed = bypassed;
    retVal->bypassActive = bypassActive;
    retVal->indication = indication;
    retVal->eventId = eventId;
    retVal->reason = reason;

    return retVal;
}

ZoneChanged *zoneChangedClone(const ZoneChanged *event)
{
    ZoneChanged *retVal = NULL;
    if (event != NULL)
    {
        retVal = malloc(sizeof(ZoneChanged));
        if (zoneChangedCopy(retVal, event) != 0)
        {
            free(retVal);
            retVal = NULL;
        }
    }

    return retVal;
}

int zoneChangedCopy(ZoneChanged *dst, const ZoneChanged *src)
{
    int err = 0;
    if (src && dst)
    {
        memcpy(dst, src, sizeof(ZoneChanged));
        dst->label = strdup(src->label);
    }
    else
    {
        err = EINVAL;
    }

    return err;
}

void zoneChangedDestroy(ZoneChanged *zoneChanged)
{
    if (zoneChanged != NULL)
    {
        free(zoneChanged->label);
        zoneChanged->label = NULL;
    }
}

char *zoneChangedToJSON(ZoneChanged *zoneChanged)
{
    AUTO_CLEAN(cJSON_Delete__auto) cJSON *json = cJSON_CreateObject();

    if (!cJSON_AddNumberToObject(json, ZONE_CHANGED_DISPLAY_INDEX, zoneChanged->displayIndex))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_DISPLAY_INDEX);
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, ZONE_CHANGED_LABEL, zoneChanged->label))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_LABEL);
        return NULL;
    }

    if (!cJSON_AddBoolToObject(json, ZONE_CHANGED_FAULTED, zoneChanged->faulted))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_FAULTED);
        return NULL;
    }

    if (!cJSON_AddBoolToObject(json, ZONE_CHANGED_BYPASSED, zoneChanged->bypassed))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_BYPASSED);
        return NULL;
    }

    if (!cJSON_AddBoolToObject(json, ZONE_CHANGED_BYPASS_ACTIVE, zoneChanged->bypassActive))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_BYPASS_ACTIVE);
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, ZONE_CHANGED_INDICATION, SecurityIndicationLabels[zoneChanged->indication]))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_INDICATION);
        return NULL;
    }

    if (!cJSON_AddNumberToObject(json, ZONE_CHANGED_EVENT_ID, zoneChanged->eventId))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_EVENT_ID);
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, ZONE_CHANGED_REASON, ZoneChangedReasonLabels[zoneChanged->reason]))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, ZONE_CHANGED_REASON);
        return NULL;
    }

    return cJSON_PrintUnformatted(json);
}

ZoneChanged *zoneChangedFromJSON(const char *json)
{
    ZoneChanged *zoneChanged = NULL;

    if (json)
    {
        AUTO_CLEAN(cJSON_Delete__auto) cJSON *parsed = cJSON_Parse(json);
        AUTO_CLEAN(free_generic__auto) const char *indicationLabel = getCJSONString(parsed, ZONE_CHANGED_INDICATION);
        AUTO_CLEAN(free_generic__auto) const char *label = getCJSONString(parsed, ZONE_CHANGED_LABEL);

        int displayIndex = 0;
        double eventId = 0;
        ZoneChangedReason reason = ZONE_CHANGED_REASON_INVALID;
        bool faulted = false;
        bool bypassed = false;
        bool bypassActive = false;
        bool ok = true;

        if (!label)
        {
            icLogError(LOG_TAG, "%s: %s failed to parse", __FUNCTION__, ZONE_CHANGED_LABEL);

            ok = false;
        }

        if (!indicationLabel)
        {
            icLogError(LOG_TAG, "%s: %s failed to parse", __FUNCTION__, ZONE_CHANGED_INDICATION);
            ok = false;
        }

        if (!getCJSONInt(parsed, ZONE_CHANGED_DISPLAY_INDEX, &displayIndex))
        {
            icLogError(LOG_TAG, "%s: %s failed to parse", __FUNCTION__, ZONE_CHANGED_DISPLAY_INDEX);
            ok = false;
        }

        if (!getCJSONBool(parsed, ZONE_CHANGED_FAULTED, &faulted))
        {
            icLogError(LOG_TAG, "%s: %s failed to parse", __FUNCTION__, ZONE_CHANGED_FAULTED);
            ok = false;
        }

        if (!getCJSONBool(parsed, ZONE_CHANGED_BYPASSED, &bypassed))
        {
            icLogError(LOG_TAG, "%s: %s failed to parse", __FUNCTION__, ZONE_CHANGED_BYPASSED);
            ok = false;
        }

        if (!getCJSONBool(parsed, ZONE_CHANGED_BYPASS_ACTIVE, &bypassActive))
        {
            icLogError(LOG_TAG, "%s: %s failed to parse", __FUNCTION__, ZONE_CHANGED_BYPASS_ACTIVE);
            ok = false;
        }

        if (!getCJSONDouble(parsed, ZONE_CHANGED_EVENT_ID, &eventId))
        {
            icLogError(LOG_TAG, "%s: %s failed to parse", __FUNCTION__, ZONE_CHANGED_EVENT_ID);
            ok = false;
        }

        AUTO_CLEAN(free_generic__auto) char *tmp = getCJSONString(parsed, ZONE_CHANGED_REASON);
        if (tmp != NULL)
        {
            reason = (ZoneChangedReason) findEnumForLabel(
                tmp, ZoneChangedReasonLabels, ARRAY_LENGTH(ZoneChangedReasonLabels));
        }
        else
        {
            icLogError(LOG_TAG, "%s: %s is NULL", __FUNCTION__, ZONE_CHANGED_REASON);
        }

        if (ok)
        {
            zoneChanged = zoneChangedCreate((uint8_t) displayIndex,
                                            label,
                                            faulted,
                                            bypassed,
                                            bypassActive,
                                            securityIndicationValueOf(indicationLabel),
                                            (uint64_t) eventId,
                                            reason);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: JSON input is NULL", __FUNCTION__);
    }

    return zoneChanged;
}
