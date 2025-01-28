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
// Created by tlea on 4/30/19.
//

/**
 * To save battery, we have extended some cluster commands (on IAS Zone and Poll Control) with additional
 * data to prevent the need to query the device after receiving some commands.
 */

#pragma once

#include <inttypes.h>

#define OPTIONAL_SENSOR_DATA_TYPE_METADATA         "comcast.battSave.extra"
#define OPTIONAL_SENSOR_DATA_TYPE_MAGNETIC         "magStr"
#define OPTIONAL_SENSOR_DATA_TYPE_BATTERY_CAP_USED "battCapUsed"

typedef union
{
    uint16_t data;
    uint16_t battUsedMilliAmpHr;
    uint16_t magStrMicroTesla;
} OptionalSensorData;

typedef struct
{
    uint16_t battVoltage;
    OptionalSensorData optionalData;
    int16_t temp;
    int8_t rssi;
    uint8_t lqi;
    uint32_t retries;
    uint32_t rejoins;
} ComcastBatterySavingData;
