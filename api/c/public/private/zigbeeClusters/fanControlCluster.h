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
// Created by tlea on 3/1/19
//

#ifndef ZILKER_FANCONTROLCLUSTER_H
#define ZILKER_FANCONTROLCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*fanModeChanged)(uint64_t eui64, uint8_t endpointId, uint8_t mode, const void *ctx);
} FanControlClusterCallbacks;


ZigbeeCluster *fanControlClusterCreate(const FanControlClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void fanControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

bool fanControlClusterGetFanMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t *mode);

bool fanControlClusterSetFanMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t mode);

const char *fanControlClusterGetFanModeString(uint8_t fanMode);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_FANCONTROLCLUSTER_H
