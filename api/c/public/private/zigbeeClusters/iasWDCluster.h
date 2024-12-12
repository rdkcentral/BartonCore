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
// Created by mkoch201 on 3/25/19.
//

#ifndef ZILKER_IASWDCLUSTER_H
#define ZILKER_IASWDCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
} IASWDClusterCallbacks;

typedef enum
{
    IASWD_WARNING_MODE_STOP = 0,
    IASWD_WARNING_MODE_BURGLAR,
    IASWD_WARNING_MODE_FIRE,
    IASWD_WARNING_MODE_EMERGENCY,
    IASWD_WARNING_MODE_POLICE_PANIC,
    IASWD_WARNING_MODE_FIRE_PANIC,
    IASWD_WARNING_MODE_EMERGENCY_PANIC,
    IASWD_WARNING_MODE_CO
} IASWDWarningMode;

typedef enum
{
    IASWD_SIREN_LEVEL_LOW = 0,
    IASWD_SIREN_LEVEL_MEDIUM,
    IASWD_SIREN_LEVEL_HIGH,
    IASWD_SIREN_LEVEL_MAXIMUM
} IASWDSirenLevel;

typedef enum
{
    IASWD_STROBE_LEVEL_LOW = 0,
    IASWD_STROBE_LEVEL_MEDIUM,
    IASWD_STROBE_LEVEL_HIGH,
    IASWD_STROBE_LEVEL_MAXIMUM
} IASWDStrobeLevel;

ZigbeeCluster *iasWDClusterCreate(const IASWDClusterCallbacks *callbacks, void *callbackContext);

bool iasWDClusterStartWarning(uint64_t eui64,
                              uint8_t endpointId,
                              IASWDWarningMode warningMode,
                              IASWDSirenLevel sirenLevel,
                              bool enableStrobe,
                              uint16_t warningDuration,
                              uint8_t strobeDutyCycle,
                              IASWDStrobeLevel strobeLevel);


#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_IASWDCLUSTER_H
