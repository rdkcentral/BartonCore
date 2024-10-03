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
// Created by tlea on 7/9/19.
//

#include "zigbeeHealthCheck.h"
#include "devicePrivateProperties.h"
#include "deviceServiceConfiguration.h"
#include "provider/device-service-property-provider.h"
#include <event/deviceEventProducer.h>
#include <icLog/logging.h>
#include <pthread.h>
#include <stdint.h>
#include <zhal/zhal.h>

#define LOG_TAG                                                             "zigbeeHealthCheck"

// dont allow health checking faster than this
#define MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS                            1000

// default to off
#define NETWORK_HEALTH_CHECK_INTERVAL_MILLIS_DEFAULT                        0

// positive values dont make sense and are used to disable adjusting the CCA threshold
#define NETWORK_HEALTH_CHECK_CCA_THRESHOLD_DEFAULT                          1

#define NETWORK_HEALTH_CHECK_CCA_FAILURE_THRESHOLD_DEFAULT                  10
#define NETWORK_HEALTH_CHECK_RESTORE_THRESHOLD_DEFAULT                      600
#define NETWORK_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS_DEFAULT 1000

static pthread_mutex_t interferenceDetectedMtx = PTHREAD_MUTEX_INITIALIZER;
static bool interferenceDetected = false;

void zigbeeHealthCheckStart()
{
    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    uint32_t intervalMillis = b_device_service_property_provider_get_property_as_uint32(
        propertyProvider, ZIGBEE_HEALTH_CHECK_INTERVAL_MILLIS, NETWORK_HEALTH_CHECK_INTERVAL_MILLIS_DEFAULT);
    if (intervalMillis == 0)
    {
        icLogDebug(LOG_TAG, "%s: not monitoring, feature disabled", __FUNCTION__);

        zigbeeHealthCheckStop();

        // if there was interference before, we need to send a clear event since we are stopping monitoring
        pthread_mutex_lock(&interferenceDetectedMtx);
        if (interferenceDetected)
        {
            interferenceDetected = false;
            sendZigbeeNetworkInterferenceEvent(false);
        }
        pthread_mutex_unlock(&interferenceDetectedMtx);
    }
    else
    {
        if (intervalMillis < MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS)
        {
            icLogWarn(LOG_TAG,
                      "%s: Attempt to set network health check intervalMillis to %" PRIu32
                      " is below minimum, using %" PRIu32,
                      __FUNCTION__,
                      intervalMillis,
                      MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS);

            intervalMillis = MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS;
        }

        int32_t ccaThreshold = b_device_service_property_provider_get_property_as_int32(
            propertyProvider, ZIGBEE_HEALTH_CHECK_CCA_THRESHOLD, NETWORK_HEALTH_CHECK_CCA_THRESHOLD_DEFAULT);
        uint32_t ccaFailureThreshold = b_device_service_property_provider_get_property_as_uint32(
            propertyProvider,
            ZIGBEE_HEALTH_CHECK_CCA_FAILURE_THRESHOLD,
            NETWORK_HEALTH_CHECK_CCA_FAILURE_THRESHOLD_DEFAULT);
        uint32_t restoreThreshold = b_device_service_property_provider_get_property_as_uint32(
            propertyProvider, ZIGBEE_HEALTH_CHECK_RESTORE_THRESHOLD, NETWORK_HEALTH_CHECK_RESTORE_THRESHOLD_DEFAULT);
        uint32_t delayBetweenRetriesMillis = b_device_service_property_provider_get_property_as_uint32(
            propertyProvider,
            ZIGBEE_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS,
            NETWORK_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS_DEFAULT);

        if (zhalConfigureNetworkHealthCheck(
                intervalMillis, ccaThreshold, ccaFailureThreshold, restoreThreshold, delayBetweenRetriesMillis) ==
            false)
        {
            icLogError(LOG_TAG, "%s: failed to start network health checking", __FUNCTION__);
        }
    }
}

void zigbeeHealthCheckStop()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (zhalConfigureNetworkHealthCheck(0, 0, 0, 0, 0) == false)
    {
        icLogError(LOG_TAG, "%s: failed to stop network health checking", __FUNCTION__);
    }
}

void zigbeeHealthCheckSetProblem(bool problemExists)
{
    icLogDebug(LOG_TAG, "%s: problemExists = %s", __FUNCTION__, problemExists ? "true" : "false");

    pthread_mutex_lock(&interferenceDetectedMtx);
    interferenceDetected = problemExists;
    pthread_mutex_unlock(&interferenceDetectedMtx);

    sendZigbeeNetworkInterferenceEvent(problemExists);
}
