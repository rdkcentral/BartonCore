//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by tlea on 8/5/19.
//

#include "zigbeeDefender.h"
#include "devicePrivateProperties.h"
#include "deviceServiceConfiguration.h"
#include "provider/device-service-property-provider.h"
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

    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    panIdChangeThreshold = b_device_service_property_provider_get_property_as_uint32(
        propertyProvider, ZIGBEE_DEFENDER_PAN_ID_CHANGE_THRESHOLD_OPTION, DEFENDER_PAN_ID_CHANGE_THRESHOLD_DEFAULT);

    panIdChangeWindowMillis =
        b_device_service_property_provider_get_property_as_uint32(propertyProvider,
                                                                  ZIGBEE_DEFENDER_PAN_ID_CHANGE_WINDOW_MILLIS_OPTION,
                                                                  DEFENDER_PAN_ID_CHANGE_WINDOW_MILLIS_DEFAULT);

    panIdChangeRestoreMillis =
        b_device_service_property_provider_get_property_as_uint32(propertyProvider,
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
