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
// Created by Thomas Lea on 4/17/25.
//

#pragma once

#include "zigbeeCluster.h"

#define RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_INVALID_VALUE 0xffff
#define RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_MAX_VALUE 0x2710

typedef struct
{
    void (*measuredValueUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, uint16_t value);
} RelativeHumidityMeasurementClusterCallbacks;

ZigbeeCluster *relativeHumidityMeasurementClusterCreate(const RelativeHumidityMeasurementClusterCallbacks *callbacks,
                                                        void *callbackContext);

bool relativeHumidityMeasurementClusterGetMeasuredValue(uint64_t eui64, uint8_t endpointId, uint16_t *value);

/**
 * Helper to check for valid value.
 *
 * @param value relative humidity value to check
 * @return true if value is valid
 */

static inline bool relativeHumidityMeasurementClusterIsValueValid(uint16_t value)
{
    return (value != RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_INVALID_VALUE &&
            value <= RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_MAX_VALUE);
}
