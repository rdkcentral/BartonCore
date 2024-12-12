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
// Created by tlea on 2/15/19.
//

#ifndef ZILKER_ALARMSCLUSTER_H
#define ZILKER_ALARMSCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"


typedef struct
{
    void (*alarmReceived)(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx);

    void (*alarmCleared)(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx);
} AlarmsClusterCallbacks;

ZigbeeCluster *alarmsClusterCreate(const AlarmsClusterCallbacks *callbacks, const void *callbackContext);

/**
 * Retrieve the current entries in the alarm table.  Per spec, this will also clear the alarm table.
 *
 * @param cluster
 * @param eui64
 * @param endpointId
 * @return  a linked list of ZigbeeAlarmTableEntrys or NULL on failure
 */
icLinkedList *alarmsClusterGetAlarms(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void alarmsClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_ALARMSCLUSTER_H
