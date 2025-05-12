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
 * Created by Thomas Lea on 9/19/16.
 */

#include "deviceCommunicationWatchdog.h"
#include "devicePrivateProperties.h"
#include "deviceServiceConfiguration.h"
#include "deviceServicePrivate.h"
#include "provider/device-service-property-provider.h"

#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icLog/telemetryMarkers.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG                    "deviceCommunicationWatchdog"
#define logFmt(fmt)                "%s: " fmt, __func__

#define MONITOR_THREAD_SLEEP_SECS  60
#define MIN_UPDATE_INTERVAL_MILLIS 5000

typedef struct
{
    char *uuid;                               // device id
    uint32_t commFailTimeoutSeconds;          // default timeout for device (in seconds)
    uint32_t commFailTimeRemainingMillis;     // Time remaining until comm fail. Read with getMillisUntilCommFail
    uint64_t commFailRemainingLastSyncMillis; // Monotonic timestamp of last time remaining update. set with
                                              // setMillisUntilCommFail
    bool inCommFail;                          // flag for if in commFail (true) or not (false)
} MonitoredDeviceInfo;

static pthread_mutex_t monitoredDevicesMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t monitorThreadId;
static GHashTable *monitoredDevices = NULL;
static void monitoredDevicesEntryDestroy(void *value);
static uint32_t getMillisUntilCommFail(const MonitoredDeviceInfo *info);
static uint64_t setMillisUntilCommFail(MonitoredDeviceInfo *info, uint32_t millisUntilCommFail);

static deviceCommunicationWatchdogCommFailedCallback failedCallback = NULL;
static deviceCommunicationWatchdogCommRestoredCallback restoredCallback = NULL;

static pthread_mutex_t controlMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t controlCond = PTHREAD_COND_INITIALIZER;
static bool fastCommFailTimer = false;

static void *commFailWatchdogThreadProc(void *arg);
static bool running = false;
static uint32_t monitorThreadSleepSeconds = MONITOR_THREAD_SLEEP_SECS;

void deviceCommunicationWatchdogInit(deviceCommunicationWatchdogCommFailedCallback failedcb,
                                     deviceCommunicationWatchdogCommRestoredCallback restoredcb)
{
    if (failedcb == NULL || restoredcb == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    pthread_mutex_lock(&controlMutex);

    if (failedCallback != NULL || restoredCallback != NULL)
    {
        icLogError(LOG_TAG, "%s: already initialized", __FUNCTION__);
        pthread_mutex_unlock(&controlMutex);
        return;
    }

    running = true;

    initTimedWaitCond(&controlCond);
    createThread(&monitorThreadId, commFailWatchdogThreadProc, NULL, "commFailWD");

    monitoredDevices = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, monitoredDevicesEntryDestroy);

    failedCallback = failedcb;
    restoredCallback = restoredcb;
    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    fastCommFailTimer =
        b_device_service_property_provider_get_property_as_bool(propertyProvider, FAST_COMM_FAIL_PROP, false);

    pthread_mutex_unlock(&controlMutex);
}

void deviceCommunicationWatchdogSetMonitorInterval(uint32_t seconds)
{
    monitorThreadSleepSeconds = seconds;
}

void deviceCommunicationWatchdogTerm()
{
    pthread_mutex_lock(&controlMutex);

    running = false;

    failedCallback = NULL;
    restoredCallback = NULL;

    pthread_mutex_lock(&monitoredDevicesMutex);
    g_hash_table_destroy(monitoredDevices);
    monitoredDevices = NULL;
    pthread_mutex_unlock(&monitoredDevicesMutex);

    pthread_cond_broadcast(&controlCond);

    pthread_mutex_unlock(&controlMutex);

    pthread_join(monitorThreadId, NULL);
}

void deviceCommunicationWatchdogMonitorDevice(const char *uuid, const uint32_t commFailTimeoutSeconds, bool inCommFail)
{
    icLogDebug(LOG_TAG,
               "%s: start monitoring %s with commFailTimeoutSeconds %d, inCommFail %s",
               __FUNCTION__,
               uuid,
               commFailTimeoutSeconds,
               inCommFail ? "true" : "false");

    if (uuid == NULL || commFailTimeoutSeconds == 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    pthread_mutex_lock(&monitoredDevicesMutex);

    if (running == true)
    {
        MonitoredDeviceInfo *info = (MonitoredDeviceInfo *) calloc(1, sizeof(MonitoredDeviceInfo));
        info->uuid = strdup(uuid);
        info->commFailTimeoutSeconds = commFailTimeoutSeconds;
        info->inCommFail = inCommFail;
        setMillisUntilCommFail(info, commFailTimeoutSeconds * 1000);

        // Defensive in case for some reason the device already exists(can happen for device recovery)
        if (g_hash_table_insert(monitoredDevices, g_strdup(info->uuid), info) == false)
        {
            monitoredDevicesEntryDestroy(info);
            info = NULL;
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: Ignoring monitoring of %s, we aren't running", __FUNCTION__, uuid);
    }

    pthread_mutex_unlock(&monitoredDevicesMutex);
}

void deviceCommunicationWatchdogStopMonitoringDevice(const char *uuid)
{
    icLogDebug(LOG_TAG, "%s: stop monitoring %s", __FUNCTION__, uuid);

    pthread_mutex_lock(&monitoredDevicesMutex);
    g_hash_table_remove(monitoredDevices, uuid);
    pthread_mutex_unlock(&monitoredDevicesMutex);
}

void deviceCommunicationWatchdogPetDevice(const char *uuid)
{
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return;
    }

    bool doNotify = false;

    pthread_mutex_lock(&monitoredDevicesMutex);
    MonitoredDeviceInfo *info =
        (MonitoredDeviceInfo *) g_hash_table_lookup(monitoredDevices, uuid);
    if (info != NULL)
    {
        if (setMillisUntilCommFail(info, info->commFailTimeoutSeconds * 1000) >= MIN_UPDATE_INTERVAL_MILLIS)
        {
            icDebug("petting %s", uuid);
            updateDeviceDateLastContacted(uuid);
        }

        if (info->inCommFail == true)
        {
            icLogInfo(LOG_TAG, "%s is no longer in comm fail", uuid);
            TELEMETRY_COUNTER(TELEMETRY_MARKER_DEVICE_COMMRESTORE);
            info->inCommFail = false;
            doNotify = true;
        }
    }
    pthread_mutex_unlock(&monitoredDevicesMutex);

    if (doNotify == true)
    {
        restoredCallback(uuid);
    }
}

void deviceCommunicationWatchdogCheckDevices(void)
{
    pthread_mutex_lock(&controlMutex);

    pthread_cond_broadcast(&controlCond);

    pthread_mutex_unlock(&controlMutex);
}

void deviceCommunicationWatchdogForceDeviceInCommFail(const char *uuid)
{
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    icLogDebug(LOG_TAG, "%s: forcing device %s to be in comm fail", __FUNCTION__, uuid);

    bool doNotify = false;

    pthread_mutex_lock(&monitoredDevicesMutex);
    MonitoredDeviceInfo *info =
        (MonitoredDeviceInfo *) g_hash_table_lookup(monitoredDevices, uuid);
    if (info != NULL)
    {
        // if device is not in comm fail
        //
        if (info->inCommFail == false)
        {
            // force the device to be in comm fail
            //
            info->inCommFail = true;

            // notify
            //
            doNotify = true;
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: device %s already in comm failure, ignoring", __FUNCTION__, uuid);
        }
    }
    pthread_mutex_unlock(&monitoredDevicesMutex);

    if (doNotify == true)
    {
        failedCallback(uuid);
    }
}

int32_t deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(const char *uuid, uint32_t commFailDelaySeconds)
{
    // FIXME: int32 is narrower than uint32 and may overflow. In practice, commFailDelaySeconds is < uint16_t
    //  and should just be changed to uint16_t.
    int32_t secsUntilCommFail = -1;
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return secsUntilCommFail;
    }

    pthread_mutex_lock(&monitoredDevicesMutex);

    MonitoredDeviceInfo *info =
        (MonitoredDeviceInfo *) g_hash_table_lookup(monitoredDevices, uuid);
    if (info != NULL)
    {

        if (info->inCommFail == true)
        {
            secsUntilCommFail = -1;
        }
        else
        {
            secsUntilCommFail = getMillisUntilCommFail(info) / 1000;

            // This function allows monitoring devices with more time than usual until
            // comm fail (e.g., when armed). It is considered an error to try to shorten
            // the timeout, as it could produce unmonitored devices that should go into comm fail later.
            int32_t secsDelayDiff = commFailDelaySeconds - info->commFailTimeoutSeconds;

            if (secsDelayDiff >= 0)
            {
                secsUntilCommFail += secsDelayDiff;
            }
            else
            {
                icWarn("ignoring invalid commfailDelaySeconds: shorter than minimum %" PRIu32
                       "s for device '%s'%" PRIu32,
                       commFailDelaySeconds,
                       uuid,
                       info->commFailTimeoutSeconds);
            }
        }

        icLogDebug(LOG_TAG, "%s: device %s has %" PRIi32 "seconds until comm fail", __func__, uuid, secsUntilCommFail);
    }

    pthread_mutex_unlock(&monitoredDevicesMutex);

    return secsUntilCommFail;
}

void deviceCommunicationWatchdogSetTimeRemainingForDeviceFromLPM(const char *uuid, int32_t commFailTimeRemainingSeconds)
{
    if (uuid == NULL || commFailTimeRemainingSeconds < 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    icLogDebug(LOG_TAG,
               "%s: setting new commFail time remaining %d for device %s",
               __FUNCTION__,
               commFailTimeRemainingSeconds,
               uuid);

    pthread_mutex_lock(&monitoredDevicesMutex);

    MonitoredDeviceInfo *info =
        (MonitoredDeviceInfo *) g_hash_table_lookup(monitoredDevices, uuid);
    if (info != NULL)
    {
        // if device is not in comm fail
        //
        if (info->inCommFail == false)
        {
            // set the new time remaining before device is considered in commFail
            //
            setMillisUntilCommFail(info, commFailTimeRemainingSeconds * 1000);
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: device %s already in comm failure, ignoring", __FUNCTION__, uuid);
        }
    }

    pthread_mutex_unlock(&monitoredDevicesMutex);
}

static void monitoredDevicesEntryDestroy(void *value)
{
    MonitoredDeviceInfo *info = (MonitoredDeviceInfo *) value;
    free(info->uuid);
    free(info);
}

static uint32_t getMillisUntilCommFail(const MonitoredDeviceInfo *info)
{
    const uint64_t millisSinceLastCheck = getMonotonicMillis() - info->commFailRemainingLastSyncMillis;
    uint32_t millisUntilCommFail = 0;

    if (info->commFailTimeRemainingMillis > millisSinceLastCheck)
    {
        millisUntilCommFail = info->commFailTimeRemainingMillis - millisSinceLastCheck;
    }

    icLogTrace(
        LOG_TAG, "%s: device %s has %" PRIu32 " millis until commFail", __func__, info->uuid, millisUntilCommFail);

    return millisUntilCommFail;
}

static uint64_t setMillisUntilCommFail(MonitoredDeviceInfo *info, uint32_t millisUntilCommFail)
{
    uint64_t nowMillis = getMonotonicMillis();
    uint64_t diffMillis = nowMillis - info->commFailRemainingLastSyncMillis;

    info->commFailRemainingLastSyncMillis = nowMillis;
    info->commFailTimeRemainingMillis = millisUntilCommFail;

    return diffMillis;
}

static void *commFailWatchdogThreadProc(void *arg)
{
    icLogDebug(LOG_TAG, "%s: starting up", __FUNCTION__);

    while (true)
    {
        pthread_mutex_lock(&controlMutex);
        if (running == false)
        {
            icLogInfo(LOG_TAG, "%s exiting", __FUNCTION__);
            pthread_mutex_unlock(&controlMutex);
            break;
        }

        if (fastCommFailTimer == false)
        {
            // FIXME: narrowing conversion (uint32->int)
            incrementalCondTimedWait(&controlCond, &controlMutex, monitorThreadSleepSeconds);
        }
        else
        {
            incrementalCondTimedWaitMillis(&controlCond, &controlMutex, monitorThreadSleepSeconds);
        }

        bool isCommfailFast = fastCommFailTimer;

        pthread_mutex_unlock(&controlMutex);

        GList *uuidsInCommFail = NULL;

        // iterate over all monitored devices and check to see if any have hit comm fail
        pthread_mutex_lock(&monitoredDevicesMutex);
        GHashTableIter monitoredDevicesIter;
        gchar *uuid;
        MonitoredDeviceInfo *info;
        g_hash_table_iter_init(&monitoredDevicesIter, monitoredDevices);
        while (g_hash_table_iter_next(&monitoredDevicesIter, (gpointer *) &uuid, (gpointer *) &info))
        {
            uint32_t millisUntilCommFail = getMillisUntilCommFail(info);

            icLogTrace(LOG_TAG, "%s: checking on %s", __FUNCTION__, uuid);

            if (isCommfailFast)
            {
                millisUntilCommFail /= 100;
            }

            if (millisUntilCommFail == 0 && info->inCommFail == false)
            {
                icLogWarn(LOG_TAG, "%s: %s is in comm fail", __FUNCTION__, uuid);
                TELEMETRY_COUNTER(TELEMETRY_MARKER_DEVICE_COMMFAIL);
                info->inCommFail = true;
                uuidsInCommFail = g_list_append(uuidsInCommFail, strdup(uuid));
            }

            icLogTrace(LOG_TAG,
                       "%s: device %s; millisLeft=%" PRIu32 "; lastChecked=%" PRIu64 "; inCommFail=%s",
                       __FUNCTION__,
                       uuid,
                       millisUntilCommFail,
                       info->commFailRemainingLastSyncMillis,
                       stringValueOfBool(info->inCommFail));

            setMillisUntilCommFail(info, millisUntilCommFail);
        }
        pthread_mutex_unlock(&monitoredDevicesMutex);

        GList *iter = uuidsInCommFail;
        while (iter != NULL)
        {
            icLogDebug(LOG_TAG, "%s: notifying callback of comm fail on %s", __FUNCTION__, (char*)iter->data);
            failedCallback(iter->data);
            iter = iter->next;
        }
        g_list_free_full(uuidsInCommFail, g_free);
    }

    return NULL;
}

void deviceCommunicationWatchdogSetFastCommfail(bool inFastCommfail)
{
    pthread_mutex_lock(&controlMutex);

    fastCommFailTimer = inFastCommfail;
    pthread_cond_broadcast(&controlCond);

    pthread_mutex_unlock(&controlMutex);
}

bool deviceCommunicationWatchdogIsDeviceMonitored(const char *uuid)
{
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    pthread_mutex_lock(&monitoredDevicesMutex);
    gboolean isMonitored = g_hash_table_contains(monitoredDevices, uuid);
    pthread_mutex_unlock(&monitoredDevicesMutex);

    return isMonitored;
}
