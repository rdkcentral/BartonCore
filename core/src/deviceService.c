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

//
// Created by Thomas Lea on 7/28/15.
//

#define LOG_TAG      "deviceService"
#define G_LOG_DOMAIN LOG_TAG

#include "device-service-client.h"
#include "deviceDescriptor.h"
#include "devicePrivateProperties.h"
#include "deviceServiceConfiguration.h"
#include "icConfig/storage.h"
#include "provider/device-service-property-provider.h"
#include <curl/curl.h>
#include <glib.h>
#include <inttypes.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <sys/time.h>

#include "deviceDescriptor.h"
#include "deviceServiceCommFail.h"
#include "deviceStorageMonitor.h"
#include "event/deviceEventHandler.h"
#include "events/device-service-storage-changed-event.h"
#include <cjson/cJSON.h>
#include <commonDeviceDefs.h>
#include <database/jsonDatabase.h>
#include <device/icDeviceMetadata.h>
#include <device/icDeviceResource.h>
#include <deviceDescriptors.h>
#include <deviceHelper.h>
#include <deviceService.h>
#include <deviceServiceStatus.h>
#include <icConcurrent/delayedTask.h>
#include <icConcurrent/threadPool.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <resourceTypes.h>

#ifdef BARTON_CONFIG_ZIGBEE
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include "subsystems/zigbee/zigbeeEventTracker.h"
#endif

#ifdef BARTON_CONFIG_MATTER
#include <subsystems/matter/matterSubsystem.h>
#endif

#include "device-driver/device-driver.h"
#include "device/deviceModelHelper.h"
#include "deviceCommunicationWatchdog.h"
#include "deviceDescriptorHandler.h"
#include "deviceDiscoveryFilters.h"
#include "deviceDriverManager.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include "deviceServicePrivate.h"
#include "event/deviceEventProducer.h"
#include "icUtil/systemCommandUtils.h"
#include "subsystemManager.h"

#define logFmt(fmt) "%s: " fmt, __func__
#include <icLog/logging.h>

#define DEVICE_DESCRIPTOR_BYPASS_SYSTEM_PROP       "deviceDescriptorBypass"

#define SHOULD_NOT_PERSIST_AFTER_RMA_METADATA_NAME "shouldNotPersistAfterRMA"

#define CONFIG_CHANGE_MAX_INTERVAL_MS              5000

#define DEFAULT_COMM_FAIL_MINS                     56

static bool isDeviceServiceInLPM = false;

static pthread_mutex_t lowPowerModeMutex = PTHREAD_MUTEX_INITIALIZER;

char *deviceServiceConfigDir = NULL; // non-static with namespace friendly name so it can be altered by test code

static pthread_mutex_t deviceServiceClientMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static BDeviceServiceClient *client = NULL;

/* Private functions */
static DeviceDriver *getDeviceDriverForUri(const char *uri);

static bool finalizeNewDevice(icDevice *device, bool sendEvents, bool inRepairMode);

static void subsystemManagerReadyForDevicesCallback();

static void deviceDescriptorsReadyForPairingCallback(void);

static void descriptorsUpdatedCallback(void);

static bool deviceDescriptorsListIsReady(void);

static void scheduleDeviceDescriptorsProcessingTask(void);

static icLinkedList *deviceServiceGetResourcesByUriPatternInternal(const char *uriPattern);

static icDeviceResource *deviceServiceGetResourceByIdInternal(const char *deviceUuid,
                                                              const char *endpointId,
                                                              const char *resourceId,
                                                              bool logDebug);

static void denylistDevice(const char *uuid);

static bool checkDeviceDiscoveryFilters(icDevice *device, icInitialResourceValues *initialValues);

static void markDeviceRejected(const char *deviceUuid);

static void clearRejectedDevices(void);

static void markDevicePairingFailed(const char *deviceUuid);

static void clearPairingFailedDevices(void);

static bool updateDataOfRecoveredDevice(icDevice *device);

static void destroyReconfigureDeviceContext(ReconfigureDeviceContext *ctx);

static bool deviceServiceSetReconfigureDeviceContext(ReconfigureDeviceContext *ctx);

static ReconfigureDeviceContext *deviceServiceGetReconfigureDeviceContext(const char *deviceUuid);

static bool deviceServiceRemoveReconfigureDeviceContext(ReconfigureDeviceContext *ctx);

static void cancelPendingReconfigurationTasks();

static bool sendReconfigurationSignal(ReconfigureDeviceContext *ctx, bool shouldTerminate);

static void waitForPendingReconfigurationsToComplete();

static void pendingReconfigurationFreeFunc(void *key, void *value);

static void reconfigureDeviceTask(void *arg);

/* Private data */
typedef struct
{
    char *deviceClass;
    uint16_t timeoutSeconds;
    bool findOrphanedDevices;
    icLinkedList *filters;

    // used for shutdown delay and/or canceling
    bool useMonotonic;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
} discoverDeviceClassContext;

struct ReconfigureDeviceContext
{
    char *deviceUuid;
    uint32_t taskHandle;     // reconfiguration task handle
    pthread_mutex_t mtx;     // mutex for condition variable not the whole context
    pthread_cond_t cond;     // condition variable to wait for reconfiguration signal
    bool isSignaled;         // boolean predicate for conditional variable
    bool isAllowedAsap;      // indicates if reconfiguration allowed as soon as possible
    bool shouldTerminate;    // set to true only while signaling waiting reconfiguration tasks to terminate
    uint32_t timeoutSeconds; // timeout to wait for reconfiguration signal
    reconfigurationCompleteCallback
        reconfigurationCompleted; // optional callback, triggered once reconfiguration task completes
};

static inline void releaseReconfigureDeviceContext(ReconfigureDeviceContext *ctx);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(ReconfigureDeviceContext, releaseReconfigureDeviceContext);

static pthread_mutex_t discoveryControlMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static icHashMap *activeDiscoveries = NULL;
#define RECONFIGURATION_TIMEOUT_SEC            (30 * 60)
#define PENDING_RECONFIGURATION_TIMEOUT_SEC    (1 * 60)
#define MAX_PENDING_RECONFIGURATION_WAIT_COUNT 2
static pthread_mutex_t reconfigurationControlMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static pthread_cond_t reconfigurationControlCond = PTHREAD_COND_INITIALIZER;
static icHashMap *pendingReconfiguration = NULL;
static uint16_t discoveryTimeoutSeconds = 0;

static discoverDeviceClassContext *startDiscoveryForDeviceClass(const char *deviceClass,
                                                                icLinkedList *filters,
                                                                uint16_t timeoutSeconds,
                                                                bool findOrphanedDevices);

static bool deviceServiceIsUriAccessible(const char *uri);

static pthread_mutex_t subsystemsAreReadyMutex = PTHREAD_MUTEX_INITIALIZER;

static bool subsystemsReadyForDevices = false;

static bool isDeviceDescriptorsListReady = false;

static pthread_mutex_t deviceDescriptorsListLatestMutex = PTHREAD_MUTEX_INITIALIZER;

// set of UUIDs that are not fully added yet but have been marked to be removed.
static icHashMap *markedForRemoval = NULL;

static pthread_mutex_t markedForRemovalMutex = PTHREAD_MUTEX_INITIALIZER;

// set of UUIDs that were rejected.
static icHashMap *rejected = NULL;

static pthread_mutex_t rejectedMutex = PTHREAD_MUTEX_INITIALIZER;

// set of UUIDs that were unsuccessful in pairing
static icHashMap *failedToPair = NULL;

static pthread_mutex_t failedToPairMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static icThreadPool *deviceInitializerThreadPool = NULL;
#define MAX_DEVICE_SYNC_THREADS 5
#define MAX_DEVICE_SYNC_QUEUE   128
static void startDeviceInitialization(void);

// 1 more minute than we allow for a legacy sensor to upgrade
#define MAX_DRIVERS_SHUTDOWN_SECS (31 * 60)
static void shutdownDeviceDriverManager();

static pthread_mutex_t deviceDriverManagerShutdownMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t deviceDriverManagerShutdownCond = PTHREAD_COND_INITIALIZER;

/* Atomic: g_atomic_int_set/get */
static gboolean shuttingDown;

#define DEVICE_DESCRIPTOR_PROCESSOR_DELAY_SECS 30
static uint32_t deviceDescriptorProcessorTask = 0;
static pthread_mutex_t deviceDescriptorProcessorMtx = PTHREAD_MUTEX_INITIALIZER;

static void deviceServiceScrubDevices(void)
{
    // Do house keeping on devices.  Right now the only check is for any devices with all endpoints disabled,
    // in which case we remove the device
    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iter = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iter) == true)
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iter);

        bool hasEnabledEndpoint = false;
        icLinkedListIterator *endpointIter = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(endpointIter) == true)
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(endpointIter);
            if (endpoint->enabled == true)
            {
                hasEnabledEndpoint = true;
                break;
            }
        }
        linkedListIteratorDestroy(endpointIter);

        if (hasEnabledEndpoint == false)
        {
            icLogInfo(LOG_TAG, "Device %s has no enabled endpoints when loaded, removing device", device->uuid);
            jsonDatabaseRemoveDeviceById(device->uuid);
        }
    }
    linkedListIteratorDestroy(iter);

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

bool deviceServiceRestoreConfig(const char *tempRestoreDir)
{
    g_autofree gchar *storageDir = deviceServiceConfigurationGetStorageDir();

    scoped_icLinkedListNofree *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    scoped_icLinkedListIterator *driverPreRestoreIterator = linkedListIteratorCreate(deviceDrivers);

    // Let any drivers do anything they need to do pre config
    //
    while (linkedListIteratorHasNext(driverPreRestoreIterator))
    {
        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverPreRestoreIterator);
        if (driver->preRestoreConfig != NULL)
        {
            icLogDebug(
                LOG_TAG, "%s: performing pre restore config actions for driver %s", __FUNCTION__, driver->driverName);
            driver->preRestoreConfig(driver->callbackContext);
        }
    }

    if (jsonDatabaseRestore(tempRestoreDir))
    {
        deviceServiceScrubDevices();
    }
    else
    {
        icLogError(LOG_TAG, "Failed to restore json database!");
        return false;
    }

    // Let any subsystems do their thing
    if (subsystemManagerRestoreConfig(tempRestoreDir, storageDir) == false)
    {
        icLogError(LOG_TAG, "Failed to restore subsystem config");
        return false;
    }

    scoped_icLinkedListIterator *driverRestoreIterator = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(driverRestoreIterator))
    {
        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverRestoreIterator);
        if (driver->restoreConfig != NULL)
        {
            if (!driver->restoreConfig(driver->callbackContext, tempRestoreDir, storageDir))
            {
                icLogError(LOG_TAG, "Failed to restore config for driver %s", driver->driverName);
                return false;
            }
        }
    }

    // ----------------------------------------------------------------
    // get list of devices and loop though them to check 'shouldNotPersistAfterRMA' metadata
    // ----------------------------------------------------------------
    scoped_icDeviceList *deviceList = deviceServiceGetAllDevices();
    scoped_icLinkedListIterator *deviceIterator = linkedListIteratorCreate(deviceList);

    while (linkedListIteratorHasNext(deviceIterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(deviceIterator);

        if (device->uuid == NULL || device->deviceClass == NULL)
        {
            icLogWarn(LOG_TAG,
                      "%s: unable to use device with uuid=%s and deviceClass=%s",
                      __FUNCTION__,
                      stringCoalesce(device->uuid),
                      stringCoalesce(device->deviceClass));
            continue;
        }

        bool denylistDeviceAfterRma =
            getBooleanMetadata(device->uuid, NULL, SHOULD_NOT_PERSIST_AFTER_RMA_METADATA_NAME);

#ifndef BARTON_CONFIG_M1LTE
        // If M1 driver is not supported on this platform, then delete device and denylist
        if (stringCompare(device->deviceClass, M1LTE_DC, false) == 0)
        {
            denylistDeviceAfterRma = true;
        }
#endif

        if (denylistDeviceAfterRma)
        {
            denylistDevice(device->uuid);
        }
    }

    subsystemManagerPostRestoreConfig();

    scoped_icLinkedListIterator *driverPostRestoreIterator = linkedListIteratorCreate(deviceDrivers);

    // Let any drivers do anything they need to do post config
    //
    while (linkedListIteratorHasNext(driverPostRestoreIterator))
    {
        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverPostRestoreIterator);
        if (driver->postRestoreConfig != NULL)
        {
            icLogDebug(
                LOG_TAG, "%s: performing post restore config actions for driver %s", __FUNCTION__, driver->driverName);
            driver->postRestoreConfig(driver->callbackContext);
        }
    }

    return true;
}

// return a list of drivers appropriate for discovery considering system state.
static icLinkedList *getDriversForDiscovery(const char *deviceClass)
{
    icLinkedList *drivers = deviceDriverManagerGetDeviceDriversByDeviceClass(deviceClass);

    // if we are not fully ready for device operation, remove all drivers that are not tagged for neverReject
    if (!deviceServiceIsReadyForDeviceOperation())
    {
        scoped_icLinkedListIterator *it = linkedListIteratorCreate(drivers);

        while (linkedListIteratorHasNext(it))
        {
            DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(it);

            if (!driver->neverReject)
            {
                icInfo("Removing rejectable driver %s from discovery since device service is not ready yet",
                       driver->driverName);
                linkedListIteratorDeleteCurrent(it, standardDoNotFreeFunc);
            }
        }
    }

    return drivers;
}

/*
 * assume 'deviceClasses' is a list of strings
 */
bool deviceServiceDiscoverStart(icLinkedList *deviceClasses,
                                icLinkedList *filters,
                                uint16_t timeoutSeconds,
                                bool findOrphanedDevices)
{
    bool result = deviceClasses != NULL;

    icLogDebug(LOG_TAG, "deviceServiceDiscoverStart");

    if (deviceClasses != NULL)
    {
        mutexLock(&discoveryControlMutex);

        discoveryTimeoutSeconds = timeoutSeconds;

        icLinkedList *newDeviceClassDiscoveries = linkedListCreate();

        icLinkedListIterator *iterator = linkedListIteratorCreate(deviceClasses);
        while (linkedListIteratorHasNext(iterator) && result)
        {
            char *deviceClass = linkedListIteratorGetNext(iterator);

            // ensure we arent already discovering for this device class
            if (hashMapGet(activeDiscoveries, deviceClass, (uint16_t) (strlen(deviceClass) + 1)) != NULL)
            {
                icLogWarn(
                    LOG_TAG,
                    "deviceServiceDiscoverStart: asked to start discovery for device class %s which is already running",
                    deviceClass);
                continue;
            }

            icLinkedList *drivers = getDriversForDiscovery(deviceClass);

            // Indicate OK only when all device classes have at least one supported driver
            if (drivers != NULL && linkedListCount(drivers) > 0)
            {
                if (findOrphanedDevices == true)
                {
                    int recoveryDriversFound = 0;
                    icLinkedListIterator *driverIterator = linkedListIteratorCreate(drivers);
                    while (linkedListIteratorHasNext(driverIterator) == true)
                    {
                        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverIterator);
                        if (driver->recoverDevices != NULL)
                        {
                            recoveryDriversFound++;
                        }
                        else
                        {
                            icLogWarn(LOG_TAG, "driver %s does not support device recovery", driver->driverName);
                        }
                    }
                    linkedListIteratorDestroy(driverIterator);
                    if (recoveryDriversFound == 0)
                    {
                        result = false;
                    }
                }

                if (result == true)
                {
                    // if allowlist is missing or not the latest due to download failure,
                    // skip discovery of deviceClass with all drivers having `neverReject` equal to false.
                    // If any of the drivers has `neverReject` equals to true, we continue with publishing discovery
                    // event but drivers with `neverReject` equals to true only would be discovered (handled in
                    // discoverDeviceClassThreadProc)
                    //
                    bool shouldStartDiscoveryForDeviceClass = false;
                    if (deviceDescriptorsListIsReady() == false)
                    {
                        icLogWarn(LOG_TAG, "discover start called with allowlist in bad state");

                        icLinkedListIterator *driverIterator = linkedListIteratorCreate(drivers);
                        while (linkedListIteratorHasNext(driverIterator) == true)
                        {
                            const DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverIterator);
                            if (driver->neverReject == true)
                            {
                                shouldStartDiscoveryForDeviceClass = true;
                                icLogInfo(LOG_TAG,
                                          "discovery of devices for driver %s can not be rejected",
                                          driver->driverName);
                            }
                        }
                        linkedListIteratorDestroy(driverIterator);
                    }
                    else
                    {
                        shouldStartDiscoveryForDeviceClass = true;
                    }

                    if (shouldStartDiscoveryForDeviceClass == true)
                    {
                        linkedListAppend(newDeviceClassDiscoveries, deviceClass);
                    }
                    else
                    {
                        // we have Allowlist in bad state and discovery for all devices in a class
                        // can be rejected. Report it as failure for discovery request.
                        //
                        icLogWarn(
                            LOG_TAG, "discovery for %s cannot be started with allowlist in bad state", deviceClass);
                        result = false;
                    }
                }
            }
            else
            {
                icWarn("No drivers available for discovery.  Is device service ready?");
                result = false;
            }

            linkedListDestroy(drivers, standardDoNotFreeFunc);
        }
        linkedListIteratorDestroy(iterator);

        result = result && linkedListCount(newDeviceClassDiscoveries) != 0;
        if (result)
        {
            // provide a full list of active discoveries. due to timing, this event is sent before the new ones are
            // added to the activeDiscoveries map so we have to merge those two lists
            icLinkedList *deviceClassesForEvent = linkedListClone(newDeviceClassDiscoveries);
            sbIcHashMapIterator *activeDiscoveriesIt = hashMapIteratorCreate(activeDiscoveries);
            while (hashMapIteratorHasNext(activeDiscoveriesIt) == true)
            {
                char *key = NULL;
                uint16_t keyLen = 0;
                void *value = NULL;
                hashMapIteratorGetNext(activeDiscoveriesIt, (void **) &key, &keyLen, &value);
                linkedListAppend(deviceClassesForEvent, strdup(key));
            }
            sendDiscoveryStartedEvent(deviceClassesForEvent, timeoutSeconds, findOrphanedDevices);
            linkedListDestroy(deviceClassesForEvent, NULL);

            iterator = linkedListIteratorCreate(newDeviceClassDiscoveries);
            while (linkedListIteratorHasNext(iterator))
            {
                const char *deviceClass = linkedListIteratorGetNext(iterator);

                discoverDeviceClassContext *ctx =
                    startDiscoveryForDeviceClass(deviceClass, filters, timeoutSeconds, findOrphanedDevices);
                hashMapPut(activeDiscoveries, ctx->deviceClass, (uint16_t) (strlen(deviceClass) + 1), ctx);
            }
            linkedListIteratorDestroy(iterator);
        }

        linkedListDestroy(newDeviceClassDiscoveries, standardDoNotFreeFunc);

        mutexUnlock(&discoveryControlMutex);
    }

    return result;
}

bool deviceServiceDiscoverStop(icLinkedList *deviceClasses)
{
    mutexLock(&discoveryControlMutex);

    if (deviceClasses == NULL) // stop all active discovery
    {
        icLogDebug(LOG_TAG, "deviceServiceDiscoverStop: stopping all active discoveries");

        icHashMapIterator *iterator = hashMapIteratorCreate(activeDiscoveries);
        while (hashMapIteratorHasNext(iterator))
        {
            char *class;
            uint16_t classLen;
            discoverDeviceClassContext *ctx;
            if (hashMapIteratorGetNext(iterator, (void **) &class, &classLen, (void **) &ctx))
            {
                icLogDebug(LOG_TAG, "deviceServiceDiscoverStop: sending stop signal for device class %s", class);
                pthread_cond_signal(&ctx->cond);
            }
        }
        hashMapIteratorDestroy(iterator);
    }
    else
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(deviceClasses);
        while (linkedListIteratorHasNext(iterator))
        {
            char *item = (char *) linkedListIteratorGetNext(iterator);

            icLogDebug(LOG_TAG, "deviceServiceDiscoverStop: stopping discovery for device class %s", item);
            discoverDeviceClassContext *ctx =
                (discoverDeviceClassContext *) hashMapGet(activeDiscoveries, item, (uint16_t) (strlen(item) + 1));
            if (ctx != NULL)
            {
                pthread_cond_signal(&ctx->cond);
            }
        }
        linkedListIteratorDestroy(iterator);
    }

    mutexUnlock(&discoveryControlMutex);

    // The event will get sent when the discovery actually stops

    return true;
}

#ifdef BARTON_CONFIG_MATTER
struct OnboardMatterDeviceArgs
{
    union
    {
        char *setupPayload;
        uint64_t nodeId;
    };
    uint16_t timeoutSeconds;
};

struct commissionMatterDeviceArgs
{
    char *setupPayload;
    uint16_t timeoutSeconds;
};

static void *commissionMatterDeviceFunc(void *args)
{
    struct commissionMatterDeviceArgs *cmda = (struct commissionMatterDeviceArgs *) args;

    matterSubsystemCommissionDevice(cmda->setupPayload, cmda->timeoutSeconds);

    free(cmda->setupPayload);
    free(cmda);
    return NULL;
}

static void *pairMatterDeviceFunc(void *args)
{
    struct OnboardMatterDeviceArgs *cmda = (struct OnboardMatterDeviceArgs *) args;
    matterSubsystemPairDevice(cmda->nodeId, cmda->timeoutSeconds);
    free(cmda);

    return NULL;
}
#endif

bool deviceServiceCommissionDevice(const char *setupPayload, uint16_t timeoutSeconds)
{
    bool result = false;

    // for now, this is a Matter only operation done in the background
#ifdef BARTON_CONFIG_MATTER
    struct commissionMatterDeviceArgs *args = calloc(1, sizeof(*args));
    args->setupPayload = strdup(setupPayload);
    args->timeoutSeconds = timeoutSeconds;

    result = createDetachedThread(commissionMatterDeviceFunc, args, "MatterCommDev");
    if (!result)
    {
        icLogError(LOG_TAG, "Unable to create matter commissioning thread");
        free(args->setupPayload);
        free(args);
    }
#endif

    return result;
}

bool deviceServiceAddMatterDevice(uint64_t nodeId, uint16_t timeoutSeconds)
{
    bool result = false;
    // Since we don’t block Matter devices with the DDL, this requirement is only here to ensure we have a chance to
    // customize device configuration at pairing time
    if (deviceDescriptorsListIsReady() == false)
    {
        icLogWarn(LOG_TAG, "add matter device called with allowlist in bad state");
        return false;
    }

#ifdef BARTON_CONFIG_MATTER
    struct OnboardMatterDeviceArgs *args = calloc(1, sizeof(*args));
    args->nodeId = nodeId;
    args->timeoutSeconds = timeoutSeconds;

    result = createDetachedThread(pairMatterDeviceFunc, args, "MatterPairDev");
    if (!result)
    {
        icLogError(LOG_TAG, "Unable to create matter pairing thread");
        free(args);
    }
#endif

    return result;
}

bool deviceServiceIsDiscoveryActive()
{
    bool result;
    mutexLock(&discoveryControlMutex);
    result = hashMapCount(activeDiscoveries) > 0;
    mutexUnlock(&discoveryControlMutex);
    return result;
}

bool deviceServiceIsInRecoveryMode()
{
    bool retval = false;
    mutexLock(&discoveryControlMutex);
    icHashMapIterator *iterator = hashMapIteratorCreate(activeDiscoveries);
    if (iterator != NULL)
    {
        while (hashMapIteratorHasNext(iterator) == true)
        {
            void *mapKey;
            void *mapVal;
            uint16_t mapKeyLen = 0;
            if (hashMapIteratorGetNext(iterator, &mapKey, &mapKeyLen, &mapVal) == true)
            {
                discoverDeviceClassContext *ddcCtx = (discoverDeviceClassContext *) mapVal;
                if (ddcCtx != NULL)
                {
                    if (ddcCtx->findOrphanedDevices == true)
                    {
                        retval = true;
                        break;
                    }
                }
            }
        }
        hashMapIteratorDestroy(iterator);
    }
    mutexUnlock(&discoveryControlMutex);
    return retval;
}

bool deviceServiceRemoveDevice(const char *uuid)
{
    bool result = false;

    icDevice *device = jsonDatabaseGetDeviceById(uuid);

    if (device != NULL)
    {
        if (jsonDatabaseRemoveDeviceById(uuid) == false)
        {
            icLogError(LOG_TAG, "Failed to remove device %s", uuid);
            deviceDestroy(device);
            return false;
        }

        icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);
            if (endpoint != NULL && endpoint->enabled == true)
            {
                sendEndpointRemovedEvent(endpoint, device->deviceClass, true);
            }
        }
        linkedListIteratorDestroy(iterator);

        DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
        if (driver != NULL && driver->deviceRemoved != NULL)
        {
            driver->deviceRemoved(driver->callbackContext, device);
        }

        sendDeviceRemovedEvent(device->uuid, device->deviceClass);

        deviceDestroy(device);

        result = true;
    }
    else if (uuid != NULL)
    {
        // It may currently be being added. Add the uuid to the toRemove list
        icLogDebug(LOG_TAG, "Device %s not created yet, marking for removal", uuid);
        pthread_mutex_lock(&markedForRemovalMutex);
        char *key = strdup(uuid);
        uint16_t keyLen = (uint16_t) strlen(key) + 1;
        int *val = calloc(1, sizeof(int)); // Not used
        hashMapPut(markedForRemoval, key, keyLen, val);
        pthread_mutex_unlock(&markedForRemovalMutex);
        result = true;
    }

    return result;
}

/*
 * Checks if provided uri of endpoint/resource/metadata is accessible i.e. resolves to an
 * endpoint that has not been disabled.
 */
static bool deviceServiceIsUriAccessible(const char *uri)
{
    bool retVal = false;

    if (uri == NULL)
    {
        icLogWarn(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return retVal;
    }

    if ((retVal = jsonDatabaseIsUriAccessible(uri)) == false)
    {
        icLogTrace(LOG_TAG, "%s: URI %s is not accessible", __FUNCTION__, uri);
    }

    return retVal;
}

icDeviceResource *deviceServiceGetResourceByUri(const char *uri)
{
    icDeviceResource *result = NULL;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return NULL;
    }

    if (deviceServiceIsUriAccessible(uri) == false)
    {
        icLogWarn(LOG_TAG, "%s: resource %s is not accessible", __FUNCTION__, uri);
        return NULL;
    }

    result = jsonDatabaseGetResourceByUri(uri);
    if (result == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                result = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (result == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return NULL;
    }

    // go to the driver if we are not supposed to cache it and it is readable.  If it is always
    // cached, then the resource should always have the value.  This means resources should not
    // be created with NULL values, unless the driver is going to populate it on its own later
    if (result->cachingPolicy == CACHING_POLICY_NEVER && result->mode & RESOURCE_MODE_READABLE)
    {
        // gotta go to the device driver
        DeviceDriver *driver = getDeviceDriverForUri(uri);

        if (driver == NULL)
        {
            icLogError(LOG_TAG, "Could not find device driver for URI %s", uri);
            resourceDestroy(result);
            return NULL;
        }
        else if (driver->readResource == NULL)
        {
            icLogError(LOG_TAG, "Device driver for URI %s does not support 'read' operation", uri);
            resourceDestroy(result);
            return NULL;
        }

        char *value = NULL;
        if (driver->readResource(driver->callbackContext, result, &value))
        {
            // free the old value before updating with new
            free(result->value);

            result->value = value;
        }
        else
        {
            // the read failed... dont send stale data back to caller
            resourceDestroy(result);
            result = NULL;
        }
    }

    return result;
}

static bool isUriPattern(const char *uri)
{
    // Right now we only support using * as wildcard
    return strchr(uri, '*') != NULL;
}

static char *createRegexFromPattern(const char *uri)
{
    // Figure out how big the regex will be
    int wildcardChars = 0;
    int i = 0;
    for (; uri[i]; ++i)
    {
        if (uri[i] == '*')
        {
            ++wildcardChars;
        }
    }

    // Build the regex, essentially just replacing * with .*
    char *regexUri = (char *) malloc(i + wildcardChars + 1);
    int j = 0;
    for (i = 0; uri[i]; ++i)
    {
        if (uri[i] == '*')
        {
            regexUri[j++] = '.';
        }
        regexUri[j++] = uri[i];
    }

    regexUri[j] = '\0';

    return regexUri;
}

static bool deviceServiceWriteResourceNoPattern(const char *uri, const char *value)
{
    bool result = false;

    icDeviceResource *resource = jsonDatabaseGetResourceByUri(uri);
    if (resource == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                resource = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (resource == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return false;
    }

    if ((resource->mode & RESOURCE_MODE_WRITEABLE) == 0)
    {
        icLogError(LOG_TAG, "Attempt to alter a non-writable resource (%s) rejected.", uri);
        resourceDestroy(resource);
        return false;
    }

    DeviceDriver *driver = getDeviceDriverForUri(uri);
    if (driver == NULL)
    {
        icLogError(LOG_TAG, "Could not find device driver for URI %s", uri);
    }
    else if (driver->writeResource == NULL)
    {
        icLogError(LOG_TAG, "Device driver for URI %s does not support 'write' operations", uri);
    }
    else
    {
        // The device driver's contract states that they will call us back at updateResource if successful
        //  we save the change there
        result = driver->writeResource(driver->callbackContext, resource, resource->value, value);
    }

    resourceDestroy(resource);

    return result;
}

bool deviceServiceWriteResource(const char *uri, const char *value)
{
    bool result = false;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "%s: NULL argument passed to function", __FUNCTION__);
        return false;
    }

    // check if we are dealing with pattern and get resource(s) accordingly
    //
    icLinkedList *resources = deviceServiceGetResourcesByUriPatternInternal(uri);
    if (resources != NULL)
    {
        result = true;
        icLinkedListIterator *iter = linkedListIteratorCreate(resources);
        while (linkedListIteratorHasNext(iter) == true)
        {
            icDeviceResource *item = (icDeviceResource *) linkedListIteratorGetNext(iter);
            // iterate over resources uri and check accessibility
            //
            if (deviceServiceIsUriAccessible(item->uri) == true)
            {
                result &= deviceServiceWriteResourceNoPattern(item->uri, value);
            }
            else
            {
                icLogDebug(LOG_TAG, "%s: %s is not accessible", __FUNCTION__, item->uri);
                result = false;
            }
        }
        linkedListIteratorDestroy(iter);
        linkedListDestroy(resources, (linkedListItemFreeFunc) resourceDestroy);
    }

    return result;
}

bool deviceServiceExecuteResource(const char *uri, const char *arg, char **response)
{
    bool result = false;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "deviceServiceExecuteResource: invalid arguments");
        return false;
    }

    if (deviceServiceIsUriAccessible(uri) == false)
    {
        icLogWarn(LOG_TAG, "%s: resource %s is not accessible", __FUNCTION__, uri);
        return false;
    }

    icDeviceResource *resource = jsonDatabaseGetResourceByUri(uri);
    if (resource == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                resource = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (resource == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return false;
    }

    if ((resource->mode & RESOURCE_MODE_EXECUTABLE) == 0)
    {
        icLogError(LOG_TAG, "Attempt to execute a non-executable resource (%s) rejected.", uri);
        resourceDestroy(resource);
        return false;
    }

    DeviceDriver *driver = getDeviceDriverForUri(uri);
    if (driver == NULL)
    {
        icLogError(LOG_TAG, "Could not find device driver for URI %s", uri);
    }
    else
    {
        if (driver->executeResource != NULL)
        {
            result = driver->executeResource(driver->callbackContext, resource, arg, response);

            if (driver->subsystemName == NULL && result == true)
            {
                // For devices that are not wired up to a subsystem, their dateLastContacted resource will not be
                // updated after successfully executing a resource, so it's done here instead.
                updateDeviceDateLastContacted(resource->deviceUuid);
            }
        }
    }

    resourceDestroy(resource);

    return result;
}

bool deviceServiceChangeResourceMode(const char *uri, uint8_t newMode)
{
    icLogDebug(LOG_TAG, "%s: uri=%s, newMode=%" PRIx8, __FUNCTION__, uri, newMode);

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    if (deviceServiceIsUriAccessible(uri) == false)
    {
        icLogWarn(LOG_TAG, "%s: resource %s is not accessible", __FUNCTION__, uri);
        return false;
    }

    icDeviceResource *resource = jsonDatabaseGetResourceByUri(uri);
    if (resource == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                resource = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (resource == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return false;
    }

    // we do not allow removing the sensitive bit
    if ((resource->mode & RESOURCE_MODE_SENSITIVE) && !(newMode & RESOURCE_MODE_SENSITIVE))
    {
        icLogWarn(LOG_TAG, "%s: attempt to remove sensitive mode rejected", __FUNCTION__);
        resource->mode = (newMode | RESOURCE_MODE_SENSITIVE);
    }
    else
    {
        resource->mode = newMode;
    }

    jsonDatabaseSaveResource(resource);

    sendResourceUpdatedEvent(resource, NULL);

    resourceDestroy(resource);

    return true;
}

icDeviceEndpoint *deviceServiceGetEndpoint(const char *deviceUuid, const char *endpointId)
{
    icDeviceEndpoint *result = NULL;

    if (deviceUuid != NULL && endpointId != NULL)
    {
        result = jsonDatabaseGetEndpointById(deviceUuid, endpointId);

        if (result != NULL && result->enabled == false)
        {
            // we cant return this endpoint...
            endpointDestroy(result);
            result = NULL;
        }
    }

    return result;
}

static bool getDeviceDescriptorCascadeAddDeleteEndpoints(icDevice *device)
{

    bool retVal = false;
    if (device != NULL)
    {
        scoped_DeviceDescriptor *dd = deviceServiceGetDeviceDescriptorForDevice(device);
        if (dd != NULL)
        {
            retVal = dd->cascadeAddDeleteEndpoints;
        }
    }
    return retVal;
}

bool deviceServiceRemoveEndpointById(const char *deviceUuid, const char *endpointId)
{
    icLogDebug(LOG_TAG, "%s: deviceUuid=%s, endpointId=%s", __FUNCTION__, deviceUuid, endpointId);

    bool result = false;
    AUTO_CLEAN(deviceDestroy__auto) icDevice *device = jsonDatabaseGetDeviceById(deviceUuid);
    AUTO_CLEAN(endpointDestroy__auto) icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointById(deviceUuid, endpointId);

    if (endpoint == NULL || device == NULL)
    {
        icLogError(LOG_TAG,
                   "deviceServiceRemoveEndpointById: device/endpoint not found for deviceId %s and endpointId %s",
                   deviceUuid,
                   endpointId);
        return false;
    }

    if (endpoint->enabled == true)
    {
        bool multiEndpointCascadeDelete = getDeviceDescriptorCascadeAddDeleteEndpoints(device);

        if (multiEndpointCascadeDelete)
        {
            icLogInfo(LOG_TAG, "MultiEndpointUnitDeletion (%s)- removing the whole thing.", endpoint->deviceUuid);
            deviceServiceRemoveDevice(endpoint->deviceUuid);
        }
        else
        {
            endpoint->enabled = false;

            // go ahead and save the change to this endpoint now even though we might drop the entire device below
            jsonDatabaseSaveEndpoint(endpoint);

            // check to see if we have any enabled endpoints left and remove the whole device if not
            bool atLeastOneActiveEndpoint = false;

            icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
            while (linkedListIteratorHasNext(iterator))
            {
                icDeviceEndpoint *ep = linkedListIteratorGetNext(iterator);
                if (ep->enabled == true && strcmp(ep->id, endpoint->id) != 0) // dont count the one we just disabled
                {
                    atLeastOneActiveEndpoint = true;
                    break;
                }
            }
            linkedListIteratorDestroy(iterator);
            if (atLeastOneActiveEndpoint == false)
            {
                icLogInfo(LOG_TAG,
                          "No more enabled endpoints exist on this device (%s), removing the whole thing.",
                          endpoint->deviceUuid);
                sendEndpointRemovedEvent(endpoint, device->deviceClass, true);
                deviceServiceRemoveDevice(endpoint->deviceUuid);
            }
            else
            {
                // Let the driver know in case it wants to react
                DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
                if (driver != NULL && driver->endpointDisabled != NULL)
                {
                    driver->endpointDisabled(driver->callbackContext, endpoint);
                }
                sendEndpointRemovedEvent(endpoint, device->deviceClass, false);
            }
        }
        result = true;
    }
    return result;
}

/*
 * Get a device descriptor for a device
 * @param device the device to retrieve the descriptor for
 * @return the device descriptor or NULL if its not found.  Caller is responsible for calling deviceDescriptorFree()
 */
DeviceDescriptor *deviceServiceGetDeviceDescriptorForDevice(icDevice *device)
{
    DeviceDescriptor *dd = NULL;
    icDeviceResource *manufacturer = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_MANUFACTURER);
    icDeviceResource *model = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_MODEL);
    icDeviceResource *hardwareVersion =
        deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION);
    icDeviceResource *firmwareVersion =
        deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if (manufacturer != NULL && model != NULL && firmwareVersion != NULL && hardwareVersion != NULL)
    {
        dd = deviceDescriptorsGet(manufacturer->value, model->value, hardwareVersion->value, firmwareVersion->value);
    }

    return dd;
}

void deviceServiceProcessDeviceDescriptors()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // presume that we are starting out, or just downloaded a new set of device descriptors
    // need to clear out what we had before so that we're forced to re-parse them
    //
    deviceDescriptorsCleanup();

    // also presume that if we're mid-discovery that updating the device descriptors won't
    // necessarily make a previously-rejected device acceptable
    if (deviceServiceIsDiscoveryActive() == false)
    {
        clearRejectedDevices();
        clearPairingFailedDevices();
    }

    // Dont let the device driver manager shutdown while we are processing descriptors
    pthread_mutex_lock(&deviceDriverManagerShutdownMtx);

    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
        DeviceDriver *driver = getDeviceDriverForUri(device->uri);
        if (driver == NULL)
        {
            icLogError(
                LOG_TAG, "deviceServiceProcessDeviceDescriptors: could not find device driver for %s", device->uuid);
            continue;
        }

        if (driver->processDeviceDescriptor == NULL)
        {
            // this driver doesnt support reprocessing device descriptors
            continue;
        }

        DeviceDescriptor *dd = deviceServiceGetDeviceDescriptorForDevice(device);
        if (dd != NULL)
        {
            // forward this descriptor to the device
            // Notice: processDeviceDescriptor doesn't require drivers to
            // update the 'device,' e.g., if metadata changes. Don't count
            // on 'device' being up-to-date after this call.
            driver->processDeviceDescriptor(driver->callbackContext, device, dd);

            deviceDescriptorFree(dd);

            // FIXME: Just do DD metadata -> device metadata here
        }
    }

    /*
     * Metadata updates may have been saved, but not necessarily in the 'device' above.
     * Resetting the global timeout will apply any commFailOverrideSeconds to drivers.
     */

    deviceServiceCommFailSetTimeoutSecs(deviceServiceCommFailGetTimeoutSecs());

    pthread_mutex_unlock(&deviceDriverManagerShutdownMtx);

    // cleanup
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

/****************** LOW POWER MODE FUNCTIONS ***********************/

/*
 * Tells all of the subsystem to enter LPM
 */
void deviceServiceEnterLowPowerMode()
{
    pthread_mutex_lock(&lowPowerModeMutex);

    // since we are already in LPM just bail,
    // nothing to do
    //
    if (isDeviceServiceInLPM == true)
    {
        pthread_mutex_unlock(&lowPowerModeMutex);
        return;
    }

    // now set our LPM flag to true
    //
    isDeviceServiceInLPM = true;
    pthread_mutex_unlock(&lowPowerModeMutex);

    subsystemManagerEnterLPM();

    deviceServiceNotifySystemPowerEvent(POWER_EVENT_LPM_ENTER);
}

/*
 * Tells all of the subsystem to exit LPM
 */
void deviceServiceExitLowPowerMode()
{
    pthread_mutex_lock(&lowPowerModeMutex);

    // since we are already not in LPM just bail,
    // nothing to do
    //
    if (isDeviceServiceInLPM == false)
    {
        pthread_mutex_unlock(&lowPowerModeMutex);
        return;
    }

    // now set our LPM flag to false
    //
    isDeviceServiceInLPM = false;
    pthread_mutex_unlock(&lowPowerModeMutex);

    subsystemManagerExitLPM();

    deviceServiceNotifySystemPowerEvent(POWER_EVENT_LPM_EXIT);
}

/*
 * Determines if system is in LPM
 */
bool deviceServiceIsInLowPowerMode()
{
    pthread_mutex_lock(&lowPowerModeMutex);
    bool retVal = isDeviceServiceInLPM;
    pthread_mutex_unlock(&lowPowerModeMutex);

    return retVal;
}

char *getCurrentTimestampString()
{
    return stringBuilder("%" PRIu64, getCurrentUnixTimeMillis());
}

void updateDeviceDateLastContacted(const char *deviceUuid)
{
    char *dateBuf = getCurrentTimestampString();

    updateResource(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, dateBuf, NULL);

    free(dateBuf);
}

uint64_t getDeviceDateLastContacted(const char *deviceUuid)
{
    uint64_t result = 0;

    icDeviceResource *resource =
        deviceServiceGetResourceById(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED);

    if (resource != NULL)
    {
        result = strtoull(resource->value, NULL, 10);
    }

    return result;
}

/****************** PRIVATE INTERNAL FUNCTIONS ***********************/

/*
 * step 2 of the startup sequence:
 * optional callback notification that occurs when
 * all services are initialized and ready for use.
 * this is triggered by the WATCHDOG_INIT_COMPLETE
 * event.
 */
void allServicesAvailableNotify(void)
{
    icLogDebug(LOG_TAG, "got watchdog event that all services are running");

    // Initialize our device descriptor handling
    //
    deviceServiceDeviceDescriptorsInit(deviceDescriptorsReadyForPairingCallback, descriptorsUpdatedCallback);

    subsystemManagerAllServicesAvailable();
}

/*
 * MT unsafe: access on main()'s thread only
 */
static GMainLoop *eventLoop;
static pthread_t eventThread;

/**
 * This is run by startMainLoop as the glib global default thread context
 */
static void *glibEventRunner(GMainLoop *mainLoop)
{
    g_return_val_if_fail(mainLoop != NULL, NULL);

    g_info("eventLoop start");

    g_main_loop_run(mainLoop);
    g_info("eventLoop quit");

    g_main_loop_unref(mainLoop);

    return NULL;
}

/**
 * Create a thread to handle the default, global main thread context.
 * This is used to support glib signals (and async callbacks) that are wired to
 * objects constructed on any thread.
 * Because our application framework already has a blocking call in main(), a separate thread is
 * created to house the main loop. All signal and async callbacks are invoked
 * on this thread; do not make blocking calls in those callbacks. Prefer to use
 * GTask or other threading techniques to isolate the main loop from "heavy" operations
 * that would otherwise block it.
 *
 * Additional GMainLoops and thread contexts can be created as necessary to
 * segment async; generally the default is all you need, so avoid creating
 * these message queues for no specific reason.
 *
 * For objects that are naturally confined to a thread for their entire life cycle,
 * those threads may push a default context to pin callbacks to their own main loops. Doing so allows threads
 * to receive specific signals/callbacks for objects constructed within them, instead of signaling the global
 * main context.
 * @see https://developer-old.gnome.org/glib/stable/glib-The-Main-Event-Loop.html
 * @see https://developer-old.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#g-main-context-pusher-new
 * @note Only to be invoked on main()'s thread
 */
static void startMainLoop(void)
{
    eventLoop = g_main_loop_new(NULL, FALSE);
    createThread(&eventThread, (taskFunc) glibEventRunner, g_main_loop_ref(eventLoop), "glibEvt");
}

static void stopMainLoop(void)
{
    g_main_loop_quit(eventLoop);
    pthread_join(eventThread, NULL);

    g_main_loop_unref(g_steal_pointer(&eventLoop));
}

bool deviceServiceInitialize(BDeviceServiceClient *service)
{
    g_return_val_if_fail(service != NULL, false);

    g_atomic_int_set(&shuttingDown, FALSE);

    mutexLock(&deviceServiceClientMutex);
    client = g_object_ref(service);
    mutexUnlock(&deviceServiceClientMutex);

    g_autoptr(BDeviceServiceInitializeParamsContainer) params = b_device_service_client_get_initialize_params(service);

    deviceServiceConfigurationStartup(params);

    if (deviceServiceConfigDir == NULL)
    {
        deviceServiceConfigDir = deviceServiceConfigurationGetStorageDir();
    }

#ifdef HAVE_STORAGE_SET_CONFIG_PATH
    storageSetConfigPath(deviceServiceConfigDir);
#endif

    if (!jsonDatabaseInitialize())
    {
        icLogError(LOG_TAG, "Failed to initialize database.");
        return false;
    }
}

// TODO: Document subsys and driver lifecycles, what type of operations are allowed prior to "all drivers starte"
// TODO: Possibly factor out DeviceDriver initialize() functions in favor of self-registration and DeviceDriver
//       callbacks, so DeviceDriver::startup() and DeviceDriver::shutdown() have clearer purpose/consistency.
/**
 * Start up the device service. Drivers and subsystems have a complex lifecycle:
 * 1. Load drivers (driverManagerInitialize) (sync)
 * 2. Init subsystems (sync + async). Subsystems not ready until async readyForDevices callback received
 * 3. Start drivers. This may occur before subsystems are ready for devices. Subsystems MUST be able to support
 *    DeviceDriver::startup immediately after Subsystem::initialize()
 * 4. Async wait for subsystemManager to signal subsystemManagerReadyForDevicesCallback: all drivers started and all
 * subsystem hardware (e.g., Zigbee network) is ready. When there are no subsystems, this is still called after all
 * drivers are started. Once ready for devices, a service status event is emitted to signal clients that they may use
 * devices.
 * 5. Run normal operations (indefinitely)
 * 6. Shutdown subsystems. Subsystems asynchronously become unready
 * 7. Stop device drivers; drivers receive DeviceDriver::shutdown()
 */
bool deviceServiceStart(void)
{
    g_autoptr(BDeviceServiceClient) service = NULL;
    mutexLock(&deviceServiceClientMutex);
    service = g_object_ref(client);
    mutexUnlock(&deviceServiceClientMutex);

    deviceEventProducerStartup(service);

    deviceEventHandlerStartup();

    startMainLoop();

    deviceServiceCommFailInit();

    deviceStorageMonitorStart();

    if (deviceServiceConfigDir != NULL)
    {
        char *allowlistPath = NULL;
        char *denylistPath = NULL;

        allowlistPath = (char *) calloc(1, strlen(deviceServiceConfigDir) + strlen("/AllowList.xml") + 1);
        if (allowlistPath == NULL)
        {
            icLogError(LOG_TAG, "Failed to allocate allowlistPath");
            return false;
        }
        sprintf(allowlistPath, "%s/AllowList.xml", deviceServiceConfigDir);

        denylistPath = (char *) calloc(1, strlen(deviceServiceConfigDir) + strlen("/DenyList.xml") + 1);
        if (denylistPath == NULL)
        {
            icLogError(LOG_TAG, "Failed to allocate denylistPath");
            free(allowlistPath);
            return false;
        }
        sprintf(denylistPath, "%s/DenyList.xml", deviceServiceConfigDir);

        deviceDescriptorsInit(allowlistPath, denylistPath);

        // Clean up any things that should not be
        deviceServiceScrubDevices();

        free(allowlistPath);
        free(denylistPath);
    }

    activeDiscoveries = hashMapCreate();
    markedForRemoval = hashMapCreate();
    rejected = hashMapCreate();
    failedToPair = hashMapCreate();
    mutexLock(&reconfigurationControlMutex);
    pendingReconfiguration = hashMapCreate();
    mutexUnlock(&reconfigurationControlMutex);

    if (deviceDriverManagerInitialize() != true)
    {
        return false;
    }

    deviceInitializerThreadPool = threadPoolCreate("DevInit", 0, MAX_DEVICE_SYNC_THREADS, MAX_DEVICE_SYNC_QUEUE);

    subsystemManagerInitialize(subsystemManagerReadyForDevicesCallback);

#ifdef BARTON_CONFIG_ZIGBEE
    // start event tracker
    //

    // FIXME: make this self-init
    initEventTracker();
#endif

    deviceDriverManagerStartDeviceDrivers();

    subsystemManagerAllDriversStarted();

    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    deviceServiceCommFailSetTimeoutSecs(
        b_device_service_property_provider_get_property_as_uint32(
            propertyProvider, TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY, DEFAULT_COMM_FAIL_MINS) *
        60);
    return true;
}

void deviceServiceShutdown()
{
    icLogDebug(LOG_TAG, "%s: shutting down", __FUNCTION__);

    g_atomic_int_set(&shuttingDown, TRUE);

    deviceEventHandlerShutdown();

    deviceStorageMonitorStop();

    // cleanup
    //
    mutexLock(&subsystemsAreReadyMutex);
    subsystemsReadyForDevices = false;
    mutexUnlock(&subsystemsAreReadyMutex);

    mutexLock(&deviceDescriptorsListLatestMutex);
    isDeviceDescriptorsListReady = false;
    mutexUnlock(&deviceDescriptorsListLatestMutex);

    // cancel any pending reconfiguration tasks and wait for the
    // running ones to complete
    //
    cancelPendingReconfigurationTasks();
    waitForPendingReconfigurationsToComplete();
    mutexLock(&reconfigurationControlMutex);
    hashMapDestroy(pendingReconfiguration, pendingReconfigurationFreeFunc);
    pendingReconfiguration = NULL;
    mutexUnlock(&reconfigurationControlMutex);

    deviceServiceCommFailShutdown();

    icLogInfo(LOG_TAG, "%s: stopping device synchronization", __FUNCTION__);
    threadPoolDestroy(deviceInitializerThreadPool);
    deviceInitializerThreadPool = NULL;

    if (deviceServiceConfigDir != NULL)
    {
        free(deviceServiceConfigDir);
        deviceServiceConfigDir = NULL;
    }

    pthread_mutex_lock(&deviceDescriptorProcessorMtx);
    if (deviceDescriptorProcessorTask != 0)
    {
        cancelDelayTask(deviceDescriptorProcessorTask);
        deviceDescriptorProcessorTask = 0;
    }
    pthread_mutex_unlock(&deviceDescriptorProcessorMtx);

    deviceServiceDeviceDescriptorsDestroy();

    shutdownDeviceDriverManager();
    subsystemManagerShutdown();

#ifdef BARTON_CONFIG_ZIGBEE
    // FIXME: obsolete with self-init
    shutDownEventTracker();
#endif

    deviceEventProducerShutdown();

    jsonDatabaseCleanup(true);

    // FIXME: refcount shared objects to cleanly prevent use-after-free

    mutexLock(&discoveryControlMutex);
    hashMapDestroy(activeDiscoveries, standardDoNotFreeHashMapFunc);
    activeDiscoveries = NULL;
    mutexUnlock(&discoveryControlMutex);

    mutexLock(&markedForRemovalMutex);
    hashMapDestroy(markedForRemoval, NULL);
    markedForRemoval = NULL;
    mutexUnlock(&markedForRemovalMutex);

    mutexLock(&rejectedMutex);
    hashMapDestroy(rejected, NULL);
    rejected = NULL;
    mutexUnlock(&rejectedMutex);

    mutexLock(&failedToPairMutex);
    hashMapDestroy(failedToPair, NULL);
    failedToPair = NULL;
    mutexUnlock(&failedToPairMutex);

    stopMainLoop();

    deviceServiceConfigurationShutdown();

    mutexLock(&deviceServiceClientMutex);
    g_clear_object(&client);
    mutexUnlock(&deviceServiceClientMutex);

    icLogDebug(LOG_TAG, "%s: shutdown complete", __FUNCTION__);
}

bool deviceServiceIsShuttingDown(void)
{
    return g_atomic_int_get(&shuttingDown) == TRUE;
}

static bool fetchCommonResourcesInitialValues(icDevice *device,
                                              const char *manufacturer,
                                              const char *model,
                                              const char *hardwareVersion,
                                              const char *firmwareVersion,
                                              icInitialResourceValues *initialResourceValues)
{
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_MANUFACTURER, manufacturer);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_MODEL, model);
    initialResourceValuesPutDeviceValue(
        initialResourceValues, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION, hardwareVersion);
    initialResourceValuesPutDeviceValue(
        initialResourceValues, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, firmwareVersion);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, NULL);

    AUTO_CLEAN(free_generic__auto) const char *dateBuf = getCurrentTimestampString();

    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_DATE_ADDED, dateBuf);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, dateBuf);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_COMM_FAIL, "false");

    return true;
}

static bool addCommonResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    bool ok = createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_MANUFACTURER,
                                              initialResourceValues,
                                              RESOURCE_TYPE_STRING,
                                              RESOURCE_MODE_READABLE,
                                              CACHING_POLICY_ALWAYS) != NULL;

    ok &= createDeviceResourceIfAvailable(device,
                                          COMMON_DEVICE_RESOURCE_MODEL,
                                          initialResourceValues,
                                          RESOURCE_TYPE_STRING,
                                          RESOURCE_MODE_READABLE | RESOURCE_MODE_EMIT_EVENTS,
                                          CACHING_POLICY_ALWAYS) != NULL;

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_HARDWARE_VERSION,
                                           initialResourceValues,
                                           RESOURCE_TYPE_VERSION,
                                           RESOURCE_MODE_READABLE,
                                           CACHING_POLICY_ALWAYS) != NULL);

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                                           initialResourceValues,
                                           RESOURCE_TYPE_VERSION,
                                           RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                           CACHING_POLICY_ALWAYS) !=
           NULL); // the device driver will update after firmware upgrade

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                                           initialResourceValues,
                                           RESOURCE_TYPE_FIRMWARE_VERSION_STATUS,
                                           RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                           CACHING_POLICY_ALWAYS) !=
           NULL); // the device driver will update as it detects status about any updates

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_DATE_ADDED,
                                           initialResourceValues,
                                           RESOURCE_TYPE_DATETIME,
                                           RESOURCE_MODE_READABLE,
                                           CACHING_POLICY_ALWAYS) != NULL);

    ok &=
        (createDeviceResourceIfAvailable(device,
                                         COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED,
                                         initialResourceValues,
                                         RESOURCE_TYPE_DATETIME,
                                         RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                         CACHING_POLICY_ALWAYS) != NULL);

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_COMM_FAIL,
                                           initialResourceValues,
                                           RESOURCE_TYPE_TROUBLE,
                                           RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                           CACHING_POLICY_ALWAYS) != NULL);

    ok &= (createDeviceResource(device,
                                COMMON_DEVICE_FUNCTION_RESET_TO_FACTORY,
                                NULL,
                                RESOURCE_TYPE_RESET_TO_FACTORY_OPERATION,
                                RESOURCE_MODE_EXECUTABLE,
                                CACHING_POLICY_NEVER) != NULL);

    return ok;
}

/*
 * check the system properties to see if device descriptors are bypassed
 */
static bool isDeviceDescriptorBypassed()
{
    bool retVal = false;

    // get from 'system properties'
    char *flag = NULL;
    if (deviceServiceGetSystemProperty(DEVICE_DESCRIPTOR_BYPASS_SYSTEM_PROP, &flag) == true)
    {
        if (flag != NULL && strcasecmp(flag, "true") == 0)
        {
            retVal = true;
        }
        free(flag);
    }

    return retVal;
}

/*
 * Check the discovery filters with the discovered device values, if it match all the filters then it will proceed for
 * the pairing process.
 */
static bool checkDeviceDiscoveryFilters(icDevice *device, icInitialResourceValues *initialValues)
{
    bool retval = true;

    // FIXME: big lock. ctx should have a refcount.
    LOCK_SCOPE(discoveryControlMutex);

    discoverDeviceClassContext *ctx = (discoverDeviceClassContext *) hashMapGet(
        activeDiscoveries, device->deviceClass, (uint16_t) (strlen(device->deviceClass) + 1));
    if (ctx != NULL && ctx->filters != NULL)
    {
        icLinkedListIterator *deviceFiltersList = linkedListIteratorCreate(ctx->filters);
        while (linkedListIteratorHasNext(deviceFiltersList) == true)
        {
            regex_t uriRegex, valueRegex;
            bool matchFound = false;
            discoveryFilter *filter = (discoveryFilter *) linkedListIteratorGetNext(deviceFiltersList);

            int ret1 = regcomp(&uriRegex, filter->uriPattern, 0);
            int ret2 = regcomp(&valueRegex, filter->valuePattern, 0);
            if ((ret1 == 0) && (ret2 == 0))
            {
                icStringHashMapIterator *deviceResourceList =
                    stringHashMapIteratorCreate((icStringHashMap *) initialValues);
                while (stringHashMapIteratorHasNext(deviceResourceList) == true)
                {
                    char *key;
                    char *value;
                    stringHashMapIteratorGetNext(deviceResourceList, &key, &value);
                    if (key != NULL && value != NULL)
                    {
                        ret1 = regexec(&uriRegex, key, 0, NULL, 0);
                        ret2 = regexec(&valueRegex, value, 0, NULL, 0);
                        if ((ret1 == 0) && (ret2 == 0))
                        {
                            // here we got the matching filter value, so go for the next filter checking.
                            matchFound = true;
                            break;
                        }
                    }
                }
                stringHashMapIteratorDestroy(deviceResourceList);
                if (matchFound == false)
                {
                    retval = false;
                    break;
                }
            }
            else
            {
                icLogDebug(LOG_TAG, "Got invalid regex for searching items %s", filter->uriPattern);

                if (ret1 == 0)
                {
                    regfree(&uriRegex);
                }

                if (ret2 == 0)
                {
                    regfree(&valueRegex);
                }

                retval = false;
                break;
            }
            regfree(&uriRegex);
            regfree(&valueRegex);
        }
        linkedListIteratorDestroy(deviceFiltersList);
    }

    return retval;
}

/*
 * Callback invoked when a device driver finds a device.
 */
bool deviceServiceDeviceFound(DeviceFoundDetails *deviceFoundDetails, bool neverReject)
{
    icLogDebug(LOG_TAG,
               "%s: deviceClass=%s, deviceClassVersion=%u, uuid=%s, manufacturer=%s, model=%s, hardwareVersion=%s, "
               "firmwareVersion=%s",
               __FUNCTION__,
               deviceFoundDetails->deviceClass,
               deviceFoundDetails->deviceClassVersion,
               deviceFoundDetails->deviceUuid,
               deviceFoundDetails->manufacturer,
               deviceFoundDetails->model,
               deviceFoundDetails->hardwareVersion,
               deviceFoundDetails->firmwareVersion);

    char *deviceClass = (char *) deviceFoundDetails->deviceClass;

    bool inRepairMode = false;

    mutexLock(&discoveryControlMutex);

    if (activeDiscoveries != NULL)
    {
        discoverDeviceClassContext *ddcCtx = (discoverDeviceClassContext *) hashMapGet(
            activeDiscoveries, deviceClass, (uint16_t) (strlen(deviceClass) + 1));
        if (ddcCtx != NULL)
        {
            inRepairMode = ddcCtx->findOrphanedDevices;
        }
    }

    mutexUnlock(&discoveryControlMutex);
    if (deviceServiceIsDeviceDenylisted(deviceFoundDetails->deviceUuid) == true)
    {
        icLogWarn(LOG_TAG,
                  "%s: This device's UUID is denylisted!  Rejecting device %s.",
                  __FUNCTION__,
                  deviceFoundDetails->deviceUuid);

        markDeviceRejected(deviceFoundDetails->deviceUuid);
        sendDeviceRejectedEvent(deviceFoundDetails, inRepairMode);

        // tell the device driver that we have rejected this device so it can do any cleanup
        return false;
    }

    bool allowPairing = true;

    // A device has been found.  We now check to see if we will keep or reject it.  If we find a matching device
    //  descriptor, then we keep it.  If we dont find one we will keep it anyway if it is either the XBB battery
    //  backup special device or if device descriptor bypass is enabled (which is used for testing/developement).

    // Attempt to locate the discovered device's descriptor
    AUTO_CLEAN(deviceDescriptorFree__auto)
    DeviceDescriptor *dd = deviceDescriptorsGet(deviceFoundDetails->manufacturer,
                                                deviceFoundDetails->model,
                                                deviceFoundDetails->hardwareVersion,
                                                deviceFoundDetails->firmwareVersion);

    if (dd == NULL)
    {
        if (neverReject)
        {
            icLogDebug(LOG_TAG, "%s: device added with 'neverReject'; allowing device to be paired", __FUNCTION__);
        }
        else if (deviceFoundDetails->deviceMigrator != NULL)
        {
            icLogDebug(LOG_TAG, "%s: device added for migration; allowing device to be paired", __FUNCTION__);
        }
        else if (isDeviceDescriptorBypassed()) // see if the device descriptors are bypassed
        {
            // bypassed, so proceed with the pairing/configuring
            icLogDebug(LOG_TAG, "%s:deviceDescriptorBypass is SET; allowing device to be paired", __FUNCTION__);
        }
        else
        {
            // no device descriptor, no bypass, and not an XBB.  Dont allow pairing
            allowPairing = false;
        }
    }

    if (!allowPairing)
    {
        icLogWarn(LOG_TAG,
                  "%s: No device descriptor found!  Rejecting device %s.",
                  __FUNCTION__,
                  deviceFoundDetails->deviceUuid);

        markDeviceRejected(deviceFoundDetails->deviceUuid);
        sendDeviceRejectedEvent(deviceFoundDetails, inRepairMode);

        // tell the device driver that we have rejected this device so it can do any cleanup
        return false;
    }

    bool pairingSuccessful = true;

    // Create a device instance populated with all required items from the base device class specification
    AUTO_CLEAN(deviceDestroy__auto)
    icDevice *device = createDevice(deviceFoundDetails->deviceUuid,
                                    deviceFoundDetails->deviceClass,
                                    deviceFoundDetails->deviceClassVersion,
                                    deviceFoundDetails->deviceDriver->driverName,
                                    dd);

    ConfigureDeviceFunc configureFunc;
    FetchInitialResourceValuesFunc fetchValuesFunc;
    RegisterResourcesFunc registerFunc;
    DevicePersistedFunc persistedFunc;
    void *ctx;

    bool sendEvents = true;
    if (deviceFoundDetails->deviceMigrator == NULL)
    {
        sendDeviceDiscoveredEvent(deviceFoundDetails, inRepairMode);

        configureFunc = deviceFoundDetails->deviceDriver->configureDevice;
        fetchValuesFunc = deviceFoundDetails->deviceDriver->fetchInitialResourceValues;
        registerFunc = deviceFoundDetails->deviceDriver->registerResources;
        persistedFunc = deviceFoundDetails->deviceDriver->devicePersisted;
        ctx = deviceFoundDetails->deviceDriver->callbackContext;
    }
    else
    {
        configureFunc = deviceFoundDetails->deviceMigrator->configureDevice;
        fetchValuesFunc = deviceFoundDetails->deviceMigrator->fetchInitialResourceValues;
        registerFunc = deviceFoundDetails->deviceMigrator->registerResources;
        persistedFunc = deviceFoundDetails->deviceMigrator->devicePersisted;
        ctx = deviceFoundDetails->deviceMigrator->callbackContext;
        deviceFoundDetails->deviceMigrator->callbackContext->deviceDriver = deviceFoundDetails->deviceDriver;
        sendEvents = false;
    }

    if (sendEvents == true)
    {
        sendDeviceConfigureStartedEvent(deviceFoundDetails->deviceClass, deviceFoundDetails->deviceUuid, inRepairMode);
    }

    uint32_t commFailTimeoutSecs = deviceServiceCommFailGetTimeoutSecs();

    if (dd != NULL)
    {
        const char *commFailTimeoutSecsText =
            deviceDescriptorGetMetadataValue(dd, COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS);
        uint64_t tmp;
        g_autoptr(GError) parseError = NULL;

        if (commFailTimeoutSecsText != NULL)
        {
            g_ascii_string_to_unsigned(commFailTimeoutSecsText, 10, 0, UINT32_MAX, &tmp, &parseError);

            if (parseError == NULL)
            {
                commFailTimeoutSecs = tmp;
            }
            else
            {
                icWarn("Unable to parse '%s' metadata for early device '%s' configuration: %s",
                       COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS,
                       device->uuid,
                       parseError->message);
            }
        }
    }

    deviceServiceCommFailHintDeviceTimeoutSecs(device, commFailTimeoutSecs);

    // here the device descriptor is used for initial configuration, not necessarily full and normal handling
    if (configureFunc != NULL && configureFunc(ctx, device, dd) == false)
    {
        // Note, parts of the deviceFoundDetails may have be freed by this point. For instance, for cameras, some of the
        // details point to the cameraDevice used in configuration above. If configuration fails, that cameraDevice is
        // cleaned up. So by this point, some of deviceFoundDetails (such as uuid) would have been freed already.
        icLogWarn(LOG_TAG, "%s: %s failed to configure.", __FUNCTION__, device->uuid);

        if (sendEvents == true)
        {
            sendDeviceConfigureFailedEvent(
                deviceFoundDetails->deviceClass, deviceFoundDetails->deviceUuid, inRepairMode);
        }
        pairingSuccessful = false;
    }
    else
    {
        if (sendEvents == true)
        {
            sendDeviceConfigureCompletedEvent(
                deviceFoundDetails->deviceClass, deviceFoundDetails->deviceUuid, inRepairMode);
        }
        icInitialResourceValues *initialValues = initialResourceValuesCreate();

        pairingSuccessful = fetchCommonResourcesInitialValues(device,
                                                              deviceFoundDetails->manufacturer,
                                                              deviceFoundDetails->model,
                                                              deviceFoundDetails->hardwareVersion,
                                                              deviceFoundDetails->firmwareVersion,
                                                              initialValues);

        if (pairingSuccessful && fetchValuesFunc != NULL)
        {
            // populate initial resource values
            pairingSuccessful &= fetchValuesFunc(ctx, device, initialValues);

            if (!pairingSuccessful)
            {
                icLogError(LOG_TAG, "%s: %s failed to fetch common resource values", __FUNCTION__, device->uuid);
            }
        }

        if (pairingSuccessful)
        {
            initialResourcesValuesLogValues(initialValues);

            pairingSuccessful = addCommonResources(device, initialValues);
        }
        else
        {
            icLogError(LOG_TAG, "%s: %s failed to fetch initial resource values", __FUNCTION__, device->uuid);
        }

        if (pairingSuccessful)
        {
            // add driver specific resources
            pairingSuccessful &= registerFunc(ctx, device, initialValues);
        }
        else
        {
            icLogError(LOG_TAG, "%s: %s failed to register common resources", __FUNCTION__, device->uuid);
        }

        if (pairingSuccessful)
        {
            // perform device filters checking operation if provided
            pairingSuccessful = checkDeviceDiscoveryFilters(device, initialValues);
        }
        else
        {
            icLogError(LOG_TAG, "%s: failed to register resources for %s", __FUNCTION__, device->uuid);
        }

        initialResourceValuesDestroy(initialValues);

        // after everything is all ready, let regular device descriptor processing take place
        if (pairingSuccessful)
        {
            if (deviceFoundDetails->deviceDriver->processDeviceDescriptor != NULL && dd != NULL)
            {
                pairingSuccessful &= deviceFoundDetails->deviceDriver->processDeviceDescriptor(
                    deviceFoundDetails->deviceDriver->callbackContext, device, dd);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: failed to match device discovery filters for %s", __FUNCTION__, device->uuid);
        }
    }

    if (pairingSuccessful)
    {
        // Before we send the discovery complete event, let's do a final check to see if this device has been marked for
        //  removal
        pthread_mutex_lock(&markedForRemovalMutex);
        bool marked = hashMapDelete(markedForRemoval, device->uuid, (uint16_t) strlen(device->uuid) + 1, NULL);
        pthread_mutex_unlock(&markedForRemovalMutex);

        if (marked == true)
        {
            icLogDebug(LOG_TAG, "Device marked for removal before finishing setup. Not adding...");
            pairingSuccessful = false;
        }
    }

    if (pairingSuccessful)
    {
        // perform any processing to make this device real and accessible now that the device driver is done
        pairingSuccessful = finalizeNewDevice(device, sendEvents, inRepairMode);
    }

    // Finally, if we made it here and are still successful, let everyone know.
    if (pairingSuccessful)
    {
        if (sendEvents)
        {
            // Signal that we finished discovering the device including all its endpoints
            sendDeviceDiscoveryCompletedEvent(device, inRepairMode);
        }

        if (persistedFunc != NULL && inRepairMode == false)
        {
            persistedFunc(ctx, device);
        }

        deviceServiceCommFailSetDeviceTimeoutSecs(device, deviceServiceCommFailGetTimeoutSecs());
    }
    else
    {
        // We need to make sure the managing driver is told to remove any persistent models of the device that they
        // may have (for instance, cameras). Otherwise, they may never be made accessible again in the current process
        // if the driver thinks it already has discovered the device.
        if (deviceFoundDetails->deviceMigrator == NULL && deviceFoundDetails->deviceDriver->deviceRemoved != NULL)
        {
            deviceFoundDetails->deviceDriver->deviceRemoved(deviceFoundDetails->deviceDriver->callbackContext, device);
        }

        if (sendEvents)
        {
            sendDeviceDiscoveryFailedEvent(deviceFoundDetails, inRepairMode);
        }

        markDevicePairingFailed(deviceFoundDetails->deviceUuid);
    }

    return pairingSuccessful;
}

static void setUriOnResource(const char *baseUri, icDeviceResource *resource)
{
    // base uri + "/r/" + id + \0
    free(resource->uri);
    resource->uri = stringBuilder("%s/r/%s", baseUri, resource->id);

    icLogDebug(LOG_TAG, "Created URI: %s", resource->uri);
}

static void setUrisOnResources(const char *baseUri, icLinkedList *resources)
{
    icLinkedListIterator *iterator = linkedListIteratorCreate(resources);
    while (linkedListIteratorHasNext(iterator))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iterator);
        setUriOnResource(baseUri, resource);
    }
    linkedListIteratorDestroy(iterator);
}

static void setUris(icDevice *device)
{
    //  "/" + uuid + \0
    device->uri = (char *) malloc(1 + strlen(device->uuid) + 1);
    sprintf(device->uri, "/%s", device->uuid);

    icLogDebug(LOG_TAG, "Created URI: %s", device->uri);

    setUrisOnResources(device->uri, device->resources);

    icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(iterator))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);

        endpoint->uri = endpointUriCreate(device->uuid, endpoint->id);

        icLogDebug(LOG_TAG, "Created URI: %s", endpoint->uri);
        setUrisOnResources(endpoint->uri, endpoint->resources);
    }
    linkedListIteratorDestroy(iterator);
}

static bool finalizeNewDevice(icDevice *device, bool sendEvents, bool inRepairMode)
{
    bool result = true;

    if (device == NULL)
    {
        icLogError(LOG_TAG, "deviceConfigured: NULL device!");
        return false;
    }

    // populate URIs on the device's tree
    setUris(device);

    // if this device has the timezone resource, set it now
    icDeviceResource *tzResource = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_TIMEZONE);
    if (tzResource != NULL)
    {
        g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        char *posixTZ =
            b_device_service_property_provider_get_property_as_string(propertyProvider, POSIX_TIME_ZONE_PROP, NULL);
        if (posixTZ != NULL)
        {
            deviceServiceWriteResource(tzResource->uri, posixTZ);
            free(posixTZ);
        }
    }

    if (inRepairMode == true)
    {
        // There can be device data (resources & metadata) which may have changed before device is recovered,
        // let everyone know about it.
        result &= updateDataOfRecoveredDevice(device);
    }
    else
    {
        result &= jsonDatabaseAddDevice(device);
    }

    icLogDebug(LOG_TAG, "device finalized:");
    devicePrint(device, "");

    if (sendEvents == true)
    {
        if (inRepairMode == false)
        {
            sendDeviceAddedEvent(device->uuid);

            icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
            while (linkedListIteratorHasNext(iterator))
            {
                icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);
                if (endpoint->enabled == true)
                {
                    sendEndpointAddedEvent(endpoint, device->deviceClass);
                }
            }
            linkedListIteratorDestroy(iterator);
        }
        else
        {
            sendDeviceRecoveredEvent(device->uuid);
        }
    }

    return result;
}

// the endpoint must have already been added to the icDevice
void deviceServiceAddEndpoint(icDevice *device, icDeviceEndpoint *endpoint)
{
    if (device == NULL || endpoint == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    endpoint->uri = endpointUriCreate(device->uuid, endpoint->id);
    icLogDebug(LOG_TAG, "Created URI: %s", endpoint->uri);
    setUrisOnResources(endpoint->uri, endpoint->resources);
    if (jsonDatabaseAddEndpoint(endpoint) == true)
    {
        if (endpoint->enabled == true)
        {
            sendEndpointAddedEvent(endpoint, device->deviceClass);
        }
    }
}

static bool updateDataOfRecoveredDevice(icDevice *device)
{
    if (device == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    // check and update resource values on device
    //
    sbIcLinkedListIterator *dResourceIter = linkedListIteratorCreate(device->resources);
    while (linkedListIteratorHasNext(dResourceIter))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(dResourceIter);
        updateResource(device->uuid, NULL, resource->id, resource->value, NULL);
    }

    // check and update metadata values on device
    //
    sbIcLinkedListIterator *dMetadataIter = linkedListIteratorCreate(device->metadata);
    while (linkedListIteratorHasNext(dMetadataIter))
    {
        icDeviceMetadata *metadata = (icDeviceMetadata *) linkedListIteratorGetNext(dMetadataIter);
        setMetadata(device->uuid, NULL, metadata->id, metadata->value);
    }

    // check and update resource & metadata values on endpoints
    //
    sbIcLinkedListIterator *dEndpointIter = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(dEndpointIter))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(dEndpointIter);

        sbIcLinkedListIterator *eResourceIter = linkedListIteratorCreate(endpoint->resources);
        while (linkedListIteratorHasNext(eResourceIter))
        {
            icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(eResourceIter);
            updateResource(device->uuid, resource->endpointId, resource->id, resource->value, NULL);
        }

        sbIcLinkedListIterator *eMetadataIter = linkedListIteratorCreate(endpoint->metadata);
        while (linkedListIteratorHasNext(eMetadataIter))
        {
            icDeviceMetadata *metadata = (icDeviceMetadata *) linkedListIteratorGetNext(eMetadataIter);
            setMetadata(device->uuid, metadata->endpointId, metadata->id, metadata->value);
        }
    }

    return true;
}

/**
 * Update an endpoint, persist to database and send out events.
 * @param device the device
 * @param endpoint the endpoint
 * @note Currently allows 'enabled' and 'resources' to change
 * FIXME: allow possibly anything to be changed, i.e., just save a valid endpoint
 */
void deviceServiceUpdateEndpoint(icDevice *device, icDeviceEndpoint *endpoint)
{
    if (device == NULL || endpoint == NULL || strcmp(device->uuid, endpoint->deviceUuid) != 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    AUTO_CLEAN(endpointDestroy__auto) icDeviceEndpoint *currentEp = jsonDatabaseGetEndpointByUri(endpoint->uri);
    bool wasEnabled = false;

    if (currentEp != NULL)
    {
        wasEnabled = currentEp->enabled;
    }
    else
    {
        icLogError(LOG_TAG, "Device endpoint %s not found!", endpoint->uri);
        return;
    }

    setUrisOnResources(endpoint->uri, endpoint->resources);
    jsonDatabaseSaveEndpoint(endpoint);

    if (wasEnabled == false && endpoint->enabled == true)
    {
        sendEndpointAddedEvent(endpoint, device->deviceClass);
    }
}

// Used to notify watchers when an resource changes
// Used by device drivers when they need to update the resource for one of their devices
void updateResource(const char *deviceUuid,
                    const char *endpointId,
                    const char *resourceId,
                    const char *newValue,
                    cJSON *metadata)
{
    // dont debug print on frequently updated resource ids to preserve log files
    if (resourceId != NULL && strcmp(COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, resourceId) != 0)
    {
        icLogDebug(LOG_TAG,
                   "%s: deviceUuid=%s, endpointId=%s, resourceId=%s, newValue=%s",
                   __FUNCTION__,
                   deviceUuid,
                   endpointId,
                   resourceId,
                   newValue);
    }

    icDeviceResource *resource = deviceServiceGetResourceByIdInternal(deviceUuid, endpointId, resourceId, false);

    if (resource != NULL)
    {
        bool sendEvent = false;

        if (resource->cachingPolicy == CACHING_POLICY_NEVER && (resource->mode & RESOURCE_MODE_EMIT_EVENTS))
        {
            // we cannot compare previous values for non cached resources, so just stuff what we got in it and
            // send the event
            free(resource->value);
            if (newValue != NULL)
            {
                resource->value = strdup(newValue);
            }
            else
            {
                resource->value = NULL;
            }
            sendEvent = true;

            // Writable resources that are never cached have no dateOfLastSync metadata in the devicedb,
            // so we need to manually update the date of last contact.
            updateDeviceDateLastContacted(deviceUuid);
        }
        else
        {
            bool didChange = false;

            if (resource->value != NULL)
            {
                if (newValue == NULL)
                {
                    // from non-null to null
                    didChange = true;
                    free(resource->value);
                    resource->value = NULL;
                }
                else if (strcmp(resource->value, newValue) != 0)
                {
                    didChange = true;
                    free(resource->value);
                    resource->value = strdup(newValue);
                }
            }
            else if (newValue != NULL)
            {
                // changed from null to not null
                didChange = true;

                // TODO: We should only update for resources marked as READABLE
                resource->value = strdup(newValue);
            }

            if (didChange)
            {
                resource->dateOfLastSyncMillis = getCurrentUnixTimeMillis();

                // the database knows how to honor lazy saves
                jsonDatabaseSaveResource(resource);
            }
            else
            {
                // nothing really changed, but we are in sync so let the database deal with that without persisting now
                jsonDatabaseUpdateDateOfLastSyncMillis(resource);
            }

            if ((resource->mode & RESOURCE_MODE_EMIT_EVENTS) && didChange)
            {
                sendEvent = true;
            }
        }

        if (sendEvent)
        {
            sendResourceUpdatedEvent(resource, metadata);
        }
    }

    resourceDestroy(resource);
}

void setMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *value)
{
    icLogDebug(LOG_TAG,
               "%s: deviceUuid=%s, endpointId=%s, name=%s, value=%s",
               __FUNCTION__,
               deviceUuid,
               endpointId,
               name,
               value);

    if (deviceUuid == NULL || name == NULL || value == NULL)
    {
        icLogWarn(LOG_TAG, "%s invalid args", __func__);
        return;
    }

    // first lets get any previous value and compare.  If they are not different, we dont need to do anything.
    //  Note: we cannot store NULL for a metadata item.  A null result from getMetadata means it wasnt set at all.
    AUTO_CLEAN(free_generic__auto) const char *metadataUri = metadataUriCreate(deviceUuid, endpointId, name);
    icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(metadataUri);
    bool doSave = false;

    if (metadata != NULL)
    {
        if (strcmp(metadata->value, value) != 0)
        {
            free(metadata->value);
            metadata->value = strdup(value);
            doSave = true;
        }
    }
    else
    {
        metadata = (icDeviceMetadata *) calloc(1, sizeof(icDeviceMetadata));
        metadata->deviceUuid = strdup(deviceUuid);
        if (endpointId != NULL)
        {
            metadata->endpointId = strdup(endpointId);
        }
        metadata->id = strdup(name);
        metadata->value = strdup(value);
        metadata->uri = strdup(metadataUri);

        doSave = true;
    }

    if (doSave)
    {
        jsonDatabaseSaveMetadata(metadata);
    }

    metadataDestroy(metadata);
}

/*
 * Caller frees result
 */
char *getMetadata(const char *deviceUuid, const char *endpointId, const char *name)
{
    char *result = NULL;

    if (deviceUuid == NULL || name == NULL)
    {
        icLogWarn(LOG_TAG, "%s invalid args", __func__);
        return NULL;
    }

    scoped_generic char *uri = metadataUriCreate(deviceUuid, endpointId, name);
    icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uri);

    if (metadata != NULL)
    {
        // we cant return the actual pointer that is stored in our hash map, so we strdup it and the caller must free
        if (metadata->value != NULL)
        {
            result = strdup(metadata->value);
        }

        metadataDestroy(metadata);
    }

    return result;
}

bool deviceServiceRemoveMetadata(const char *deviceUuid, const char *endpointId, const char *name)
{
    bool retVal = false;

    if (deviceUuid == NULL || name == NULL)
    {
        icLogWarn(LOG_TAG, "%s invalid args", __func__);
    }

    scoped_generic char *uri = metadataUriCreate(deviceUuid, endpointId, name);
    retVal = jsonDatabaseRemoveMetadataByUri(uri);
    if (retVal == false)
    {
        icLogDebug(LOG_TAG, "%s failed to remove metadata with uri: %s", __func__, uri);
    }

    return retVal;
}

void setBooleanMetadata(const char *deviceUuid, const char *endpointId, const char *name, bool value)
{
    setMetadata(deviceUuid, endpointId, name, value ? "true" : "false");
}

bool getBooleanMetadata(const char *deviceUuid, const char *endpointId, const char *name)
{
    bool result = false;

    char *value = getMetadata(deviceUuid, endpointId, name);

    if (value != NULL && strcmp(value, "true") == 0)
    {
        result = true;
    }

    free(value);
    return result;
}

void setJsonMetadata(const char *deviceUuid, const char *endpointId, const char *name, cJSON *value)
{
    scoped_generic char *stringvalue = cJSON_Print(value);
    setMetadata(deviceUuid, endpointId, name, (const char *) stringvalue);
}

/*
 * Caller frees result
 */
cJSON *getJsonMetadata(const char *deviceUuid, const char *endpointId, const char *name)
{
    cJSON *result = NULL;

    scoped_generic char *value = getMetadata(deviceUuid, endpointId, name);

    if (value != NULL)
    {
        result = cJSON_Parse((const char *) value);
    }

    return result;
}

icLinkedList *deviceServiceGetDevicesBySubsystem(const char *subsystem)
{
    icLinkedList *result = linkedListCreate();

    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = linkedListIteratorGetNext(iterator);
        DeviceDriver *driver = getDeviceDriverForUri(device->uri);
        if (driver != NULL && driver->subsystemName != NULL && strcmp(driver->subsystemName, subsystem) == 0)
        {
            // Take it out of the source list so we can put it in our list
            linkedListIteratorDeleteCurrent(iterator, standardDoNotFreeFunc);
            linkedListAppend(result, device);
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    return result;
}

static DeviceDriver *getDeviceDriverForUri(const char *uri)
{
    DeviceDriver *result = NULL;

    icDevice *device = deviceServiceGetDeviceByUri(uri);
    if (device != NULL)
    {
        result = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
        deviceDestroy(device);
    }
    else
    {
        icLogWarn(LOG_TAG, "getDeviceDriverForUri: did not find device at uri %s", uri);
    }

    return result;
}

bool addNewResource(const char *ownerUri, icDeviceResource *resource)
{
    setUriOnResource(ownerUri, resource);
    return jsonDatabaseAddResource(ownerUri, resource);
}

void deviceServiceSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

#ifndef CONFIG_DEBUG_ZITH_CI_TESTS
    subsystemManagerSetOtaUpgradeDelay(delaySeconds);
#endif /* CONFIG_DEBUG_ZITH_CI_TESTS */
}

bool deviceServiceIsReadyForPairing(void)
{
    return deviceDescriptorsListIsReady();
}

bool deviceServiceIsReadyForDeviceOperation(void)
{
    pthread_mutex_lock(&subsystemsAreReadyMutex);
    bool retVal = subsystemsReadyForDevices;
    pthread_mutex_unlock(&subsystemsAreReadyMutex);

    return retVal;
}

/****************** PRIVATE INTERNAL FUNCTIONS ***********************/

static bool deviceDescriptorsListIsReady(void)
{
    bool retVal = false;

    // Even if ddl has issues, if system property of "deviceDescriptorBypass" is set to true using "ddl bypass"
    // deviceUtil command, it means to act as if ddl is valid regardless of its state. "ddl bypass" is a
    // dev/debug/troubleshooting mode so that the state of the ddl stuff cannot block discovery/pairing.
    //
    if (isDeviceDescriptorBypassed() == true)
    {
        retVal = true;
    }
    else
    {
        pthread_mutex_lock(&deviceDescriptorsListLatestMutex);
        retVal = isDeviceDescriptorsListReady;
        pthread_mutex_unlock(&deviceDescriptorsListLatestMutex);
    }

    return retVal;
}

static void processDeviceDescriptorsBackgroundTask(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&deviceDescriptorProcessorMtx);
    deviceDescriptorProcessorTask = 0;
    pthread_mutex_unlock(&deviceDescriptorProcessorMtx);

    deviceServiceProcessDeviceDescriptors();

    icLogDebug(LOG_TAG, "%s done", __FUNCTION__);
}

static void scheduleDeviceDescriptorsProcessingTask(void)
{
    icLogDebug(LOG_TAG, "%s", __func__);
    pthread_mutex_lock(&deviceDescriptorProcessorMtx);
    if (deviceDescriptorProcessorTask == 0)
    {
        delayUnits delayTimeUnits = DELAY_SECS;

        g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

        if (b_device_service_property_provider_get_property_as_bool(propertyProvider, TEST_FASTTIMERS_PROP, false) ==
            true)
        {
            delayTimeUnits = DELAY_MILLIS;
        }

        deviceDescriptorProcessorTask = scheduleDelayTask(
            DEVICE_DESCRIPTOR_PROCESSOR_DELAY_SECS, delayTimeUnits, processDeviceDescriptorsBackgroundTask, NULL);
    }
    pthread_mutex_unlock(&deviceDescriptorProcessorMtx);
}

static void notifyDriversForServiceStatusChange(void)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    scoped_DeviceServiceStatus *status = deviceServiceGetStatus();

    scoped_icLinkedListNofree *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(iter))
    {
        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(iter);
        if (driver->serviceStatusChanged != NULL)
        {
            driver->serviceStatusChanged(driver->callbackContext, status);
        }
    }

    icLogTrace(LOG_TAG, "%s done", __FUNCTION__);
}

/*
 * It is called only if device descriptors are ready for pairing devices
 */
static void deviceDescriptorsReadyForPairingCallback(void)
{
    icLogDebug(LOG_TAG, "%s device Descriptors are ready", __FUNCTION__);

    pthread_mutex_lock(&deviceDescriptorsListLatestMutex);
    isDeviceDescriptorsListReady = true;
    pthread_mutex_unlock(&deviceDescriptorsListLatestMutex);

    notifyDriversForServiceStatusChange();
    sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonReadyForPairing);
    scheduleDeviceDescriptorsProcessingTask();
}

static void descriptorsUpdatedCallback(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    scheduleDeviceDescriptorsProcessingTask();
}

static void subsystemManagerReadyForDevicesCallback()
{
    // TODO: Currently, we are handling single subsystem ready case i.e. Zigbee and
    // we are not handling de-initialization case of it. That functionality needs
    // to be extended for multiple subsystems and deviceStatus event is to be adjusted
    // accordingly.
    //

    icLogDebug(LOG_TAG, "Subsystem manager is ready for devices");
    pthread_mutex_lock(&subsystemsAreReadyMutex);
    subsystemsReadyForDevices = true;
    pthread_mutex_unlock(&subsystemsAreReadyMutex);

    notifyDriversForServiceStatusChange();
    sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonReadyForDeviceOperation);
    // Load up a background threadpool that will perform any required device initialization
    //
    startDeviceInitialization();
}

// the time zone changed... notify any devices that have the well-known timezone resource
void timeZoneChanged(const char *timeZone)
{
    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = linkedListIteratorGetNext(iterator);
        icDeviceResource *tzResource = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_TIMEZONE);
        if (tzResource != NULL)
        {
            deviceServiceWriteResource(tzResource->uri, timeZone);
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

static void denylistDevice(const char *uuid)
{
    if (stringIsEmpty(uuid) == true)
    {
        icLogError(LOG_TAG, "Uuid is empty ! Nothing to denylist");
        return;
    }

    if (deviceServiceIsDeviceDenylisted(uuid) == true)
    {
        icLogDebug(LOG_TAG, "Device uuid=%s is already in denylist", uuid);
    }
    else
    {
        g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        AUTO_CLEAN(free_generic__auto)
        char *propValue = b_device_service_property_provider_get_property_as_string(
            propertyProvider, CPE_DENYLISTED_DEVICES_PROPERTY_NAME, "");
        AUTO_CLEAN(cJSON_Delete__auto) cJSON *denylistedDevicesArray = cJSON_Parse(propValue);

        if (cJSON_IsArray(denylistedDevicesArray) == false)
        {
            if (cJSON_IsObject(denylistedDevicesArray) == true)
            {
                cJSON_Delete(denylistedDevicesArray);
            }

            icLogDebug(LOG_TAG, "Invalid denylisted devices array in CPE property, creating new array");
            denylistedDevicesArray = cJSON_CreateArray();
        }

        cJSON_AddItemToArray(denylistedDevicesArray, cJSON_CreateString(uuid));

        AUTO_CLEAN(free_generic__auto) char *newPropValue = cJSON_PrintUnformatted(denylistedDevicesArray);
        b_device_service_property_provider_set_property_string(
            propertyProvider, CPE_DENYLISTED_DEVICES_PROPERTY_NAME, newPropValue);

        icLogDebug(LOG_TAG, "Device uuid=%s is now added to denylist", uuid);
    }
}

bool deviceServiceIsDeviceDenylisted(const char *uuid)
{
    bool retVal = false;

    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    AUTO_CLEAN(free_generic__auto)
    char *denylistedDevices = b_device_service_property_provider_get_property_as_string(
        propertyProvider, CPE_DENYLISTED_DEVICES_PROPERTY_NAME, "");

    if (stringIsEmpty(denylistedDevices) == false)
    {
        AUTO_CLEAN(cJSON_Delete__auto) cJSON *denylistedDevicesArray = cJSON_Parse(denylistedDevices);

        if (cJSON_IsArray(denylistedDevicesArray) == true)
        {
            cJSON *denylistedDeviceElem = NULL;
            cJSON_ArrayForEach(denylistedDeviceElem, denylistedDevicesArray)
            {
                if (cJSON_IsString(denylistedDeviceElem))
                {
                    char *denylistedDevice = cJSON_GetStringValue(denylistedDeviceElem);

                    icLogDebug(LOG_TAG, "Device uuid=%s is denylisted. Matching with uuid=%s", denylistedDevice, uuid);

                    if (stringCompare(uuid, denylistedDevice, true) == 0)
                    {
                        retVal = true;
                        break;
                    }
                }
            }
        }
    }

    return retVal;
}

// the denylisted devices property passed as function arg... remove a device if already paired
void processDenylistedDevices(const char *propValue)
{
    if (stringIsEmpty(propValue) == false)
    {
        AUTO_CLEAN(cJSON_Delete__auto) cJSON *denylistedDevicesArray = cJSON_Parse(propValue);

        if (cJSON_IsArray(denylistedDevicesArray) == true)
        {
            cJSON *denylistedDeviceElem = NULL;
            cJSON_ArrayForEach(denylistedDeviceElem, denylistedDevicesArray)
            {
                if (cJSON_IsString(denylistedDeviceElem))
                {
                    char *denylistedDevice = cJSON_GetStringValue(denylistedDeviceElem);

                    icLogDebug(LOG_TAG, "Deleting device uuid=%s because it is denylisted", denylistedDevice);
                    deviceServiceRemoveDevice(denylistedDevice);
                }
            }
        }
    }
}

static void *discoverDeviceClassThreadProc(void *arg)
{
    discoverDeviceClassContext *ctx = (discoverDeviceClassContext *) arg;

    // start discovery for all device drivers that support this device class

    icLinkedList *startedDeviceDrivers = linkedListCreate();

    bool atLeastOneStarted = false;
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDriversByDeviceClass(ctx->deviceClass);
    if (deviceDrivers != NULL)
    {
        icLinkedListIterator *driverIterator = linkedListIteratorCreate(deviceDrivers);
        while (linkedListIteratorHasNext(driverIterator))
        {
            DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverIterator);
            // If allowList is missing or could not be downloaded, only devices with `neverReject` equals
            // to true of a class are to be discovered
            //
            if (driver->neverReject == false && deviceDescriptorsListIsReady() == false)
            {
                icLogInfo(LOG_TAG,
                          "device descriptor list is not latest, skipping discovery for device %s",
                          driver->driverName);
                continue;
            }

            if (ctx->findOrphanedDevices == true)
            {
                if (driver->recoverDevices != NULL)
                {
                    icLogDebug(LOG_TAG, "telling %s to start device recovery...", driver->driverName);
                    bool started = driver->recoverDevices(driver->callbackContext, ctx->deviceClass);

                    if (started == false)
                    {
                        // this is only a warning because other drivers for this class might successfully
                        // start recovery
                        icLogWarn(LOG_TAG, "device driver %s failed to start device recovery", driver->driverName);
                    }
                    else
                    {
                        icLogDebug(LOG_TAG, "device driver %s start device recovery successfully", driver->driverName);
                        atLeastOneStarted = true;
                        linkedListAppend(startedDeviceDrivers, driver);
                    }
                }
                else
                {
                    icLogInfo(LOG_TAG, "device driver %s does not support device recovery", driver->driverName);
                }
            }
            else
            {
                if (driver->discoverDevices != NULL)
                {
                    icLogDebug(LOG_TAG, "telling %s to start discovering...", driver->driverName);
                    bool started = driver->discoverDevices(driver->callbackContext, ctx->deviceClass);
                    if (started == false)
                    {
                        icLogError(LOG_TAG, "device driver %s failed to start discovery", driver->driverName);
                    }
                    else
                    {
                        icLogDebug(LOG_TAG, "device driver %s started discovering successfully", driver->driverName);
                        atLeastOneStarted = true;
                        linkedListAppend(startedDeviceDrivers, driver);
                    }
                }
            }
        }
        linkedListIteratorDestroy(driverIterator);
        linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);
    }

    if (atLeastOneStarted)
    {
        pthread_mutex_lock(&ctx->mtx);
        if (ctx->timeoutSeconds > 0)
        {
            icLogDebug(LOG_TAG,
                       "discoverDeviceClassThreadProc: waiting %d seconds to auto stop discovery/recovery of %s",
                       ctx->timeoutSeconds,
                       ctx->deviceClass);
            incrementalCondTimedWait(&ctx->cond, &ctx->mtx, ctx->timeoutSeconds);
        }
        else
        {
            icLogDebug(LOG_TAG,
                       "discoverDeviceClassThreadProc: waiting for explicit stop discovery/recovery of %s",
                       ctx->deviceClass);
            // TODO switch this over to timedWait library
            pthread_cond_wait(&ctx->cond, &ctx->mtx);
        }
        pthread_mutex_unlock(&ctx->mtx);

        icLogInfo(LOG_TAG, "discoverDeviceClassThreadProc: stopping discovery/recovery of %s", ctx->deviceClass);

        // stop discovery
        icLinkedListIterator *iterator = linkedListIteratorCreate(startedDeviceDrivers);
        while (linkedListIteratorHasNext(iterator))
        {
            DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(iterator);
            driver->stopDiscoveringDevices(driver->callbackContext, ctx->deviceClass);
        }
        linkedListIteratorDestroy(iterator);

        clearRejectedDevices();
        clearPairingFailedDevices();
        if (ctx->findOrphanedDevices)
        {
            sendRecoveryStoppedEvent(ctx->deviceClass);
        }
        else
        {
            sendDiscoveryStoppedEvent(ctx->deviceClass);
        }
    }

    mutexLock(&discoveryControlMutex);

    if (activeDiscoveries != NULL)
    {
        hashMapDelete(activeDiscoveries,
                      ctx->deviceClass,
                      (uint16_t) (strlen(ctx->deviceClass) + 1),
                      standardDoNotFreeHashMapFunc);
    }

    mutexUnlock(&discoveryControlMutex);

    linkedListDestroy(startedDeviceDrivers, standardDoNotFreeFunc);
    pthread_mutex_destroy(&ctx->mtx);
    pthread_cond_destroy(&ctx->cond);
    linkedListDestroy(ctx->filters, destroyDiscoveryFilterFromList);
    free(ctx->deviceClass);
    free(ctx);

    return NULL;
}

static discoverDeviceClassContext *startDiscoveryForDeviceClass(const char *deviceClass,
                                                                icLinkedList *filters,
                                                                uint16_t timeoutSeconds,
                                                                bool findOrphanedDevices)
{
    icLogDebug(LOG_TAG, "startDiscoveryForDeviceClass: %s for %d seconds", deviceClass, timeoutSeconds);

    discoverDeviceClassContext *ctx = (discoverDeviceClassContext *) calloc(1, sizeof(discoverDeviceClassContext));

    ctx->useMonotonic = true;
    initTimedWaitCond(&ctx->cond);

    pthread_mutex_init(&ctx->mtx, NULL);
    ctx->timeoutSeconds = timeoutSeconds;
    ctx->deviceClass = strdup(deviceClass);
    ctx->findOrphanedDevices = findOrphanedDevices;
    if (filters != NULL)
    {
        ctx->filters = linkedListDeepClone(filters, cloneDiscoveryFilterItems, NULL);
    }

    char *name = stringBuilder("discoverDC:%s", deviceClass);
    createDetachedThread(discoverDeviceClassThreadProc, ctx, name);
    free(name);

    return ctx;
}

/* return a portion of a URI.  If uri = /3908023984/ep/3/m/test and numSlashes is 3 then
 * we return /3908023984/ep/3
 */
static char *getPartialUri(const char *uri, int numSlashes)
{
    if (uri == NULL)
    {
        return NULL;
    }

    char *result = strdup(uri);
    char *tmp = result;
    int slashCount = 0;
    while (*tmp != '\0')
    {
        if (*tmp == '/')
        {
            if (slashCount++ == numSlashes)
            {
                *tmp = '\0';
                break;
            }
        }

        tmp++;
    }

    return result;
}

static void
yoinkResource(icDeviceResource *oldRes, icDevice *newDevice, icLinkedList *resources, const char *resourceId)
{
    sbIcLinkedListIterator *newResIt = linkedListIteratorCreate(resources);
    while (linkedListIteratorHasNext(newResIt) == true)
    {
        icDeviceResource *newRes = linkedListIteratorGetNext(newResIt);
        if (stringCompare(resourceId, newRes->id, false) == 0)
        {
            // found it
            free(newRes->value);
            newRes->value = oldRes->value;
            oldRes->value = NULL;
            break;
        }
    }
}

/*
 * Copy/move (aka 'yoink') some custom data from one device to another as part of reconfiguration.  This data
 * includes metadata, date added, and labels.  Return true on success.
 *
 * TODO: if a driver has any non-standard custom stuff that isnt covered here, this may need to get expanded
 */
static bool yoinkCustomizedDeviceData(icDevice *oldDevice, icDevice *newDevice)
{
    bool result = true;

    linkedListDestroy(newDevice->metadata, (linkedListItemFreeFunc) metadataDestroy);
    newDevice->metadata = oldDevice->metadata;
    oldDevice->metadata = NULL;

    // grab the original date added
    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *oldDateAddedRes =
        deviceServiceGetResourceByIdInternal(oldDevice->uuid, NULL, COMMON_DEVICE_RESOURCE_DATE_ADDED, false);

    // yoink the date added from old and apply to new
    if (oldDateAddedRes != NULL)
    {
        yoinkResource(oldDateAddedRes, newDevice, newDevice->resources, COMMON_DEVICE_RESOURCE_DATE_ADDED);
    }

    // loop over each endpoint on the old device so we can yoink metadata and labels
    sbIcLinkedListIterator *it = linkedListIteratorCreate(oldDevice->endpoints);
    while (linkedListIteratorHasNext(it) == true)
    {
        icDeviceEndpoint *endpoint = linkedListIteratorGetNext(it);

        // find the label resource, if it exists
        AUTO_CLEAN(resourceDestroy__auto)
        icDeviceResource *oldLabelRes =
            deviceServiceGetResourceByIdInternal(oldDevice->uuid, endpoint->id, COMMON_ENDPOINT_RESOURCE_LABEL, false);

        // find the matching endpoint on the new instance
        bool endpointMatched = false;
        sbIcLinkedListIterator *newIt = linkedListIteratorCreate(newDevice->endpoints);
        while (linkedListIteratorHasNext(newIt) == true)
        {
            icDeviceEndpoint *newEndpoint = linkedListIteratorGetNext(newIt);

            if (stringCompare(endpoint->id, newEndpoint->id, false) == 0)
            {
                // this is the matching endpoint, engage yoinkification technology

                linkedListDestroy(newEndpoint->metadata, (linkedListItemFreeFunc) metadataDestroy);
                newEndpoint->metadata = endpoint->metadata;
                endpoint->metadata = NULL;

                if (oldLabelRes != NULL)
                {
                    // yoink the label from old and apply to new
                    yoinkResource(oldLabelRes, newDevice, newEndpoint->resources, COMMON_ENDPOINT_RESOURCE_LABEL);
                }

                endpointMatched = true;
            }
        }

        if (endpointMatched == false)
        {
            icLogError(LOG_TAG,
                       "%s: failed to match endpoints for metadata migration! (%s not found)",
                       __func__,
                       endpoint->id);
            result = false;
            break;
        }
    }

    return result;
}

static void reconfigureDeviceTask(void *arg)
{
    bool result = false;

    g_autoptr(ReconfigureDeviceContext) ctx = arg;
    arg = NULL;

    // holding ctx->mtx only for a short while to avoid dead locks in configureDevice()
    mutexLock(&ctx->mtx);
    scoped_generic char *deviceUuid = strdup(ctx->deviceUuid);
    mutexUnlock(&ctx->mtx);
    // set the metadata if not already set
    setMetadata(deviceUuid, NULL, COMMON_DEVICE_METADATA_RECONFIGURATION_REQUIRED, "true");

    DeviceDriver *driver = NULL;
    scoped_icDevice *device = deviceServiceGetDevice(deviceUuid);
    if (device == NULL)
    {
        icError("Failed to get device for uuid: %s", deviceUuid);
    }
    else if ((driver = getDeviceDriverForUri(device->uri)) == NULL)
    {
        icError("Failed to get device driver device uri: %s", device->uri);
    }
    else if (driver->getDeviceClassVersion == NULL)
    {
        icError("device reconfiguration required, but unable to determine new device class version");
    }
    else
    {
        // these resources are destroyed when the device is destroyed in deviceInitializationTask
        icDeviceResource *manufacturer =
            deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_MANUFACTURER);
        icDeviceResource *model = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_MODEL);
        icDeviceResource *hardwareVersion =
            deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION);
        icDeviceResource *firmwareVersion =
            deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

        if (manufacturer == NULL || model == NULL || firmwareVersion == NULL || hardwareVersion == NULL)
        {
            icError("device reconfiguration required, but unable to locate required resources");
        }
        else
        {
            // Reconfiguration works by creating a new icDevice instance much like how we did when we first
            // discovered it.  We then move over any transient data (metadata) from the old instance, then
            // persist it and discard the old one.

            uint8_t newDeviceClassVersion = 0;
            if (driver->getDeviceClassVersion(driver->callbackContext, device->deviceClass, &newDeviceClassVersion) ==
                true)
            {
                icInitialResourceValues *initialValues = initialResourceValuesCreate();

                AUTO_CLEAN(deviceDescriptorFree__auto)
                DeviceDescriptor *dd = deviceDescriptorsGet(
                    manufacturer->value, model->value, hardwareVersion->value, firmwareVersion->value);

                // Create a device instance populated with all required items from the base device class specification
                AUTO_CLEAN(deviceDestroy__auto)
                icDevice *newDevice =
                    createDevice(device->uuid, device->deviceClass, newDeviceClassVersion, driver->driverName, dd);

                deviceServiceCommFailHintDeviceTimeoutSecs(newDevice,
                                                           deviceServiceCommFailGetDeviceTimeoutSecs(device));
                // configureDevice() can block for sleepy devices
                if (driver->configureDevice(driver->callbackContext, newDevice, dd) == false)
                {
                    icError("failed to reconfigure device");
                }
                else if (fetchCommonResourcesInitialValues(newDevice,
                                                           manufacturer->value,
                                                           model->value,
                                                           hardwareVersion->value,
                                                           firmwareVersion->value,
                                                           initialValues) == false)
                {
                    icError("failed to fetch common initial resource values for reconfiguration");
                }
                else if (addCommonResources(newDevice, initialValues) == false)
                {
                    icError("failed to add common resources for reconfiguration");
                }
                else if (driver->fetchInitialResourceValues(driver->callbackContext, newDevice, initialValues) == false)
                {
                    icError("failed to fetch initial resource values for reconfiguration");
                }
                else if (driver->registerResources(driver->callbackContext, newDevice, initialValues) == false)
                {
                    icError("failed to register resources for reconfiguration");
                }
                else
                {
                    icInfo("device reconfigured -- persisting.");

                    // changes were likely made to the device, persist it.  Steal metadata from the old instance.
                    result = yoinkCustomizedDeviceData(device, newDevice);

                    if (result)
                    {
                        jsonDatabaseRemoveDeviceById(device->uuid);
                        finalizeNewDevice(newDevice, false, false);
                        // remove the metadata since reconfiguration succeeded
                        if (deviceServiceRemoveMetadata(
                                deviceUuid, NULL, COMMON_DEVICE_METADATA_RECONFIGURATION_REQUIRED) == false)
                        {
                            icWarn("Failed to remove metadata %s for device: %s",
                                   COMMON_DEVICE_METADATA_RECONFIGURATION_REQUIRED,
                                   ctx->deviceUuid);
                        }
                    }
                }

                initialResourceValuesDestroy(initialValues);
            }
            else
            {
                icError("failed to get device class version.  Skipping reconfiguration");
            }
        }
    }

    // invoke the reconfiguration completed callback
    if (ctx->reconfigurationCompleted != NULL)
    {
        ctx->reconfigurationCompleted(result, ctx->deviceUuid);
    }

    // remove the context from pending reconfiguration hashmap
    deviceServiceRemoveReconfigureDeviceContext(ctx);
}

bool deviceServiceDeviceNeedsReconfiguring(const char *deviceUuid)
{
    bool retVal = false;

    scoped_icDevice *device = deviceServiceGetDevice(deviceUuid);
    if (device == NULL)
    {
        icError("Failed to get device for uuid: %s", deviceUuid);
        return false;
    }

    DeviceDriver *driver = getDeviceDriverForUri(device->uri);
    if (driver == NULL)
    {
        icError("Failed to get device driver device uri: %s", device->uri);
        return false;
    }

    // Compare device class versions first
    uint8_t newDeviceClassVersion = 0;
    if (driver->getDeviceClassVersion != NULL &&
        driver->getDeviceClassVersion(driver->callbackContext, device->deviceClass, &newDeviceClassVersion) == true)
    {
        if (device->deviceClassVersion != newDeviceClassVersion)
        {
            icInfo("device %s has different class version than expected (%" PRIu8 " vs %" PRIu8
                   ").  Reconfiguration needed",
                   device->uuid,
                   device->deviceClassVersion,
                   newDeviceClassVersion);
            retVal = true;
        }
    }
    else
    {
        icError("Failed to get device class version for device %s!", device->uuid);
    }

    // If the driver has defined endpoint profile versions, then we have something to check against.
    if (retVal == false && driver->endpointProfileVersions != NULL)
    {
        sbIcLinkedListIterator *endpointsIt = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(endpointsIt) == true)
        {
            const icDeviceEndpoint *endpoint = linkedListIteratorGetNext(endpointsIt);
            const uint8_t *expectedEndpointProfileVersion =
                hashMapGet(driver->endpointProfileVersions, endpoint->profile, strlen(endpoint->profile) + 1);

            if (expectedEndpointProfileVersion != NULL && endpoint->profileVersion != *expectedEndpointProfileVersion)
            {
                icInfo("device %s has an endpoint with differing %s profile version (%" PRIu8 " vs %" PRIu8 "). "
                       "Reconfiguration needed",
                       device->uuid,
                       endpoint->profile,
                       endpoint->profileVersion,
                       *expectedEndpointProfileVersion);
                retVal = true;
                break;
            }
        }
    }

    if (retVal == false)
    {
        // check the meta data to see if reconfiguration is requied for this device or not
        scoped_generic char *reconfRequired =
            getMetadata(device->uuid, NULL, COMMON_DEVICE_METADATA_RECONFIGURATION_REQUIRED);
        if (stringToBool(reconfRequired))
        {
            icInfo("device has %s metadata set. Reconfiguration needed",
                   COMMON_DEVICE_METADATA_RECONFIGURATION_REQUIRED);
            retVal = true;
        }
    }

    return retVal;
}

static void reconfigurationCompletedCallback(bool result, const char *deviceUuid)
{
    // This reconfiguration is part of initial device startup where we either reconfigure or synchronize.
    // If re-configuring failed, then fall back to synchronization.
    if (result == false && deviceServiceIsReadyForDeviceOperation())
    {
        scoped_icDevice *device = deviceServiceGetDevice(deviceUuid);
        if (device != NULL)
        {
            DeviceDriver *driver = getDeviceDriverForUri(device->uri);
            if (driver != NULL)
            {
                driver->synchronizeDevice(driver->callbackContext, device);
            }
        }
    }
}

static void deviceInitializationTask(void *arg)
{
    char *uuid = (char *) arg;

    if (deviceServiceIsShuttingDown())
    {
        icDebug("synchronization cancelled");
        return;
    }

    scoped_icDevice *device = deviceServiceGetDevice(uuid);
    if (device != NULL)
    {
        DeviceDriver *driver = getDeviceDriverForUri(device->uri);
        if (driver != NULL)
        {
            // a device can optionally be reconfigured OR synchronized (reconfiguration covers synchronization)
            if (deviceServiceDeviceNeedsReconfiguring(uuid) == true)
            {
                // scheduling a asynchronous reconfiguration to run now
                deviceServiceReconfigureDevice(uuid, 0, reconfigurationCompletedCallback, false);
            }
            // if reconfiguration is not required, then just synchronize the device.
            // The synchronize device operation is synchronous (ie blocking) unlike
            // reconfiguration which is asynchronous
            else if (driver->synchronizeDevice != NULL)
            {
                driver->synchronizeDevice(driver->callbackContext, device);
            }
        }
        else
        {
            icError("driver for device %s not found", uuid);
        }
    }
    else
    {
        icError("device %s not found", uuid);
    }
}

static void startDeviceInitialization(void)
{
    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = linkedListIteratorGetNext(iterator);
        if (threadPoolAddTask(deviceInitializerThreadPool, deviceInitializationTask, strdup(device->uuid), NULL) ==
            false)
        {
            icLogError(LOG_TAG, "%s: failed to add deviceInitializationTask to thread pool", __FUNCTION__);
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

static void *shutdownDeviceDriverManagerThreadProc(void *arg)
{
    deviceDriverManagerShutdown();

    pthread_mutex_lock(&deviceDriverManagerShutdownMtx);
    pthread_cond_signal(&deviceDriverManagerShutdownCond);
    pthread_mutex_unlock(&deviceDriverManagerShutdownMtx);

    return NULL;
}

/*
 * Give the device driver manager a maximum amount of time to shut down the device drivers.  Some may be in the
 * middle of a firmware upgrade and we need to give them ample time to finish.
 */
static void shutdownDeviceDriverManager()
{
    icLogDebug(LOG_TAG, "%s: shutting down", __FUNCTION__);

    initTimedWaitCond(&deviceDriverManagerShutdownCond);
    pthread_mutex_lock(&deviceDriverManagerShutdownMtx);

    createDetachedThread(shutdownDeviceDriverManagerThreadProc, NULL, "driverMgrShutdown");

    if (incrementalCondTimedWait(
            &deviceDriverManagerShutdownCond, &deviceDriverManagerShutdownMtx, MAX_DRIVERS_SHUTDOWN_SECS) != 0)
    {
        icLogWarn(LOG_TAG, "%s: timed out waiting for drivers to shut down.", __FUNCTION__);
    }

    pthread_mutex_unlock(&deviceDriverManagerShutdownMtx);

    icLogDebug(LOG_TAG, "%s: finished shutting down", __FUNCTION__);
}

/****************** Simple Data Accessor Functions ***********************/
icLinkedList *deviceServiceGetDevicesByProfile(const char *profileId)
{
    // just pass through to database
    return jsonDatabaseGetDevicesByEndpointProfile(profileId);
}

icLinkedList *deviceServiceGetDevicesByDeviceClass(const char *deviceClass)
{
    // just pass through to database
    return jsonDatabaseGetDevicesByDeviceClass(deviceClass);
}

icLinkedList *deviceServiceGetDevicesByDeviceDriver(const char *deviceDriver)
{
    // just pass through to database
    return jsonDatabaseGetDevicesByDeviceDriver(deviceDriver);
}

icLinkedList *deviceServiceGetAllDevices()
{
    // just pass through to database
    return jsonDatabaseGetDevices();
}

icLinkedList *deviceServiceGetEndpointsByProfile(const char *profileId)
{
    icLinkedList *retval = NULL;

    icLinkedList *endpoints = jsonDatabaseGetEndpointsByProfile(profileId);
    if (endpoints != NULL)
    {
        retval = linkedListCreate();
        icLinkedListIterator *listIter = linkedListIteratorCreate(endpoints);
        while (linkedListIteratorHasNext(listIter) == true)
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(listIter);
            if (endpoint != NULL && deviceServiceIsUriAccessible(endpoint->uri) == true)
            {
                linkedListAppend(retval, endpointClone(endpoint));
            }
        }
        linkedListIteratorDestroy(listIter);
        linkedListDestroy(endpoints, (linkedListItemFreeFunc) endpointDestroy);
    }

    return retval;
}

icDevice *deviceServiceGetDevice(const char *uuid)
{
    // just pass through to database
    return jsonDatabaseGetDeviceById(uuid);
}

bool deviceServiceIsDeviceKnown(const char *uuid)
{
    // just pass through to database
    return jsonDatabaseIsDeviceKnown(uuid);
}

icDevice *deviceServiceGetDeviceByUri(const char *uri)
{
    if (uri == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return NULL;
    }

    if (deviceServiceIsUriAccessible(uri) == false)
    {
        icLogWarn(LOG_TAG, "%s: uri %s is not accessible", __FUNCTION__, uri);
        return NULL;
    }

    icDevice *device = jsonDatabaseGetDeviceByUri(uri);
    return device;
}

icDeviceEndpoint *deviceServiceGetEndpointByUri(const char *uri)
{
    icLogDebug(LOG_TAG, "%s: uri=%s", __FUNCTION__, uri);

    icDeviceEndpoint *endpoint = NULL;
    if (deviceServiceIsUriAccessible(uri) == true)
    {
        endpoint = jsonDatabaseGetEndpointByUri(uri);
    }

    return endpoint;
}

icDeviceEndpoint *deviceServiceGetEndpointById(const char *deviceUuid, const char *endpointId)
{
    icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointById(deviceUuid, endpointId);
    if (endpoint != NULL && deviceServiceIsUriAccessible(endpoint->uri) == false)
    {
        endpointDestroy(endpoint);
        endpoint = NULL;
    }

    return endpoint;
}

/*
 * if endpointId is NULL, we are after an resource on the root device
 */
static icDeviceResource *deviceServiceGetResourceByIdInternal(const char *deviceUuid,
                                                              const char *endpointId,
                                                              const char *resourceId,
                                                              bool logDebug)
{
    icDeviceResource *result = NULL;

    // dont debug print on frequently fetched resource ids to preserve log files
    if (logDebug == true && resourceId != NULL && strcmp(COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, resourceId) != 0)
    {
        icLogDebug(LOG_TAG,
                   "deviceServiceGetResource: deviceUuid=%s, endpointId=%s, resourceId=%s",
                   deviceUuid,
                   stringCoalesce(endpointId),
                   resourceId);
    }

    if (deviceUuid != NULL && resourceId != NULL)
    {
        scoped_generic char *uri = resourceUriCreate(deviceUuid, endpointId, resourceId);
        result = jsonDatabaseGetResourceByUri(uri);
    }

    if (logDebug == true && result == NULL)
    {
        icLogDebug(LOG_TAG, "did not find the resource");
    }

    return result;
}

/*
 * Get the age (in milliseconds) since the provided resource was last updated/sync'd with the device.
 */
bool deviceServiceGetResourceAgeMillis(const char *deviceUuid,
                                       const char *endpointId,
                                       const char *resourceId,
                                       uint64_t *ageMillis)
{
    bool result = true;

    if (deviceUuid == NULL || resourceId == NULL || ageMillis == NULL)
    {
        return false;
    }

    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *resource = deviceServiceGetResourceByIdInternal(deviceUuid, endpointId, resourceId, true);

    if (resource != NULL)
    {
        *ageMillis = getCurrentUnixTimeMillis();
        *ageMillis -= resource->dateOfLastSyncMillis;
        result = true;
    }
    else
    {
        *ageMillis = 0;
        result = false;
    }

    return result;
}

/*
 * if endpointId is NULL, we are after an resource on the root device
 */
icDeviceResource *deviceServiceGetResourceById(const char *deviceUuid, const char *endpointId, const char *resourceId)
{
    return deviceServiceGetResourceByIdInternal(deviceUuid, endpointId, resourceId, true);
}

/*
 * Retrieve an icDeviceResource by id.  This will not look on any endpoints, but only on the device itself
 *
 * Caller should NOT destroy the icDeviceResource on the non-NULL result, as it belongs to the icDevice object
 *
 * @param device - the device
 * @param resourceId - the unique id of the desired resource
 *
 * @returns the icDeviceResource or NULL if not found
 */
icDeviceResource *deviceServiceFindDeviceResourceById(icDevice *device, const char *resourceId)
{
    icDeviceResource *retval = NULL;
    if (device == NULL)
    {
        icLogDebug(LOG_TAG, "%s: NULL device passed", __FUNCTION__);
        return NULL;
    }
    icLinkedListIterator *iter = linkedListIteratorCreate(device->resources);
    while (linkedListIteratorHasNext(iter))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iter);
        if (strcmp(resource->id, resourceId) == 0)
        {
            retval = resource;
            break;
        }
    }
    linkedListIteratorDestroy(iter);

    return retval;
}

bool deviceServiceGetSystemProperty(const char *name, char **value)
{
    return jsonDatabaseGetSystemProperty(name, value);
}

bool deviceServiceGetAllSystemProperties(icStringHashMap *map)
{
    return jsonDatabaseGetAllSystemProperties(map);
}

bool deviceServiceSetSystemProperty(const char *name, const char *value)
{
    return jsonDatabaseSetSystemProperty(name, value);
}

bool deviceServiceGetMetadata(const char *uri, char **value)
{
    bool result = false;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return false;
    }

    if (deviceServiceIsUriAccessible(uri) == false)
    {
        icLogWarn(LOG_TAG, "%s: uri %s is not accessible", __FUNCTION__, uri);
        return false;
    }

    if (value != NULL)
    {
        icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uri);

        if (metadata != NULL && metadata->value != NULL)
        {
            *value = strdup(metadata->value);
        }
        else
        {
            *value = NULL;
        }

        metadataDestroy(metadata);
        result = true;
    }

    return result;
}

bool deviceServiceSetMetadata(const char *uri, const char *value)
{
    bool result = false;
    bool saveDb = false;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return false;
    }

    if (deviceServiceIsUriAccessible(uri) == false)
    {
        icLogWarn(LOG_TAG, "%s: resource %s is not accessible", __FUNCTION__, uri);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: setting metadata %s %s", __FUNCTION__, uri, value);

    icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uri);
    if (metadata == NULL)
    {
        // new item
        metadata = (icDeviceMetadata *) calloc(1, sizeof(icDeviceMetadata));
        saveDb = true;

        char *deviceId = (char *) calloc(strlen(uri), 1);   // allocate worst case
        char *name = (char *) calloc(strlen(uri), 1);       // allocate worst case
        char *endpointId = (char *) calloc(strlen(uri), 1); // allocate worst case

        if (parseMetadataUri(uri, endpointId, deviceId, name) == true)
        {
            if (strstr(uri, JSON_DATABASE_ENDPOINT_MARKER) == NULL)
            {
                // metadata is on device
                //
                free(endpointId);
                endpointId = NULL;
            }
            metadata->id = name;
            metadata->deviceUuid = deviceId;
            metadata->endpointId = endpointId;
            metadata->uri = strdup(uri);
        }
        else
        {
            icLogError(LOG_TAG, "Invalid URI %s", uri);
            free(deviceId);
            free(name);
            free(metadata);
            free(endpointId);
            return false;
        }
    }
    else
    {
        if (strcmp(uri, metadata->uri) != 0 || strcmp(value, metadata->value) != 0)
        {
            saveDb = true;
        }

        if (metadata->value != NULL)
        {
            free(metadata->value);
            metadata->value = NULL;
        }
    }

    if (saveDb)
    {
        if (value != NULL)
        {
            metadata->value = strdup(value);
        }

        result = jsonDatabaseSaveMetadata(metadata);
    }
    else
    {
        // there was no change, so just return success
        result = true;
    }

    metadataDestroy(metadata);

    return result;
}

/*
 * Retrieve a list of devices that contains the metadataId or
 * contains the metadataId value that is equal to valueToCompare
 *
 * If valueToCompare is NULL, will only look if the metadata exists.
 * Otherwise will only add devices that equal the metadata Id and it's value.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @return - linked list of icDevices with found metadata, or NULL error occurred
 */
icLinkedList *deviceServiceGetDevicesByMetadata(const char *metadataId, const char *valueToCompare)
{
    // sanity check
    if (metadataId == NULL)
    {
        icLogWarn(LOG_TAG, "%s: unable to use metadataId ... bailing", __FUNCTION__);
        return NULL;
    }

    // get the list of devices
    //
    icLinkedList *deviceList = deviceServiceGetAllDevices();

    // sanity check
    if (deviceList == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to get list of all devices ... bailing", __FUNCTION__);
        return NULL;
    }

    // create the found list
    //
    icLinkedList *devicesFound = linkedListCreate();

    // iterate though each device
    //
    icLinkedListIterator *listIter = linkedListIteratorCreate(deviceList);
    while (linkedListIteratorHasNext(listIter))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(listIter);

        // need to create the metadata deviceUri
        //
        char *deviceMetadataUri = createDeviceMetadataUri(device->uuid, metadataId);
        if (deviceMetadataUri != NULL)
        {
            // now get the metadata value from device
            //
            char *deviceMetadataValue = NULL;
            if (deviceServiceGetMetadata(deviceMetadataUri, &deviceMetadataValue) == true &&
                deviceMetadataValue != NULL)
            {
                bool addDevice = false;

                // determine if device should be added to list
                //
                if (valueToCompare == NULL)
                {
                    addDevice = true;
                }
                else if (strcasecmp(deviceMetadataValue, valueToCompare) == 0)
                {
                    addDevice = true;
                }

                // remove from device list and add to found list
                //
                if (addDevice)
                {
                    linkedListIteratorDeleteCurrent(listIter, standardDoNotFreeFunc);
                    linkedListAppend(devicesFound, device);
                }

                // cleanup value
                free(deviceMetadataValue);
            }

            // cleanup uri
            free(deviceMetadataUri);
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: unable to create device Metadata Uri for device %s", __FUNCTION__, device->uuid);
        }
    }

    // cleanup
    linkedListIteratorDestroy(listIter);
    linkedListDestroy(deviceList, (linkedListItemFreeFunc) deviceDestroy);

    return devicesFound;
}

bool deviceServiceReloadDatabase()
{
    return jsonDatabaseReload();
}

icLinkedList *deviceServiceGetMetadataByUriPattern(char *uriPattern)
{
    icLinkedList *retval = NULL;
    if (uriPattern == NULL)
    {
        icLogWarn(LOG_TAG, "%s: uriPattern is NULL", __FUNCTION__);
        return NULL;
    }

    if (isUriPattern(uriPattern) == true)
    {
        retval = linkedListCreate();
        scoped_generic char *regex = createRegexFromPattern(uriPattern);

        icLinkedList *metadataList = jsonDatabaseGetMetadataByUriRegex(regex);
        icLinkedListIterator *listIter = linkedListIteratorCreate(metadataList);
        while (linkedListIteratorHasNext(listIter) == true)
        {
            icDeviceMetadata *metadata = (icDeviceMetadata *) linkedListIteratorGetNext(listIter);
            if (metadata != NULL && deviceServiceIsUriAccessible(metadata->uri) == true)
            {
                linkedListAppend(retval, metadataClone(metadata));
            }
        }
        linkedListIteratorDestroy(listIter);
        linkedListDestroy(metadataList, (linkedListItemFreeFunc) metadataDestroy);
    }
    else
    {
        retval = linkedListCreate();
        icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uriPattern);
        if (metadata != NULL && deviceServiceIsUriAccessible(metadata->uri) == true)
        {
            linkedListAppend(retval, metadata);
        }
        else
        {
            metadataDestroy(metadata);
        }
    }

    return retval;
}

/*
 * Query for resources based on a uri pattern, currently only supported matching is with wildcards, e.g. *
 *
 * @param uriPattern the uri pattern to search with
 * @return the list of matching resources
 */
icLinkedList *deviceServiceGetResourcesByUriPatternInternal(const char *uriPattern)
{
    icLinkedList *retval = NULL;

    if (uriPattern == NULL)
    {
        icLogError(LOG_TAG, "%s: URI pattern is NULL", __FUNCTION__);
        return NULL;
    }

    if (isUriPattern(uriPattern) == true)
    {
        scoped_generic char *regex = createRegexFromPattern(uriPattern);
        retval = jsonDatabaseGetResourcesByUriRegex(regex);
    }
    else
    {
        retval = linkedListCreate();
        icDeviceResource *resource = jsonDatabaseGetResourceByUri(uriPattern);
        if (resource != NULL)
        {
            linkedListAppend(retval, resource);
        }
    }

    return retval;
}

icLinkedList *deviceServiceGetResourcesByUriPattern(char *uriPattern)
{
    if (uriPattern == NULL)
    {
        icLogError(LOG_TAG, "%s: URI pattern is NULL", __FUNCTION__);
        return NULL;
    }

    icLinkedList *retval = NULL;

    icLinkedList *resourceList = deviceServiceGetResourcesByUriPatternInternal(uriPattern);
    if (resourceList != NULL)
    {
        retval = linkedListCreate();
        icLinkedListIterator *listIter = linkedListIteratorCreate(resourceList);
        while (linkedListIteratorHasNext(listIter) == true)
        {
            icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(listIter);
            if (resource != NULL && deviceServiceIsUriAccessible(resource->uri) == true)
            {
                linkedListAppend(retval, resourceClone(resource));
            }
        }
        linkedListIteratorDestroy(listIter);
        linkedListDestroy(resourceList, (linkedListItemFreeFunc) resourceDestroy);
    }

    return retval;
}

void deviceServiceNotifySystemPowerEvent(DeviceServiceSystemPowerEventType powerEvent)
{
    icLogDebug(LOG_TAG, "%s: state=%s", __FUNCTION__, DeviceServiceSystemPowerEventTypeLabels[powerEvent]);

    // Let any drivers do anything they need to do
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    icLinkedListIterator *iter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(iter))
    {
        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(iter);
        if (driver->systemPowerEvent != NULL)
        {
            driver->systemPowerEvent(driver->callbackContext, powerEvent);
        }
    }
    linkedListIteratorDestroy(iter);
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);
}

void deviceServiceNotifyPropertyChange(const char *propKey, const char *propValue)
{
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    icLinkedListIterator *iter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(iter))
    {
        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(iter);
        if (driver->propertyChanged != NULL)
        {
            driver->propertyChanged(driver->callbackContext, propKey, propValue);
        }
    }
    linkedListIteratorDestroy(iter);
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);
}

static icLinkedList *getSupportedDeviceClasses(void)
{
    icLinkedList *result = linkedListCreate();

    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    sbIcLinkedListIterator *drivers = linkedListIteratorCreate(deviceDrivers);
    while (linkedListIteratorHasNext(drivers))
    {
        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(drivers);
        sbIcLinkedListIterator *deviceClassesIter = linkedListIteratorCreate(driver->supportedDeviceClasses);
        while (linkedListIteratorHasNext(deviceClassesIter))
        {
            char *deviceClass = linkedListIteratorGetNext(deviceClassesIter);

            // a hash set would be more efficient, but probably not worth the complexity given that this
            // is expected to be very infrequently invoked.
            if (linkedListFind(result, deviceClass, linkedListStringCompareFunc) == false)
            {
                linkedListAppend(result, strdup(deviceClass));
            }
        }
    }
    // This is a shallow clone, so free function doesn't really matter
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);

    return result;
}

DeviceServiceStatus *deviceServiceGetStatus(void)
{
    DeviceServiceStatus *result = calloc(1, sizeof(DeviceServiceStatus));
    result->supportedDeviceClasses = getSupportedDeviceClasses();
    result->discoveryRunning = deviceServiceIsDiscoveryActive();
    result->discoveringDeviceClasses = linkedListCreate();
    result->findingOrphanedDevices = false;

    // if all the subsystems are ready, it means we can operate paired devices.
    //
    result->isReadyForDeviceOperation = deviceServiceIsReadyForDeviceOperation();
    // we cannot pair unless all subsystems are ready
    //
    result->isReadyForPairing = deviceDescriptorsListIsReady() && result->isReadyForDeviceOperation;

    if (result->discoveryRunning == true)
    {
        mutexLock(&discoveryControlMutex);

        result->discoveryTimeoutSeconds = discoveryTimeoutSeconds;

        icHashMapIterator *iterator = hashMapIteratorCreate(activeDiscoveries);
        while (hashMapIteratorHasNext(iterator))
        {
            char *class;
            uint16_t classLen;
            discoverDeviceClassContext *ctx;
            if (hashMapIteratorGetNext(iterator, (void **) &class, &classLen, (void **) &ctx))
            {
                if (linkedListFind(result->discoveringDeviceClasses, class, linkedListStringCompareFunc) == false)
                {
                    linkedListAppend(result->discoveringDeviceClasses, strdup(class));
                }
                result->findingOrphanedDevices = result->findingOrphanedDevices || ctx->findOrphanedDevices;
            }
        }
        hashMapIteratorDestroy(iterator);

        mutexUnlock(&discoveryControlMutex);
    }

    result->subsystemsJsonStatus = hashMapCreate();

    scoped_icLinkedListGeneric *subsystems = subsystemManagerGetRegisteredSubsystems();
    sbIcLinkedListIterator *subsystemsIt = linkedListIteratorCreate(subsystems);
    while (linkedListIteratorHasNext(subsystemsIt))
    {
        char *subsystemName = linkedListIteratorGetNext(subsystemsIt);
        hashMapPut(result->subsystemsJsonStatus,
                   strdup(subsystemName),
                   strlen(subsystemName),
                   subsystemManagerGetSubsystemStatusJson(subsystemName));
    }

    return result;
}

bool deviceServiceIsDeviceInCommFail(const char *deviceUuid)
{
    if (deviceUuid == NULL)
    {
        return false;
    }

    bool retVal = false;

    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *commFailResource =
        deviceServiceGetResourceById(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_COMM_FAIL);

    if (commFailResource != NULL)
    {
        retVal = stringToBool(commFailResource->value);
    }

    return retVal;
}

char *deviceServiceGetDeviceFirmwareVersion(const char *deviceUuid)
{
    char *retVal = NULL;

    if (deviceUuid == NULL)
    {
        return NULL;
    }

    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *fwResource =
        deviceServiceGetResourceById(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if (fwResource != NULL)
    {
        retVal = fwResource->value;
        fwResource->value = NULL;
    }

    return retVal;
}

static void markDeviceRejected(const char *deviceUuid)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, deviceUuid);

    mutexLock(&rejectedMutex);

    char *key = strdup(deviceUuid);
    uint16_t keyLen = (uint16_t) strlen(key) + 1;

    // Delete previous entry if one exists
    hashMapDelete(rejected, key, keyLen, NULL);

    // Add the new entry
    if (hashMapPut(rejected, key, keyLen, NULL) == false)
    {
        icLogWarn(LOG_TAG, "%s: failed to mark device rejected: %s", __FUNCTION__, deviceUuid);
        free(key);
    }

    mutexUnlock(&rejectedMutex);
}

bool deviceServiceWasDeviceRejected(const char *deviceUuid)
{
    pthread_mutex_lock(&rejectedMutex);
    bool wasRejected = hashMapContains(rejected, (void *) deviceUuid, strlen(deviceUuid) + 1);
    pthread_mutex_unlock(&rejectedMutex);

    return wasRejected;
}

static void clearRejectedDevices()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&rejectedMutex);
    hashMapClear(rejected, NULL);
    pthread_mutex_unlock(&rejectedMutex);
}

static void markDevicePairingFailed(const char *deviceUuid)
{
    icDebug("%s", deviceUuid);

    mutexLock(&failedToPairMutex);

    char *key = strdup(deviceUuid);
    uint16_t keyLen = (uint16_t) strlen(key) + 1;

    // Delete previous entry if one exists
    hashMapDelete(failedToPair, key, keyLen, NULL);

    // Add the new entry
    if (hashMapPut(failedToPair, key, keyLen, NULL) == false)
    {
        icWarn("failed to mark device for pairing failure: %s", deviceUuid);
        free(key);
    }

    mutexUnlock(&failedToPairMutex);
}

bool deviceServiceDidDeviceFailToPair(const char *deviceUuid)
{
    mutexLock(&failedToPairMutex);
    bool failed = hashMapContains(failedToPair, (void *) deviceUuid, strlen(deviceUuid) + 1);
    mutexUnlock(&failedToPairMutex);

    return failed;
}

static void clearPairingFailedDevices()
{
    icDebug();

    mutexLock(&failedToPairMutex);
    hashMapClear(failedToPair, NULL);
    mutexUnlock(&failedToPairMutex);
}

static ReconfigureDeviceContext *acquireReconfigureDeviceContext(ReconfigureDeviceContext *ctx)
{
    ReconfigureDeviceContext *out = NULL;

    if (ctx != NULL)
    {
        out = g_atomic_rc_box_acquire(ctx);
    }

    return out;
}

static inline void releaseReconfigureDeviceContext(ReconfigureDeviceContext *ctx)
{
    if (ctx != NULL)
    {
        g_atomic_rc_box_release_full(ctx, (GDestroyNotify) destroyReconfigureDeviceContext);
    }
}

static void destroyReconfigureDeviceContext(ReconfigureDeviceContext *ctx)
{
    if (ctx != NULL)
    {
        free(ctx->deviceUuid);
    }
}

static void cancelPendingReconfigurationTasks()
{
    mutexLock(&reconfigurationControlMutex);
    icHashMapIterator *pendingIt = hashMapIteratorCreate(pendingReconfiguration);
    while (hashMapIteratorHasNext(pendingIt))
    {
        void *deviceUuid;
        uint16_t keyLen;
        ReconfigureDeviceContext *ctx;
        hashMapIteratorGetNext(pendingIt, &deviceUuid, &keyLen, (void **) &ctx);

        mutexLock(&ctx->mtx);
        uint32_t taskHandle = ctx->taskHandle;
        mutexUnlock(&ctx->mtx);

        // cancel the waiting tasks
        if (isDelayTaskWaiting(taskHandle))
        {
            cancelDelayTask(taskHandle);
            hashMapIteratorDeleteCurrent(pendingIt, pendingReconfigurationFreeFunc);
        }
        else
        {
            // Send terminate signal to the tasks that are already scheduled and waiting
            sendReconfigurationSignal(ctx, true);
        }
    }
    hashMapIteratorDestroy(pendingIt);
    mutexUnlock(&reconfigurationControlMutex);
}

static void waitForPendingReconfigurationsToComplete()
{
    LOCK_SCOPE(reconfigurationControlMutex);
    uint16_t waitCount = 0;
    while (waitCount < MAX_PENDING_RECONFIGURATION_WAIT_COUNT)
    {
        uint16_t count = 0;
        if (pendingReconfiguration != NULL)
        {
            count = hashMapCount(pendingReconfiguration);
        }

        if (count == 0)
        {
            break;
        }
        else
        {
            icDebug("%" PRIu16 " reconfiguration tasks are blocking", count);
            incrementalCondTimedWait(
                &reconfigurationControlCond, &reconfigurationControlMutex, PENDING_RECONFIGURATION_TIMEOUT_SEC);
        }
        waitCount++;
    }
}

static void pendingReconfigurationFreeFunc(void *key, void *value)
{
    /* key is owned by value, do not free */

    if (value != NULL)
    {
        releaseReconfigureDeviceContext((ReconfigureDeviceContext *) value);
    }
}

static bool deviceServiceSetReconfigureDeviceContext(ReconfigureDeviceContext *ctx)
{
    bool result = false;
    mutexLock(&reconfigurationControlMutex);
    if (ctx != NULL && pendingReconfiguration != NULL)
    {
        result = hashMapPut(pendingReconfiguration,
                            ctx->deviceUuid,
                            (uint16_t) (strlen(ctx->deviceUuid) + 1),
                            acquireReconfigureDeviceContext(ctx));
    }
    mutexUnlock(&reconfigurationControlMutex);
    return result;
}

// context acquired from calling this function should be released with releaseReconfigureDeviceContext()
// unless g_autoptr is used, as it calls releaseReconfigureDeviceContext() automatically
static ReconfigureDeviceContext *deviceServiceGetReconfigureDeviceContext(const char *deviceUuid)
{
    ReconfigureDeviceContext *ctx = NULL;
    mutexLock(&reconfigurationControlMutex);
    if (deviceUuid != NULL && pendingReconfiguration != NULL)
    {
        ctx = acquireReconfigureDeviceContext(
            hashMapGet(pendingReconfiguration, (char *) deviceUuid, (uint16_t) (strlen(deviceUuid) + 1)));
    }
    mutexUnlock(&reconfigurationControlMutex);
    return ctx;
}

bool deviceServiceWaitForReconfigure(const char *deviceUuid)
{
    bool result = false;

    g_autoptr(ReconfigureDeviceContext) ctx = deviceServiceGetReconfigureDeviceContext(deviceUuid);
    if (ctx != NULL)
    {
        result = true;
        icDebug("reconfiguration pending for device uuid: %s. Waiting on reconfiguration condition", deviceUuid);
        mutexLock(&ctx->mtx);

        struct timespec start;
        struct timespec now;
        uint32_t timeoutSeconds = ctx->timeoutSeconds;
        getCurrentTime(&start, supportMonotonic());
        while (timeoutSeconds > 0 && ctx->isSignaled == false)
        {
            incrementalCondTimedWait(&ctx->cond, &ctx->mtx, timeoutSeconds);
            if (ctx->isSignaled == false)
            {
                getCurrentTime(&now, supportMonotonic());
                if (ctx->timeoutSeconds > (now.tv_sec - start.tv_sec))
                {
                    timeoutSeconds = ctx->timeoutSeconds - (now.tv_sec - start.tv_sec);
                }
                else
                {
                    timeoutSeconds = 0;
                    result = false;
                    icError("Timed out while waiting reconfiguration condition");
                }
            }
        }

        if (result && ctx->shouldTerminate)
        {
            icInfo("Asked to terminate, so abandoning reconfiguration for device uuid: %s", deviceUuid);
            result = false;
        }
        mutexUnlock(&ctx->mtx);
    }
    return result;
}

bool deviceServiceIsReconfigurationPending(const char *deviceUuid)
{
    bool result = false;

    g_autoptr(ReconfigureDeviceContext) ctx = deviceServiceGetReconfigureDeviceContext(deviceUuid);
    if (ctx != NULL)
    {
        result = true;
    }
    return result;
}

bool deviceServiceIsReconfigurationAllowedAsap(const char *deviceUuid)
{
    bool result = false;

    g_autoptr(ReconfigureDeviceContext) ctx = deviceServiceGetReconfigureDeviceContext(deviceUuid);
    if (ctx != NULL && ctx->isAllowedAsap == true)
    {
        result = true;
    }

    return result;
}

static bool sendReconfigurationSignal(ReconfigureDeviceContext *ctx, bool shouldTerminate)
{
    bool result = false;

    if (ctx != NULL)
    {
        mutexLock(&ctx->mtx);
        ctx->isSignaled = true;
        ctx->shouldTerminate = shouldTerminate;
        pthread_cond_signal(&ctx->cond);
        mutexUnlock(&ctx->mtx);
        result = true;
    }

    return result;
}

bool deviceServiceSendReconfigurationSignal(const char *deviceUuid, bool shouldTerminate)
{
    g_autoptr(ReconfigureDeviceContext) ctx = deviceServiceGetReconfigureDeviceContext(deviceUuid);
    return sendReconfigurationSignal(ctx, shouldTerminate);
}

static bool deviceServiceRemoveReconfigureDeviceContext(ReconfigureDeviceContext *ctx)
{
    bool result = false;
    mutexLock(&reconfigurationControlMutex);
    if (ctx != NULL && pendingReconfiguration != NULL)
    {
        result = hashMapDelete(pendingReconfiguration,
                               ctx->deviceUuid,
                               (uint16_t) (strlen(ctx->deviceUuid) + 1),
                               pendingReconfigurationFreeFunc);
        // signal that pending reconfiguration hasmap has shrunk,
        // which means reconfiguration task has completed.
        pthread_cond_broadcast(&reconfigurationControlCond);
    }
    mutexUnlock(&reconfigurationControlMutex);
    return result;
}

void deviceServiceReconfigureDevice(const char *deviceUuid,
                                    uint32_t delaySeconds,
                                    reconfigurationCompleteCallback reconfigurationCompleted,
                                    bool allowAsap)
{
    // schedule a reconfiguration task if one is not already scheduled
    if (deviceServiceIsReconfigurationPending(deviceUuid) == false)
    {
        icDebug("Scheduling reconfiguration for device %s after %d seconds", deviceUuid, delaySeconds);
        ReconfigureDeviceContext *ctx = g_atomic_rc_box_new0(ReconfigureDeviceContext);
        ctx->deviceUuid = strdup(deviceUuid);
        mutexInitWithType(&ctx->mtx, PTHREAD_MUTEX_ERRORCHECK);
        pthread_cond_init(&ctx->cond, NULL);
        ctx->reconfigurationCompleted = reconfigurationCompleted;
        ctx->shouldTerminate = false;
        ctx->isSignaled = false;
        ctx->isAllowedAsap = allowAsap;

        // The RECONFIGURATION_TIMEOUT_SEC limits how long we should wait before we give up
        // trying to reconfigure a sleepy device.
        ctx->timeoutSeconds = RECONFIGURATION_TIMEOUT_SEC;

        // add the context to pending reconfiguration hashmap
        if (deviceServiceSetReconfigureDeviceContext(ctx) == false)
        {
            icError("Failed to add reconfiguration device context to pending reconfiguration list, not scheduling "
                    "reconfiguration task");
            return;
        }
        // schedule reconfiguration
        ctx->taskHandle = scheduleDelayTask(delaySeconds, DELAY_SECS, reconfigureDeviceTask, ctx);
        ctx = NULL;
    }
    else
    {
        icDebug("Reconfiguration is already in progress for device %s", deviceUuid);
    }
}

PostUpgradeAction deviceServiceGetPostUpgradeAction(const char *deviceUuid)
{
    PostUpgradeAction retVal = POST_UPGRADE_ACTION_NONE;

    icDebug("device uuid: %s", deviceUuid);

    if (deviceUuid != NULL)
    {
        scoped_icDevice *device = deviceServiceGetDevice(deviceUuid);
        scoped_DeviceDescriptor *dd = deviceServiceGetDeviceDescriptorForDevice(device);

        if (dd != NULL && dd->latestFirmware != NULL)
        {
            // get the post upgrade action from latest firmware
            retVal = dd->latestFirmware->upgradeAction;
        }
    }

    return retVal;
}
