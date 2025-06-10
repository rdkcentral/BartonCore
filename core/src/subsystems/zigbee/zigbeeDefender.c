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
// Created by tlea on 8/5/19.
//

#include "zigbeeDefender.h"
#include "devicePrivateProperties.h"
#include "deviceServiceConfiguration.h"
#include "provider/barton-core-property-provider.h"
#include <event/deviceEventProducer.h>
#include <icLog/logging.h>
#include <pthread.h>
#include <stdint.h>
#include <zhal/zhal.h>

#define LOG_TAG                                       "zigbeeDefender"

#define DEFENDER_PAN_ID_CHANGE_THRESHOLD_DEFAULT      0
#define DEFENDER_PAN_ID_CHANGE_WINDOW_MILLIS_DEFAULT  1000
#define DEFENDER_PAN_ID_CHANGE_RESTORE_MILLIS_DEFAULT 1000

static pthread_mutex_t problemMtx = PTHREAD_MUTEX_INITIALIZER;
static bool panIdAttackDetected = false;

void zigbeeDefenderConfigure()
{
    uint8_t panIdChangeThreshold;
    uint32_t panIdChangeWindowMillis;
    uint32_t panIdChangeRestoreMillis;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    panIdChangeThreshold = b_core_property_provider_get_property_as_uint32(
        propertyProvider, ZIGBEE_DEFENDER_PAN_ID_CHANGE_THRESHOLD_OPTION, DEFENDER_PAN_ID_CHANGE_THRESHOLD_DEFAULT);

    panIdChangeWindowMillis =
        b_core_property_provider_get_property_as_uint32(propertyProvider,
                                                                  ZIGBEE_DEFENDER_PAN_ID_CHANGE_WINDOW_MILLIS_OPTION,
                                                                  DEFENDER_PAN_ID_CHANGE_WINDOW_MILLIS_DEFAULT);

    panIdChangeRestoreMillis =
        b_core_property_provider_get_property_as_uint32(propertyProvider,
                                                                  ZIGBEE_DEFENDER_PAN_ID_CHANGE_RESTORE_MILLIS_OPTION,
                                                                  DEFENDER_PAN_ID_CHANGE_RESTORE_MILLIS_DEFAULT);

    if (zhalDefenderConfigure(panIdChangeThreshold, panIdChangeWindowMillis, panIdChangeRestoreMillis) != true)
    {
        icLogError(LOG_TAG, "%s: failed to configure defender", __FUNCTION__);
    }

    if (panIdChangeThreshold == 0)
    {
        icLogDebug(LOG_TAG, "%s: not monitoring, feature disabled", __FUNCTION__);

        // if there was a problem before, we need to send a clear event since we are stopping monitoring
        pthread_mutex_lock(&problemMtx);
        if (panIdAttackDetected == true)
        {
            panIdAttackDetected = false;
            sendZigbeePanIdAttackEvent(false);
        }
        pthread_mutex_unlock(&problemMtx);
    }
}
void zigbeeDefenderSetPanIdAttack(bool attackDetected)
{
    icLogDebug(LOG_TAG, "%s: attackDetected = %s", __FUNCTION__, attackDetected ? "true" : "false");

    pthread_mutex_lock(&problemMtx);
    panIdAttackDetected = attackDetected;
    pthread_mutex_unlock(&problemMtx);

    sendZigbeePanIdAttackEvent(attackDetected);
}
