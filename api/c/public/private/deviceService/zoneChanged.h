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

#pragma once

#include "securityState.h"
#include <icUtil/array.h>
#include <stdint.h>

#define ZONE_CHANGED_DISPLAY_INDEX "displayIndex"
#define ZONE_CHANGED_LABEL         "label"
#define ZONE_CHANGED_FAULTED       "faulted"
#define ZONE_CHANGED_BYPASSED      "bypassed"
#define ZONE_CHANGED_BYPASS_ACTIVE "bypassActive"
#define ZONE_CHANGED_INDICATION    "indication"
#define ZONE_CHANGED_EVENT_ID      "eventId"
#define ZONE_CHANGED_REASON        "reason"

#undef ENUM_LABEL
#define ENUM_LABEL(x) x
#define ENUM_VALUES                                                                                                    \
    ENUM_LABEL(ZONE_CHANGED_REASON_INVALID), ENUM_LABEL(ZONE_CHANGED_REASON_CRUD),                                     \
        ENUM_LABEL(ZONE_CHANGED_REASON_FAULT_CHANGED), ENUM_LABEL(ZONE_CHANGED_REASON_BYPASS_CHANGED),                 \
        ENUM_LABEL(ZONE_CHANGED_REASON_REORDER)
// END ENUM_VALUES

typedef enum
{
    ENUM_VALUES
} ZoneChangedReason;

#undef ENUM_LABEL
#define ENUM_LABEL(x) #x

static const char *ZoneChangedReasonLabels[] = {ENUM_VALUES};

#undef ENUM_VALUES
#undef ENUM_LABEL

typedef struct
{
    uint8_t displayIndex;
    char *label;
    SecurityIndication indication;
    bool faulted;
    bool bypassed;
    bool bypassActive;
    uint64_t eventId;
    ZoneChangedReason reason;
} ZoneChanged;

static const ZoneChanged zoneChangedEmpty;

/**
 * Create an immutable zone changed
 * @return a heap allocated event pointer
 * @see zoneChangedDestroy
 */
ZoneChanged *zoneChangedCreate(uint8_t displayIndex,
                               const char *label,
                               bool faulted,
                               bool bypassed,
                               bool bypassActive,
                               SecurityIndication indication,
                               uint64_t eventId,
                               ZoneChangedReason reason);

/**
 * Clone a zone changed
 * @param event
 * @return A heap allocated event pointer
 * @see zoneChangedDestroy
 */
ZoneChanged *zoneChangedClone(const ZoneChanged *event);

/**
 * Safely copy one zone changed to another
 * @param dst
 * @param src
 * @return EINVAL when either or both arguments are NULL, otherwise 0
 */
int zoneChangedCopy(ZoneChanged *dst, const ZoneChanged *src);

/**
 * Safely discard a zone changed. It does NOT free the state pointer itself
 */
void zoneChangedDestroy(ZoneChanged *zoneChanged);

/**
 * For use with auto cleanup. This WILL free the state pointer.
 */
inline void zoneChangedDestroy__auto(ZoneChanged **zoneChanged)
{
    zoneChangedDestroy(*zoneChanged);
    free(*zoneChanged);
}

#define scoped_zoneChanged AUTO_CLEAN(zoneChangedDestroy__auto) zoneChanged

/**
 * Serialize a zone changed to JSON. The result must be freed.
 */
char *zoneChangedToJSON(ZoneChanged *zoneChanged);

/**
 * Deserialize a JSON zone changed
 * @note the caller must free the result pointer
 * @see zoneChangedDestroy
 * @return NULL when parsing fails
 */
ZoneChanged *zoneChangedFromJSON(const char *json);
