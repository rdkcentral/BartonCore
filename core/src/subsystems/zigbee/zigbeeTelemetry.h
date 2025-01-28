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
// Created by tlea on 9/11/19.
//

#pragma once

#ifdef BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY

/**
 * Initialize the Zigbee Telemetry system, potentially starting network captures.
 */
void zigbeeTelemetryInitialize(void);

/**
 * Shut down the Zigbee Telemetry system, stopping any running captures and releasing resources.
 */
void zigbeeTelemetryShutdown(void);

/**
 * Update a Zigbee telemetry related property which could start, stop, or change a running capture's configuration.
 * @param key
 * @param value
 */
void zigbeeTelemetrySetProperty(const char *key, const char *value);

#endif // BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY
