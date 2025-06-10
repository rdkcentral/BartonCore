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
// Created by Kevin Funderburg on 08/15/2024
//

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_TYPE b_core_zigbee_energy_scan_result_get_type()
G_DECLARE_FINAL_TYPE(BCoreZigbeeEnergyScanResult,
                     b_core_zigbee_energy_scan_result,
                     B_CORE,
                     ZIGBEE_ENERGY_SCAN_RESULT,
                     GObject)

typedef enum
{
    B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL = 1,
    B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX,
    B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN,
    B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG,
    B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE,

    N_B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTIES
} BCoreZigbeeEnergyScanResultProperty;

static const char *B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES[] =
    {NULL, "channel", "max-rssi", "min-rssi", "avg-rssi", "score"};

/**
 * b_core_zigbee_energy_scan_result_new
 *
 * Returns: (transfer full): BCoreZigbeeEnergyScanResult*
 */
BCoreZigbeeEnergyScanResult *b_core_zigbee_energy_scan_result_new(void);

G_END_DECLS
