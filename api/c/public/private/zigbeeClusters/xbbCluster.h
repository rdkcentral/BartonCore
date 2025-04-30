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

/*
 * Created by Thomas Lea on 4/2/21.
 */

#pragma once

#include "zigbeeCluster.h"

#define XBB_CLUSTER_STATUS_LID_OPEN_BIT               0x00
#define XBB_CLUSTER_STATUS_AC_POWER_LOSS_BIT          0x01
#define XBB_CLUSTER_STATUS_BATTERY_BANK_1_PRESENT_BIT 0x02
#define XBB_CLUSTER_STATUS_BATTERY_BANK_2_PRESENT_BIT 0x03
#define XBB_CLUSTER_STATUS_BATTERY_BANK_3_PRESENT_BIT 0x04
#define XBB_CLUSTER_STATUS_BATTERY_BANK_1_LOW_BIT     0x05
#define XBB_CLUSTER_STATUS_BATTERY_BANK_2_LOW_BIT     0x06
#define XBB_CLUSTER_STATUS_BATTERY_BANK_3_LOW_BIT     0x07
#define XBB_CLUSTER_STATUS_TEMP_LOW_BIT               0x08
#define XBB_CLUSTER_STATUS_TEMP_HIGH_BIT              0x09

typedef enum
{
    xbbBatteryTypeUnknown,
    xbbBatteryTypeXbb1_or_24,
    xbbBatteryTypeXbbl,
} XbbBatteryType;

typedef struct
{
    void (*xbbStatusChanged)(void *ctx, uint64_t eui64, uint8_t endpointId, uint16_t status);
} XbbClusterCallbacks;

/**
 * Create an instance of the XBB cluster
 *
 * @param callbacks
 * @param callbackContext
 * @return
 */
ZigbeeCluster *xbbClusterCreate(const XbbClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set the battery type to be used by this cluster instance
 *
 * @param cluster
 * @param batteryType
 */
void xbbClusterSetBatteryType(const ZigbeeCluster *cluster, XbbBatteryType batteryType);

/**
 * Read the status attribute.
 *
 * @param cluster
 * @param eui64
 * @param endpointId
 * @param status will be populated with the current status
 * @return true on succcess
 */
bool xbbClusterGetStatus(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint16_t *status);
