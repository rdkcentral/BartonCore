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

/*-----------------------------------------------
 * philipsHue.h
 *
 * Philips Hue Library
 *
 * Author: tlea
 *-----------------------------------------------*/

#ifndef PHILIPS_HUE_H
#define PHILIPS_HUE_H

#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>
#include <stdbool.h>

typedef struct
{
    char *id;
    bool isOn;
} PhilipsHueLight;

typedef void (*PhilipsHueBridgeDiscoverCallback)(const char *macAddress, const char *ipAddress, const char *username);

/*
 * Start discovering Philips Hue Bridges on the local network.
 */
bool philipsHueStartDiscoveringBridges(PhilipsHueBridgeDiscoverCallback callback);

/*
 * Stop discovering Philips Hue Bridges on the local network.
 */
bool philipsHueStopDiscoveringBridges();

/*
 * Retrieve the list and state of all lights connected to the bridge
 */
icLinkedList *philipsHueGetLights(const char *ipAddress, const char *username);

/*
 * Turn a light on or off
 */
bool philipsHueSetLight(const char *ipAddress, const char *username, const char *lightId, bool on);

typedef void (*PhilipsHueLightChangedCallback)(const char *macAddress, const char *lightId, bool isOn);
typedef void (*PhilipsHueIpAddressChangedCallback)(const char *macAddress, char *newIpAddress);

/*
 * Start monitoring a bridge for changes and problems
 */
bool philipsHueStartMonitoring(const char *macAddress,
                               const char *ipAddress,
                               const char *username,
                               PhilipsHueLightChangedCallback lightChangedCallback,
                               PhilipsHueIpAddressChangedCallback ipAddressChangedCallback);

/*
 * Stop monitoring a bridge for changes and problems
 */
bool philipsHueStopMonitoring(const char *ipAddress);

/*
 * Release the resources used by the provided light
 */
void philipsHueLightDestroy(PhilipsHueLight *light);

#endif // PHILIPS_HUE_H
