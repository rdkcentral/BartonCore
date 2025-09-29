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
#include "glib.h"
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

static pthread_rwlock_t mutex = PTHREAD_RWLOCK_INITIALIZER;
static GHashTable *subsystems;
static bool allDriversStarted = false;
static void subsystemPreRegistrationDestroy(void);
static subsystemManagerReadyForDevicesFunc readyForDevicesCB = NULL;
static void checkSubsystemForMigration(SubsystemRegistration *registration);
static SubsystemRegistration *subsystemRegistrationAcquire(SubsystemRegistration *reg);
static bool subsystemManagerInitialized = false;

// Pre-registered subsystems are those that wish to be registered before the
// subsystem manager is initialized. These are typically automatically registered
// __attribute__((constructor)) functions in subsystem modules.
static GList *preRegisteredSubsystems = NULL;

__attribute__((constructor)) void registerPreRegistrationDestroy(void)
{
    atexit(subsystemPreRegistrationDestroy);
}

Subsystem *createSubsystem(void)
{
    Subsystem *retVal = g_atomic_rc_box_new0(Subsystem);
    return g_steal_pointer(&retVal);
}

static void destroySubsystem(Subsystem *subsystem)
{
    // no members of Subsystem to free
    return;
}

Subsystem *acquireSubsystem(Subsystem *subsystem)
{
    Subsystem *retVal = NULL;

    if (subsystem)
    {
        retVal = g_atomic_rc_box_acquire(subsystem);
    }

    return retVal;
}

void releaseSubsystem(Subsystem *subsystem)
{
    if (subsystem)
    {
        g_atomic_rc_box_release_full(subsystem, (GDestroyNotify) destroySubsystem);
    }
}

/**
 * Create and initialize a new refcounted SubsystemRegistration.
 *
 * @param subsystem The subsystem to create a registration for
 * @return Pointer to new reference counted SubsystemRegistration
 */
static SubsystemRegistration *createSubsystemRegistration(Subsystem *subsystem)
{
    SubsystemRegistration *retVal = g_atomic_rc_box_new0(SubsystemRegistration);
    retVal->subsystem = acquireSubsystem(subsystem);
    mutexInitWithType(&retVal->mtx, PTHREAD_MUTEX_ERRORCHECK);
    retVal->ready = false;

    return g_steal_pointer(&retVal);
}

/**
 * Acquires a pointer to the reference counted SubsystemRegistration, with its reference count increased.
 *
 * @param reg pointer to reference counted SubsystemRegistration
 */
static SubsystemRegistration *subsystemRegistrationAcquire(SubsystemRegistration *reg)
{
    SubsystemRegistration *retVal = NULL;

    if (reg)
    {
        retVal = g_atomic_rc_box_acquire(reg);
    }

    return retVal;
}

/**
 * Cleans up contents of SubsystemRegistration - GLib frees the struct itself when refcount reaches zero
 */
static void subsystemRegistrationDestroy(SubsystemRegistration *reg)
{
    if (reg)
    {
        pthread_mutex_destroy(&reg->mtx);
        releaseSubsystem((Subsystem *) reg->subsystem);
    }
}

/**
 * Release a SubsystemRegistration. This will decrement the refcount and destroy the SubsystemRegistration if the
 * refcount reaches zero.
 *
 * @param reg pointer to reference counted SubsystemRegistration
 */
static void subsystemRegistrationRelease(SubsystemRegistration *reg)
{
    if (reg)
    {
        g_atomic_rc_box_release_full(reg, (GDestroyNotify) subsystemRegistrationDestroy);
    }
}

/*
 * Convenience macro to declare a scope bound SubsystemRegistration
 */
G_DEFINE_AUTOPTR_CLEANUP_FUNC(SubsystemRegistration, subsystemRegistrationRelease)

static void subsystemRegistrationMapDestroy(void *value)
{
    subsystemRegistrationRelease((SubsystemRegistration *) value);
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

    g_autoptr(SubsystemRegistration) reg = subsystemRegistrationAcquire(g_hash_table_lookup(subsystems, subsystem));
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
 * @brief Cleanup pre-registered subsystems at program exit.
 */
static void subsystemPreRegistrationDestroy(void)
{
    WRITE_LOCK_SCOPE(mutex);
    g_list_free_full(preRegisteredSubsystems, (GDestroyNotify) releaseSubsystem);
    preRegisteredSubsystems = NULL;
}

/**
 * Helper to register a subsystem to a specific hash table
 *
 * @param subsystem The subsystem to register
 */
static void subsystemManagerRegisterToTable(Subsystem *subsystem, GHashTable *table)
{
    if (subsystem == NULL || subsystem->initialize == NULL || subsystem->name == NULL || table == NULL)
    {
        icLogError(LOG_TAG, "%s: Can't register subsystem with unknown type or no initializer!", __func__);
        return;
    }

    SubsystemRegistration *registration = createSubsystemRegistration(subsystem);

    if (!g_hash_table_insert(table, (char *) subsystem->name, registration))
    {
        icLogWarn(LOG_TAG, "%s: unable to register subsystem '%s'!", __func__, subsystem->name);
        subsystemRegistrationDestroy(registration);
    }
    else
    {
        icLogInfo(LOG_TAG, "subsystem '%s' registered", subsystem->name);
    }
}

void subsystemManagerRegister(Subsystem *subsystem)
{
    WRITE_LOCK_SCOPE(mutex);
    if (!subsystemManagerInitialized)
    {
        preRegisteredSubsystems = g_list_append(preRegisteredSubsystems, acquireSubsystem(subsystem));
    }
    else
    {
        subsystemManagerRegisterToTable(subsystem, subsystems);
    }
}

static void collectSubsystemNamesCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    GPtrArray *result = (GPtrArray *) user_data;
    g_ptr_array_add(result, g_strdup(registration->subsystem->name));
}

GPtrArray *subsystemManagerGetRegisteredSubsystems(void)
{
    icDebug();

    GPtrArray *result = g_ptr_array_new_full(0, g_free);

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, result);

    g_hash_table_foreach(subsystems, collectSubsystemNamesCallback, result);

    return result;
}

cJSON *subsystemManagerGetSubsystemStatusJson(const char *subsystemName)
{
    icDebug();

    cJSON *result = NULL;

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, NULL);

    g_autoptr(SubsystemRegistration) reg = subsystemRegistrationAcquire(g_hash_table_lookup(subsystems, subsystemName));
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

/**
 * @brief Predicate function for removing disabled subsystems from the hash table.
 *
 * Used with g_hash_table_foreach_remove() to conditionally remove subsystems
 * that are found in the disabled subsystems configuration string.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Pointer to disabled subsystems configuration string
 * @return TRUE if the subsystem should be removed, FALSE to keep it
 */
static gboolean removeDisabledSubsystemPredicate(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_val_if_fail(registration != NULL, FALSE);

    const char *disabledSubsystems = (const char *) user_data;

    return strstr(disabledSubsystems, registration->subsystem->name) != NULL;
}

/**
 * @brief Callback function for performing subsystem migrations.
 *
 * Used with g_hash_table_foreach() to check and perform migrations for each
 * registered subsystem as necessary.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void checkSubsystemForMigrationCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    checkSubsystemForMigration(registration);
}

/**
 * @brief Callback function for initializing subsystems.
 *
 * Used with g_hash_table_foreach() to initialize each registered subsystem.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void initializeSubsystemCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    mutexLock(&registration->mtx);
    registration->ready = false;
    mutexUnlock(&registration->mtx);

    if (registration->subsystem->initialize)
    {
        registration->subsystem->initialize(onSubsystemInitialized, onSubsystemDeinitialized);
    }
}

void subsystemManagerInitialize(subsystemManagerReadyForDevicesFunc readyForDevicesCallback)
{
    icLogDebug(LOG_TAG, "%s", __func__);

    GList *localPreRegisteredSubsystems = NULL;
    GHashTable *localSubsystems = NULL;
    bool localSubsystemManagerInitialized = false;

    {
        WRITE_LOCK_SCOPE(mutex);

        if (subsystemManagerInitialized)
        {
            return;
        }

        localSubsystemManagerInitialized = subsystemManagerInitialized;
        localPreRegisteredSubsystems = g_steal_pointer(&preRegisteredSubsystems);
        localSubsystems = g_steal_pointer(&subsystems);
    }

    if (!localSubsystemManagerInitialized)
    {
        localSubsystems =
            g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify) subsystemRegistrationMapDestroy);

        if (localPreRegisteredSubsystems)
        {
            g_list_foreach(localPreRegisteredSubsystems, (GFunc) subsystemManagerRegisterToTable, localSubsystems);
        }

        localSubsystemManagerInitialized = true;
    }

    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    scoped_generic char *disabledSubsystems =
        b_core_property_provider_get_property_as_string(propertyProvider, "device.subsystem.disable", NULL);

    if (disabledSubsystems && localSubsystems)
    {
        g_hash_table_foreach_remove(localSubsystems, removeDisabledSubsystemPredicate, disabledSubsystems);
    }

    // Attempt to perform migrations as necessary before kicking off initialization
    g_hash_table_foreach(localSubsystems, checkSubsystemForMigrationCallback, NULL);

    // Move local work to globals atomically. We must do this BEFORE calling
    // subsystem->initialize() because those functions will trigger callbacks
    // that need to access the global subsystems table.
    {
        WRITE_LOCK_SCOPE(mutex);
        readyForDevicesCB = readyForDevicesCallback;
        subsystems = g_steal_pointer(&localSubsystems);
        preRegisteredSubsystems = g_steal_pointer(&localPreRegisteredSubsystems);
        subsystemManagerInitialized = localSubsystemManagerInitialized;
    }

    {
        READ_LOCK_SCOPE(mutex);

        g_return_if_fail(subsystems != NULL);

        g_hash_table_foreach(subsystems, initializeSubsystemCallback, NULL);
    }
}

/**
 * @brief Callback function for shutting down subsystems.
 *
 * Used with g_hash_table_foreach() to shutdown each registered subsystem.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void shutdownSubsystemCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    mutexLock(&registration->mtx);
    registration->ready = false;
    mutexUnlock(&registration->mtx);

    if (registration->subsystem->shutdown)
    {
        registration->subsystem->shutdown();
    }
}

void subsystemManagerShutdown(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    GHashTable *localSubsystems = NULL;
    bool localSubsystemManagerInitialized = false;

    {
        WRITE_LOCK_SCOPE(mutex);
        localSubsystemManagerInitialized = subsystemManagerInitialized;

        if (localSubsystemManagerInitialized)
        {
            localSubsystems = g_steal_pointer(&subsystems);
            subsystemManagerInitialized = false;
            readyForDevicesCB = NULL;
        }
    }

    icDebug("finished stealing");

    if (localSubsystemManagerInitialized)
    {
        icDebug("about to loop through subsystems to shut them down");
        g_hash_table_foreach(localSubsystems, shutdownSubsystemCallback, NULL);
        icDebug("finished shutting down subsystems");

        if (localSubsystems)
        {
            icDebug("about to destroy local subsystems");
            g_hash_table_destroy(localSubsystems);
        }
    }

    icDebug("complete");
}

/**
 * @brief Callback function for notifying subsystems that all drivers have started.
 *
 * Used with g_hash_table_foreach() to notify each registered subsystem that
 * all device drivers have been started and are ready for operation.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void notifyAllDriversStartedCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    if (registration->subsystem->onAllDriversStarted != NULL)
    {
        registration->subsystem->onAllDriversStarted();
    }
}

void subsystemManagerAllDriversStarted(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    {
        READ_LOCK_SCOPE(mutex);

        g_return_if_fail(subsystems != NULL);

        g_hash_table_foreach(subsystems, notifyAllDriversStartedCallback, NULL);
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


/**
 * @brief Callback function for notifying subsystems that all services are available.
 *
 * Used with g_hash_table_foreach() to notify each registered subsystem that
 * all system services are now available and ready for use.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void notifyAllServicesAvailableCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    if (registration->subsystem->onAllServicesAvailable != NULL)
    {
        registration->subsystem->onAllServicesAvailable();
    }
}

void subsystemManagerAllServicesAvailable(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    g_hash_table_foreach(subsystems, notifyAllServicesAvailableCallback, NULL);
}

/**
 * @brief Callback function for notifying subsystems to restore their configuration.
 *
 * Used with g_hash_table_foreach() to notify each registered subsystem to
 * restore its configuration after a restore operation.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void notifyPostRestoreConfigCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    if (registration->subsystem->onPostRestoreConfig != NULL)
    {
        registration->subsystem->onPostRestoreConfig();
    }
}

void subsystemManagerPostRestoreConfig(void)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    g_hash_table_foreach(subsystems, notifyPostRestoreConfigCallback, NULL);
}

/**
 * @brief Callback function for setting OTA upgrade delay on subsystems.
 *
 * Used with g_hash_table_foreach() to set the OTA upgrade delay on each
 * registered subsystem that supports this operation.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data uint32_t value cast to gpointer containing the delay in seconds
 */
static void setOtaUpgradeDelayCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    uint32_t delaySeconds = GPOINTER_TO_UINT(user_data);

    if (registration->subsystem->setOtaUpgradeDelay)
    {
        icLogDebug(LOG_TAG,
                   "Setting %s OTA upgrade delay to : %" PRIu32 " seconds",
                   registration->subsystem->name,
                   delaySeconds);

        registration->subsystem->setOtaUpgradeDelay(delaySeconds);
    }
}

void subsystemManagerSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    g_hash_table_foreach(subsystems, setOtaUpgradeDelayCallback, GUINT_TO_POINTER(delaySeconds));
}

/**
 * @brief Callback function for notifying subsystems to enter Low Power Mode.
 *
 * Used with g_hash_table_foreach() to notify each registered subsystem to
 * enter Low Power Mode (LPM) if it supports this operation.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void notifyLPMStartCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    if (registration->subsystem->onLPMStart)
    {
        registration->subsystem->onLPMStart();
    }
}

void subsystemManagerEnterLPM(void)
{
    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    g_hash_table_foreach(subsystems, notifyLPMStartCallback, NULL);
}

/**
 * @brief Callback function for notifying subsystems to exit Low Power Mode.
 *
 * Used with g_hash_table_foreach() to notify each registered subsystem to
 * exit Low Power Mode (LPM) if it supports this operation.
 *
 * @param key Hash table key (subsystem name)
 * @param value Hash table value (SubsystemRegistration pointer)
 * @param user_data Unused parameter (required by GHashTableFunc signature)
 */
static void notifyLPMEndCallback(gpointer key, gpointer value, gpointer user_data)
{
    g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
    g_return_if_fail(registration != NULL);

    if (registration->subsystem->onLPMEnd != NULL)
    {
        registration->subsystem->onLPMEnd();
    }
}

void subsystemManagerExitLPM(void)
{
    READ_LOCK_SCOPE(mutex);

    g_return_if_fail(subsystems != NULL);

    g_hash_table_foreach(subsystems, notifyLPMEndCallback, NULL);
}

/*
 * Check if a specific subsystem is ready for devices
 */
bool subsystemManagerIsSubsystemReady(const char *subsystem)
{
    bool result = false;

    READ_LOCK_SCOPE(mutex);

    g_return_val_if_fail(subsystems != NULL, false);

    g_autoptr(SubsystemRegistration) reg = subsystemRegistrationAcquire(g_hash_table_lookup(subsystems, subsystem));
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

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, subsystems);

    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);
        if (registration != NULL)
        {
            mutexLock(&registration->mtx);
            bool ready = registration->ready;
            mutexUnlock(&registration->mtx);

            if (!ready)
            {
                icInfo("Subsystem %s is not yet ready", registration->subsystem->name);
                allReady = false;
                break;
            }
        }
    }

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

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, subsystems);

    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        g_autoptr(SubsystemRegistration) registration = subsystemRegistrationAcquire((SubsystemRegistration *) value);

        if (registration && registration->subsystem->onRestoreConfig)
        {
            result &= registration->subsystem->onRestoreConfig(tempRestoreDir, dynamicConfigPath);

            if (!result)
            {
                icWarn("failed for %s!", registration->subsystem->name);
            }
        }
    }

    return result;
}
