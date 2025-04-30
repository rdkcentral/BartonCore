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

#pragma once

#include "zigbeeCluster.h"

typedef enum
{
    deviceTemperatureStatusNormal,
    deviceTemperatureStatusLow,
    deviceTemperatureStatusHigh,
} DeviceTemperatureStatus;

typedef struct
{
    void (*deviceTemperatureStatusChanged)(void *ctx,
                                           uint64_t eui64,
                                           uint8_t endpointId,
                                           DeviceTemperatureStatus status);
} DeviceTemperatureConfigurationClusterCallbacks;

ZigbeeCluster *
deviceTemperatureConfigurationClusterCreate(const DeviceTemperatureConfigurationClusterCallbacks *callbacks,
                                            void *callbackContext);

/**
 * Set whether or not to configure the temperature alarm mask.  By default the temperature alarm mask will not be
 * configured unless explicitly told to.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void deviceTemperatureConfigurationClusterSetConfigureTemperatureAlarmMask(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure);
