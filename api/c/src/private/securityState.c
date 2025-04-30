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

#include "resourceContainer.h"
#include <deviceService/securityState.h>
#include <errno.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <jsonHelper/jsonHelper.h>
#include <memory.h>

#define LOG_TAG "SecurityState"

/* Public functions */

extern inline void securityStateDestroy__auto(SecurityState **event);

SecurityState *securityStateCreate(const PanelStatus panelStatus,
                                   const uint32_t timeLeft,
                                   const SecurityIndication indication,
                                   const bool bypassActive)
{
    if (indication == SECURITY_INDICATION_INVALID)
    {
        icLogWarn(LOG_TAG, "Creating state with invalid indication");
    }

    if (panelStatus == PANEL_STATUS_INVALID)
    {
        icLogWarn(LOG_TAG, "Creating state with invalid panel status");
    }

    SecurityState event = {.indication = indication,
                           .panelStatus = panelStatus,
                           .timeLeft = (uint8_t) timeLeft,
                           .bypassActive = bypassActive};

    SecurityState *retVal = malloc(sizeof(event));
    memcpy(retVal, &event, sizeof(event));

    return retVal;
}

SecurityState *securityStateClone(const SecurityState *event)
{
    SecurityState *retVal = NULL;
    if (event != NULL)
    {
        retVal = malloc(sizeof(SecurityState));
        if (securityStateCopy(retVal, event) != 0)
        {
            free(retVal);
            retVal = NULL;
        }
    }

    return retVal;
}

int securityStateCopy(SecurityState *dst, const SecurityState *src)
{
    int err = 0;
    if (src && dst)
    {
        memcpy(dst, src, sizeof(SecurityState));
    }
    else
    {
        err = EINVAL;
    }

    return err;
}

void securityStateDestroy(SecurityState *state)
{
    /* noop for now */
}

const char *securityStateToJSON(SecurityState *state)
{
    AUTO_CLEAN(cJSON_Delete__auto) cJSON *json = cJSON_CreateObject();

    if (!cJSON_AddStringToObject(json, SECURITY_STATE_PANEL_STATUS, PanelStatusLabels[state->panelStatus]))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, SECURITY_STATE_PANEL_STATUS);
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, SECURITY_STATE_INDICATION, SecurityIndicationLabels[state->indication]))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, SECURITY_STATE_INDICATION);
        return NULL;
    }

    if (!cJSON_AddNumberToObject(json, SECURITY_STATE_TIME_LEFT, state->timeLeft))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, SECURITY_STATE_TIME_LEFT);
        return NULL;
    }

    if (!cJSON_AddBoolToObject(json, SECURITY_STATE_BYPASS_ACTIVE, state->bypassActive))
    {
        icLogError(LOG_TAG, "%s: failed to add %s", __FUNCTION__, SECURITY_STATE_BYPASS_ACTIVE);
        return NULL;
    }

    return cJSON_PrintUnformatted(json);
}

SecurityState *securityStateFromJSON(const char *json)
{
    SecurityState *state = NULL;

    if (json)
    {
        AUTO_CLEAN(cJSON_Delete__auto) cJSON *parsed = cJSON_Parse(json);
        AUTO_CLEAN(free_generic__auto) const char *indicationLabel = getCJSONString(parsed, SECURITY_STATE_INDICATION);
        AUTO_CLEAN(free_generic__auto)
        const char *panelStatusLabel = getCJSONString(parsed, SECURITY_STATE_PANEL_STATUS);
        uint32_t timeLeft = 0;
        bool bypassActive = false;
        bool ok = true;

        if (panelStatusLabel == NULL)
        {
            icLogError(LOG_TAG, "%s: %s is NULL", __FUNCTION__, SECURITY_STATE_PANEL_STATUS);
            ok = false;
        }

        if (indicationLabel == NULL)
        {
            icLogError(LOG_TAG, "%s: %s is NULL", __FUNCTION__, SECURITY_STATE_INDICATION);
            ok = false;
        }

        if (!getCJSONInt(parsed, SECURITY_STATE_TIME_LEFT, (int *) &timeLeft))
        {
            icLogError(LOG_TAG, "%s: %s is not a number", __FUNCTION__, SECURITY_STATE_TIME_LEFT);
            ok = false;
        }

        if (!getCJSONBool(parsed, SECURITY_STATE_BYPASS_ACTIVE, &bypassActive))
        {
            icLogError(LOG_TAG, "%s: %s is not a bool", __FUNCTION__, SECURITY_STATE_BYPASS_ACTIVE);
            ok = false;
        }

        if (ok)
        {
            state = securityStateCreate(panelStatusValueOf(panelStatusLabel),
                                        timeLeft,
                                        securityIndicationValueOf(indicationLabel),
                                        bypassActive);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: JSON input is NULL", __FUNCTION__);
    }

    return state;
}

SecurityIndication securityIndicationValueOf(const char *indicationLabel)
{
    return (SecurityIndication) findEnumForLabel(
        indicationLabel, SecurityIndicationLabels, ARRAY_LENGTH(SecurityIndicationLabels));
}

PanelStatus panelStatusValueOf(const char *panelStatusLabel)
{
    return (PanelStatus) findEnumForLabel(panelStatusLabel, PanelStatusLabels, ARRAY_LENGTH(PanelStatusLabels));
}
