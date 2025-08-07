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

#define DEVICE_PROP_MATTER_NS                      "device.matter."

// TODO: generate this from propertyDefinitions

/**
 * @brief Base64 encoded identity protection key
 * @ref Matter 1.0 4.13.2.6
 * @note this data is sensitive
 */
#define DEVICE_PROP_MATTER_CURRENT_IPK             DEVICE_PROP_MATTER_NS "currentIpk"

/**
 * @brief A string enumeration describing the operational envrionment of the device service matter stack
 * @deprecated - Use B_CORE_BARTON_MATTER_OPERATIONAL_HOST instead.
 */
#define DEVICE_PROP_MATTER_OPERATIONAL_ENVIRONMENT DEVICE_PROP_MATTER_NS "environment"

#define DEVICE_PROP_MATTER_DAC_P12_PATH         DEVICE_PROP_MATTER_NS "dacP12Path"
#define DEVICE_PROP_MATTER_DAC_P12_PASSWORD     DEVICE_PROP_MATTER_NS "dacP12Password"

// WifiNetworkDiagnosticsCluster

/**
 * @brief uint16 min rate limiter for Wifi RSSI subscriptions
 * @ref Matter 1.0 11.14
 */
#define DEVICE_PROP_MATTER_WIFI_DIAGNOSTICS_RSSI_MIN_INTERVAL_SECS                                                     \
    "device.matter.wifiDiagnostics.rssi.minIntervalSeconds"

/**
 * @brief uint16 max rate limiter for Wifi RSSI subscriptions
 * @ref Matter 1.0 11.14
 */
#define DEVICE_PROP_MATTER_WIFI_DIAGNOSTICS_RSSI_MAX_INTERVAL_SECS                                                     \
    "device.matter.wifiDiagnostics.rssi.maxIntervalSeconds"
