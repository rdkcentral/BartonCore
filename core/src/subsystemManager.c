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
 * Created by Thomas Lea on 3/10/16.
 */

#include "deviceService.h"
#include "deviceServiceConfiguration.h"
#include "icUtil/stringUtils.h"
#include "provider/barton-core-property-provider.h"
#define LOG_TAG     "deviceService"
#define logFmt(fmt) "subsystemManager: %s - " fmt, __func__

#include <assert.h>
#include <icLog/logging.h>
#include <inttypes.h>

#include <icConcurrent/threadUtils.h>
#include <icTypes/icHashMap.h>
#include <pthread.h>
#include <string.h>

#ifdef BARTON_CONFIG_ZIGBEE
#include <zhal/zhal.h>
#endif

#include "subsystemManager.h"

#include "deviceDriverManager.h"
#include "deviceServicePrivate.h"
#include "event/deviceEventProducer.h"

typedef struct
{
    const Subsystem *subsystem;

    pthread_mutex_t mtx;
    bool ready;
} SubsystemRegistration;

#define MAP_FOREACH(value, map, expr)                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (map)                                                                                                       \
        {                                                                                                              \
            GHashTableIter iter = {0};                                                                                 \
            gpointer key = {0};                                                                                        \
            gpointer _value = {0};                                                                                     \
            g_hash_table_iter_init(&iter, (map));                                                                      \
            while (g_hash_table_iter_next(&iter, &key, &_value))                                                       \
            {                                                                                                          \
                value = (SubsystemRegistration *) _value;                                                              \
                expr                                                                                                   \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

static pthread_once_t initOnce = PTHREAD_ONCE_INIT;

static pthread_rwlock_t mutex = PTHREAD_RWLOCK_INITIALIZER;
static GHashTable *subsystems;
static bool allDriversStarted = false;
static subsystemManagerReadyForDevicesFunc readyForDevicesCB = NULL;
static void checkSubsystemForMigration(SubsystemRegistration *registration);

static void subsystemRegistrationDestroy(SubsystemRegistration *reg)
{
    pthread_mutex_destroy(&reg->mtx);
    free(reg);
}

static void subsystemRegistrationMapDestroy(void *value)
{
    subsystemRegistrationDestroy(value);
}

/**
 * Acquire read lock on 'mutex' before calling
 * @param subsystem
 * @param isReady
 */
static void setSubsystemReadyLocked(const char *subsystem, bool isReady)
{
    if (subsystem == NULL)
    {
        return;
    }

    SubsystemRegistration *reg = g_hash_table_lookup(subsystems, subsystem);
    if (reg != NULL)
    {
        mutexLock(&reg->mtx);
        reg->ready = isReady;
        mutexUnlock(&reg->mtx);
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: Gratuitous subsystem ready received for '%s'!", __func__, subsystem);
    }
}

static void onSubsystemInitialized(const char *subsystem)
{
    {
        READ_LOCK_SCOPE(mutex);
        setSubsystemReadyLocked(subsystem, true);
    }

    icLogDebug(LOG_TAG, "%s: '%s'", __func__, subsystem);

    scoped_icLinkedListNofree *drivers = deviceDriverManagerGetDeviceDriversBySubsystem(subsystem);
    scoped_icLinkedListIterator *it = linkedListIteratorCreate(drivers);
    while (linkedListIteratorHasNext(it) == true)
    {
        const DeviceDriver *driver = linkedListIteratorGetNext(it);
        if (driver->subsystemInitialized != NULL)
        {
            driver->subsystemInitialized(driver->callbackContext);
        }
    }

    sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonSubsystemStatus);

    // If everything is now ready, callback
    if (subsystemManagerIsReadyForDevices() == true && readyForDevicesCB != NULL)
    {
        readyForDevicesCB();
    }
}

static void onSubsystemDeinitialized(const char *subsystem)
{
    {
        READ_LOCK_SCOPE(mutex);
        setSubsystemReadyLocked(subsystem, false);
    }

    sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonSubsystemStatus);

    icLogDebug(LOG_TAG, "%s: '%s'", __func__, subsystem);
}

/**
 * Called by atexit
 */
static void unregisterSubsystems(void)
{
    g_hash_table_destroy(subsystems);
    subsystems = NULL;
}

void subsystemManagerRegister(Subsystem *subsystem)
{
    if (subsystem == NULL || subsystem->initialize == NULL || subsystem->name == NULL)
    {
        icLogError(LOG_TAG, "%s: Can't register subsystem with unknown type or no initializer!", __func__);
        return;
    }

    SubsystemRegistration *registration = malloc(sizeof(SubsystemRegistration));
    registration->subsystem = subsystem;
    mutexInitWithType(&registration->mtx, PTHREAD_MUTEX_ERRORCHECK);
    registration->ready = false;

    {
        WRITE_LOCK_SCOPE(mutex);

        if (subsystems == NULL)
        {
            subsystems =
                g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify) subsystemRegistrationMapDestroy);
            atexit(unregisterSubsystems);
        }

        if (!g_hash_table_insert(subsystems, (char *) subsystem->name, registration))
        {
            subsystemRegistrationDestroy(registration);
            registration = NULL;
            icLogWarn(LOG_TAG, "%s: unable to register subsystem '%s'!", __func__, subsystem->name);
        }
        else
        {
            icLogInfo(LOG_TAG, "subsystem '%s' registered", subsystem->name);
        }
    }
}

GPtrArray *subsystemManagerGetRegisteredSubsystems(void)
{
    icDebug();

    GPtrArray *result = g_ptr_array_new_full(0, g_free);

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, result);

    SubsystemRegistration *registration = NULL;

    MAP_FOREACH(registration, subsystems, g_ptr_array_add(result, g_strdup(registration->subsystem->name)););

    return result;
}

cJSON *subsystemManagerGetSubsystemStatusJson(const char *subsystemName)
{
    icDebug();

    cJSON *result = NULL;

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, NULL);

    SubsystemRegistration *reg = g_hash_table_lookup(subsystems, subsystemName);
    if (reg != NULL)
    {
        mutexLock(&reg->mtx);
        if (reg->subsystem->getStatusJson != NULL)
        {
            result = reg->subsystem->getStatusJson();
        }
        mutexUnlock(&reg->mtx);
    }

    return result;
}

static void checkSubsystemForMigration(SubsystemRegistration *registration)
{
    scoped_generic char *subsystemVersionKey = stringBuilder("%sSubsystemVersion", registration->subsystem->name);
    scoped_generic char *oldSubsystemVersionString = NULL;
    uint16_t oldSubsystemVersion = 0;

    if (!deviceServiceGetSystemProperty(subsystemVersionKey, &oldSubsystemVersionString))
    {
        icWarn("Did not find subsystem version for %s in system properties. Assuming 0", subsystemVersionKey);
    }
    else if (!stringToUint16(oldSubsystemVersionString, &oldSubsystemVersion))
    {
        icWarn("Could not convert subsystem version %s stored in system properties to number. Aborting.",
               subsystemVersionKey);
        return;
    }

    // Assume we are saving the version. Only don't save the version if we fail migration, a migration routine is
    // unspecified yet a subsystem version is specified, or we somehow went back in time.
    bool saveNewVersion = false;

    if (oldSubsystemVersion < registration->subsystem->version)
    {
        icDebug("%s : old version - %" PRIu16 " is less than new version - %" PRIu16,
                subsystemVersionKey,
                oldSubsystemVersion,
                registration->subsystem->version);

        // Capture developer mistake: a non-zero version is registered with a subsystem, but a migration routine is not.
        assert(registration->subsystem->migrate != NULL);

        if (registration->subsystem->migrate != NULL)
        {

            icDebug("Migration routine available for subsystem %s. Running...", registration->subsystem->name);

            if (!(saveNewVersion =
                      registration->subsystem->migrate(oldSubsystemVersion, registration->subsystem->version)))
            {
                icError("Migration for subsystem %s failed. Not saving new version.", registration->subsystem->name);
            }
        }
    }
    else if (oldSubsystemVersion > registration->subsystem->version)
    {
        icError("%s : old version - %" PRIu16 " is somehow greater than new version - %" PRIu16
                ". Have we gone back in time?",
                subsystemVersionKey,
                oldSubsystemVersion,
                registration->subsystem->version);
    }
    else
    {
        // Nothing to do, but save in case we didn't have this version recorded.
        saveNewVersion = true;
    }

    if (saveNewVersion)
    {

        icDebug("Saving new %s (%" PRIu16 ")", subsystemVersionKey, registration->subsystem->version);

        scoped_generic char *newSubsystemVersionString = stringBuilder("%" PRIu16, registration->subsystem->version);

        if (!deviceServiceSetSystemProperty(subsystemVersionKey, newSubsystemVersionString))
        {
            icError("Failed to save new version!");
        }
    }
}

void subsystemManagerInitialize(subsystemManagerReadyForDevicesFunc readyForDevicesCallback)
{
    icLogDebug(LOG_TAG, "%s", __func__);

    {
        g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        scoped_generic char *disabledSubsystems = b_core_property_provider_get_property_as_string(
            propertyProvider, "device.subsystem.disable", NULL);

        WRITE_LOCK_SCOPE(mutex);
        readyForDevicesCB = readyForDevicesCallback;
        if (disabledSubsystems && subsystems)
        {
            SubsystemRegistration *registration = NULL;
            MAP_FOREACH(
                registration, subsystems, if (strstr(disabledSubsystems, registration->subsystem->name) != NULL) {
                    g_hash_table_iter_remove(&iter);
                });
        }
    }

    {
        READ_LOCK_SCOPE(mutex);

        g_return_if_fail(subsystems != NULL);

        // Attempt to perform migrations as necessary before kicking off initialization
        SubsystemRegistration *registration = NULL;
        MAP_FOREACH(registration, subsystems, checkSubsystemForMigration(registration););

        registration = NULL;
        MAP_FOREACH(registration,
                    subsystems,

                    mutexLock(&registration->mtx);
                    registration->ready = false;
                    mutexUnlock(&registration->mtx);
                    registration->subsystem->initialize(onSubsystemInitialized, onSubsystemDeinitialized););
    }
}

void subsystemManagerShutdown(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    {
        WRITE_LOCK_SCOPE(mutex);
        readyForDevicesCB = NULL;
    }

    {
        READ_LOCK_SCOPE(mutex);

        g_return_if_fail(subsystems != NULL);

        SubsystemRegistration *registration = NULL;
        MAP_FOREACH(
            registration,
            subsystems,

            mutexLock(&registration->mtx);
            registration->ready = false;
            mutexUnlock(&registration->mtx);

            if (registration->subsystem->shutdown != NULL) { registration->subsystem->shutdown(); });
    }
}

void subsystemManagerAllDriversStarted(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    {
        READ_LOCK_SCOPE(mutex);

        g_return_if_fail(subsystems != NULL);

        SubsystemRegistration *registration = NULL;
        MAP_FOREACH(
            registration,
            subsystems,

            if (registration->subsystem->onAllDriversStarted != NULL) {
                registration->subsystem->onAllDriversStarted();
            });
    }

    {
        WRITE_LOCK_SCOPE(mutex);
        allDriversStarted = true;
    }

    if (subsystemManagerIsReadyForDevices() == true && readyForDevicesCB != NULL)
    {
        readyForDevicesCB();
    }
}

void subsystemManagerAllServicesAvailable(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(
        registration,
        subsystems,

        if (registration->subsystem->onAllServicesAvailable != NULL) {
            registration->subsystem->onAllServicesAvailable();
        });
}

void subsystemManagerPostRestoreConfig(void)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(
        registration,
        subsystems,

        if (registration->subsystem->onPostRestoreConfig != NULL) { registration->subsystem->onPostRestoreConfig(); });
}

void subsystemManagerSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(
        registration,
        subsystems,

        if (registration->subsystem->setOtaUpgradeDelay != NULL) {
            icLogDebug(LOG_TAG,
                       "Setting %s OTA upgrade delay to : %" PRIu32 " seconds",
                       registration->subsystem->name,
                       delaySeconds);

            registration->subsystem->setOtaUpgradeDelay(delaySeconds);
        });
}

void subsystemManagerEnterLPM(void)
{
    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(
        registration,
        subsystems,

        if (registration->subsystem->onLPMStart != NULL) { registration->subsystem->onLPMStart(); });
}


void subsystemManagerExitLPM(void)
{
    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(
        registration,
        subsystems,

        if (registration->subsystem->onLPMEnd != NULL) { registration->subsystem->onLPMEnd(); });
}

/*
 * Check if a specific subsystem is ready for devices
 */
bool subsystemManagerIsSubsystemReady(const char *subsystem)
{
    bool result = false;

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, false);

    SubsystemRegistration *reg = g_hash_table_lookup(subsystems, subsystem);
    if (reg != NULL)
    {
        mutexLock(&reg->mtx);
        result = reg->ready;
        mutexUnlock(&reg->mtx);
    }

    return result;
}

/*
 * Check if all subsystems are ready for devices
 */
bool subsystemManagerIsReadyForDevices(void)
{
    bool allReady = true;

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, false);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(
        registration,
        subsystems,

        mutexLock(&registration->mtx);
        bool ready = registration->ready;
        mutexUnlock(&registration->mtx);

        if (ready == false) {
            icInfo("Subsystem %s is not yet ready", registration->subsystem->name);
            allReady = false;
            break;
        });

    return allReady && allDriversStarted;
}

/*
 * Restore config for RMA
 */
bool subsystemManagerRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    bool result = true;

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, false);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(
        registration,
        subsystems,

        if (registration->subsystem->onRestoreConfig != NULL) {
            result &= registration->subsystem->onRestoreConfig(tempRestoreDir, dynamicConfigPath);

            if (!result)
            {
                icWarn("failed for %s!", registration->subsystem->name);
            }
        });

    return result;
}
