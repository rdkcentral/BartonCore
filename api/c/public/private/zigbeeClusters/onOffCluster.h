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
// Created by tlea on 2/13/19.
//

#ifndef ZILKER_ONOFFCLUSTER_H
#define ZILKER_ONOFFCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*onOffStateChanged)(uint64_t eui64, uint8_t endpointId, bool isOn, const void *ctx);
} OnOffClusterCallbacks;


ZigbeeCluster *onOffClusterCreate(const OnOffClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set whether or not to set a binding on the on off cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void onOffClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);
bool onOffClusterSetAttributeReporting(uint64_t eui64, uint8_t endpointId);
bool onOffClusterIsOn(uint64_t eui64, uint8_t endpointId, bool *isOn);
bool onOffClusterSetOn(uint64_t eui64, uint8_t endpointId, bool isOn);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_ONOFFCLUSTER_H
