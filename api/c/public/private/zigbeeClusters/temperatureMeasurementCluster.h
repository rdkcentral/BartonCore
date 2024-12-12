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
// Created by tlea on 2/18/19.
//

#ifndef ZILKER_TEMPERATUREMEASUREMENTCLUSTER_H
#define ZILKER_TEMPERATUREMEASUREMENTCLUSTER_H

#include "zigbeeCluster.h"

#define TEMPERATURE_MEASUREMENT_CLUSTER_INVALID_TEMPERATURE_VALUE 0x8000

typedef struct
{
    void (*measuredValueUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, int16_t value);
} TemperatureMeasurementClusterCallbacks;

ZigbeeCluster *temperatureMeasurementClusterCreate(const TemperatureMeasurementClusterCallbacks *callbacks,
                                                   void *callbackContext);

bool temperatureMeasurementClusterGetMeasuredValue(uint64_t eui64, uint8_t endpointId, int16_t *value);

/**
 * Set whether or not to configure temperature reporting.  By default temperature reporting is not configured
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void temperatureMeasurementSetTemperatureReporting(const DeviceConfigurationContext *deviceConfigurationContext,
                                                   bool configure);

/**
 * Helper to check for valid temperature value.
 *
 * @param value temperature value to check
 * @return true if value is valid
 */

inline bool temperatureMeasurementClusterIsTemperatureValid(uint16_t value)
{
    return (value != TEMPERATURE_MEASUREMENT_CLUSTER_INVALID_TEMPERATURE_VALUE);
}

#endif // ZILKER_TEMPERATUREMEASUREMENTCLUSTER_H
