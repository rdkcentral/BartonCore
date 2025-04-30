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
// Created by tlea on 2/19/19.
//

#pragma once


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*levelChanged)(uint64_t eui64, uint8_t endpointId, uint8_t level, const void *ctx);
} LevelControlClusterCallbacks;


ZigbeeCluster *levelControlClusterCreate(const LevelControlClusterCallbacks *callbacks, void *callbackContext);

bool levelControlClusterGetLevel(uint64_t eui64, uint8_t endpointId, uint8_t *level);

bool levelControlClusterSetLevel(uint64_t eui64, uint8_t endpointId, uint8_t level);

void levelControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

bool levelControlClusterSetAttributeReporting(uint64_t eui64, uint8_t endpointId);

// caller frees
char *levelControlClusterGetLevelString(uint8_t level);

uint8_t levelControlClusterGetLevelFromString(const char *level);

#endif // BARTON_CONFIG_ZIGBEE
