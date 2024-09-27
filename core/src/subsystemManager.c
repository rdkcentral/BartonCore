//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
/*
 * Created by Thomas Lea on 3/10/16.
 */

#include "deprecated/deviceService.h"
#include "deviceServiceConfiguration.h"
#include "icUtil/stringUtils.h"
#include "provider/device-service-property-provider.h"
#define LOG_TAG "deviceService"
#define logFmt(fmt) "subsystemManager: %s - " fmt, __func__

#include <assert.h>
#include <icLog/logging.h>
#include <inttypes.h>

#include <zhal/zhal.h>
#include <string.h>
#include <icTypes/icHashMap.h>
#include <pthread.h>
#include <icConcurrent/threadUtils.h>

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

#define MAP_FOREACH(value, map, expr)                                           \
do                                                                              \
{                                                                               \
    scoped_icHashMapIterator *it = hashMapIteratorCreate((map));                \
    while (hashMapIteratorHasNext(it) == true)                                  \
    {                                                                           \
        void *key = NULL;                                                       \
        uint16_t keyLen = 0;                                                    \
        hashMapIteratorGetNext(it, &key, &keyLen, (void **) &(value));          \
        expr                                                                    \
    }                                                                           \
} while (0)

static pthread_once_t  initOnce = PTHREAD_ONCE_INIT;

static pthread_rwlock_t mutex = PTHREAD_RWLOCK_INITIALIZER;
static icHashMap *subsystems;
static bool allDriversStarted = false;
static subsystemManagerReadyForDevicesFunc readyForDevicesCB = NULL;
static void checkSubsystemForMigration(SubsystemRegistration *registration);

static void subsystemRegistrationDestroy(SubsystemRegistration *reg)
{
    pthread_mutex_destroy(&reg->mtx);
    free(reg);
}

static void subsystemRegistrationMapDestroy(void *key, void *value)
{
    (void) key;

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

    SubsystemRegistration *reg = hashMapGet(subsystems, (void *)subsystem, strlen(subsystem));
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
    hashMapDestroy(subsystems, subsystemRegistrationMapDestroy);
    subsystems = NULL;
}

void subsystemManagerRegister(Subsystem *subsystem)
{
    if (subsystem == NULL ||
        subsystem->initialize == NULL ||
        subsystem->name == NULL)
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
            subsystems = hashMapCreate();
            atexit(unregisterSubsystems);
        }

        if (hashMapPut(subsystems,
                       (char *) subsystem->name,
                       strlen(subsystem->name),
                       registration) == false)
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

icLinkedList *subsystemManagerGetRegisteredSubsystems(void)
{
    icDebug();

    icLinkedList *result = linkedListCreate();

    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                linkedListAppend(result, strdup(registration->subsystem->name));
    );

    return result;
}

cJSON *subsystemManagerGetSubsystemStatusJson(const char *subsystemName)
{
    icDebug();

    cJSON *result = NULL;

    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *reg = hashMapGet(subsystems, (char *) subsystemName, strlen(subsystemName));
    if (reg != NULL)
    {
        mutexLock(&reg->mtx);
        result = reg->subsystem->getStatusJson();
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
        g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        scoped_generic char *disabledSubsystems = b_device_service_property_provider_get_property_as_string(
            propertyProvider, "device.subsystem.disable", NULL);

        WRITE_LOCK_SCOPE(mutex);
        readyForDevicesCB = readyForDevicesCallback;
        if (disabledSubsystems != NULL)
        {
            SubsystemRegistration *registration = NULL;
            MAP_FOREACH(registration, subsystems,
                        if (strstr(disabledSubsystems, registration->subsystem->name) != NULL)
                        {
                            hashMapIteratorDeleteCurrent(it, subsystemRegistrationMapDestroy);
                        }
            );
        }

    }

    {
        READ_LOCK_SCOPE(mutex);

        // Attempt to perform migrations as necessary before kicking off initialization
        SubsystemRegistration *registration = NULL;
        MAP_FOREACH(registration, subsystems, checkSubsystemForMigration(registration););

        registration = NULL;
        MAP_FOREACH(registration, subsystems,

                    mutexLock(&registration->mtx);
                    registration->ready = false;
                    mutexUnlock(&registration->mtx);
                    registration->subsystem->initialize(onSubsystemInitialized, onSubsystemDeinitialized);
        );
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

        SubsystemRegistration *registration = NULL;
        MAP_FOREACH(registration, subsystems,

                    mutexLock(&registration->mtx);
                    registration->ready = false;
                    mutexUnlock(&registration->mtx);

                    if (registration->subsystem->shutdown != NULL)
                    {
                        registration->subsystem->shutdown();
                    }
        );
    }
}

void subsystemManagerAllDriversStarted(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    {
        READ_LOCK_SCOPE(mutex);

        SubsystemRegistration *registration = NULL;
        MAP_FOREACH(registration, subsystems,

                    if (registration->subsystem->onAllDriversStarted != NULL)
                    {
                        registration->subsystem->onAllDriversStarted();
                    }
        );
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

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                if (registration->subsystem->onAllServicesAvailable != NULL)
                {
                    registration->subsystem->onAllServicesAvailable();
                }
    );
}

void subsystemManagerPostRestoreConfig(void)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                if (registration->subsystem->onPostRestoreConfig != NULL)
                {
                    registration->subsystem->onPostRestoreConfig();
                }
    );
}

void subsystemManagerSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                if (registration->subsystem->setOtaUpgradeDelay != NULL)
                {
                    icLogDebug(LOG_TAG,
                               "Setting %s OTA upgrade delay to : %" PRIu32 " seconds",
                               registration->subsystem->name, delaySeconds);

                    registration->subsystem->setOtaUpgradeDelay(delaySeconds);
                }
    );
}

void subsystemManagerEnterLPM(void)
{
    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                if (registration->subsystem->onLPMStart != NULL)
                {
                    registration->subsystem->onLPMStart();
                }
    );
}


void subsystemManagerExitLPM(void)
{
    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                if (registration->subsystem->onLPMEnd != NULL)
                {
                    registration->subsystem->onLPMEnd();
                }
    );
}

/*
 * Check if a specific subsystem is ready for devices
 */
bool subsystemManagerIsSubsystemReady(const char *subsystem)
{
    bool result = false;

    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *reg = hashMapGet(subsystems, (char *) subsystem, strlen(subsystem));
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

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                mutexLock(&registration->mtx);
                bool ready = registration->ready;
                mutexUnlock(&registration->mtx);

                if (ready == false)
                {
                    icInfo("Subsystem %s is not yet ready", registration->subsystem->name);
                    allReady = false;
                    break;
                }
    );

    return allReady && allDriversStarted;
}

/*
 * Restore config for RMA
 */
bool subsystemManagerRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    bool result = true;

    READ_LOCK_SCOPE(mutex);

    SubsystemRegistration *registration = NULL;
    MAP_FOREACH(registration, subsystems,

                if (registration->subsystem->onRestoreConfig != NULL)
                {
                    result &= registration->subsystem->onRestoreConfig(tempRestoreDir, dynamicConfigPath);

                    if (!result)
                    {
                        icWarn("failed for %s!", registration->subsystem->name);
                    }
                }
    );

    return result;
}
