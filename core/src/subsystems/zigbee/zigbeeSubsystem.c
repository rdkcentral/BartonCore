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

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <memory.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "device/icDevice.h"
#include "deviceCommunicationWatchdog.h"
#include "devicePrivateProperties.h"
#include "deviceService/securityState.h"
#include "deviceServiceConfiguration.h"
#include "deviceServicePrivate.h"
#include "deviceServiceProperties.h"
#include "icTypes/icLinkedList.h"
#include "provider/barton-core-property-provider.h"
#include "subsystems/zigbee/zigbeeAttributeTypes.h"
#include "subsystems/zigbee/zigbeeCommonIds.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "zigbeeDefender.h"
#include "zigbeeEventHandler.h"
#include "zigbeeEventTracker.h"
#include "zigbeeHealthCheck.h"
#include "zigbeeSubsystemPrivate.h"
#include "zigbeeTelemetry.h"
#include <commonDeviceDefs.h>
#include <device-driver/device-driver.h>
#include <deviceDescriptors.h>
#include <deviceDriverManager.h>
#include <deviceDrivers/zigbeeDriverCommon.h>
#include <deviceHelper.h>
#include <deviceService.h>
#include <event/deviceEventProducer.h>
#include <icConcurrent/icThreadSafeWrapper.h>
#include <icConcurrent/repeatingTask.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icTime/timeTracker.h>
#include <icTime/timeUtils.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <subsystemManager.h>
#include <versionUtils.h>
#include <zhal/zhal.h>
#include <zigbeeClusters/zigbeeCluster.h>

#undef LOG_TAG
#define LOG_TAG                                            "zigbeeSubsystem"
#define logFmt(fmt)                                        "%s: " fmt, __func__

#define ZIGBEE_CORE_IP_PROPERTY_NAME                       "ZIGBEE_CORE_IP"
#define ZIGBEE_CORE_PORT_PROPERTY_NAME                     "ZIGBEE_CORE_PORT"
#define ZIGBEE_CORE_SIMPLE_NETWORK_CREATED                 "ZIGBEE_CORE_SIMPLE_NETWORK_CREATED"
#define ZIGBEE_PREVIOUS_CHANNEL_NAME                       "ZIGBEE_PREVIOUS_CHANNEL"
#define ZIGBEE_REJECT_UNKNOWN_DEVICES                      "ZIGBEE_REJECT_UNKNOWN_DEVICES"
#define ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT             "ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT"

#define ZIGBEE_HEALTH_CHECK_PROPS_PREFIX                   "cpe.zigbee.healthCheck"
#define ZIGBEE_DEFENDER_PROPS_PREFIX                       "cpe.zigbee.defender"
#define ZIGBEE_WATCHDOG_ENABLED_PROP                       "cpe.zigbee.watchdog.enabled.flag"
#define ZIGBEE_LINK_QUALITY_PROPS_PREFIX                   "cpe.zigbee.linkQuality"
#define ZIGBEE_LINK_QUALITY_LQI_ENABLED_PROP               ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".lqi.enabled"
#define ZIGBEE_LINK_QUALITY_LQI_WARN_THRESHOLD_PROP        ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".lqi.warnThreshold"
#define ZIGBEE_LINK_QUALITY_LQI_BAD_THRESHOLD_PROP         ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".lqi.badThreshold"
#define ZIGBEE_LINK_QUALITY_RSSI_ENABLED_PROP              ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".rssi.enabled"
#define ZIGBEE_LINK_QUALITY_RSSI_WARN_THRESHOLD_PROP       ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".rssi.warnThreshold"
#define ZIGBEE_LINK_QUALITY_RSSI_BAD_THRESHOLD_PROP        ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".rssi.badThreshold"
#define ZIGBEE_LINK_QUALITY_RSSI_CROSS_ABOVE_DB_PROP       ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".rssi.crossAboveDb"
#define ZIGBEE_LINK_QUALITY_RSSI_CROSS_BELOW_DB_PROP       ZIGBEE_LINK_QUALITY_PROPS_PREFIX ".rssi.crossBelowDb"

#define DEFAULT_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES 30

#define DEFAULT_ZIGBEE_CORE_IP                             "127.0.0.1"
#define DEFAULT_ZIGBEE_CORE_PORT                           "18443"

#define DELAY_BETWEEN_INITIAL_HEARTBEATS_SECONDS           1

#define LEGACY_FIRMWARE_SUBDIR                             "legacy"
#define OTA_FIRMWARE_SUBDIR                                "ota"
#define ZIGBEE_FIRMWARE_SUBDIR                             "zigbeeFirmware"

#define EUI64_JSON_PROP                                    "eui64"
#define MANUF_JSON_PROP                                    "manufacturer"
#define MODEL_JSON_PROP                                    "model"
#define HWVER_JSON_PROP                                    "hwVer"
#define FWVER_JSON_PROP                                    "fwVer"
#define APPVER_JSON_PROP                                   "appVer"
#define ID_JSON_PROP                                       "id"
#define IS_SERVER_JSON_PROP                                "isServer"
#define ATTRIBUTE_IDS_JSON_PROP                            "attributeIds"
#define PROFILEID_JSON_PROP                                "profileId"
#define DEVICEID_JSON_PROP                                 "deviceId"
#define DEVICEVER_JSON_PROP                                "deviceVer"
#define SERVERCLUSTERINFOS_JSON_PROP                       "serverClusterInfos"
#define CLIENTCLUSTERINFOS_JSON_PROP                       "clientClusterInfos"
#define ENDDEVICE_JSON_PROP                                "end"
#define ROUTERDEVICE_JSON_PROP                             "router"
#define UNKNOWN_JSON_PROP                                  "unknown"
#define DEVICETYPE_JSON_PROP                               "type"
#define MAINS_JSON_PROP                                    "mains"
#define BATT_JSON_PROP                                     "batt"
#define POWERSOURCE_JSON_PROP                              "power"
#define ENDPOINTS_JSON_PROP                                "endpoints"

#define DEVICE_USES_HASH_BASED_LINK_KEY_METADATA           "usesHashBasedLinkKey"
#define DO_NOT_UPGRADE_TO_HASH_BASED_LINK_KEY_METADATA     "doNotUpgradeToHashBasedLinkKey"

#define DEVICE_LAST_BEACON_METADATA                        "lastBeacon"

#define MIN_ZIGBEE_CHANNEL                                 11
#define MAX_ZIGBEE_CHANNEL                                 26

#define DEFAULT_ZIGBEE_CHANNEL_SCAN_DUR_MILLIS             30
#define DEFAULT_ZIGBEE_PER_CHANNEL_NUMBER_OF_SCANS         16

// Our pre-HA 1.2 sensors/devices reported this as their device id.  We will make a risky assumption that
//  any device with this ID is one of these and we will skip discovery of attributes on the basic cluster.
#define ICONTROL_BOGUS_DEVICE_ID                           0xFFFF

// The amount we should increment the counters after things like RMA.  The values here are what we have historically
// used
#define NONCE_COUNTER_INCREMENT_AMOUNT                     0x1000
#define FRAME_COUNTER_INCREMENT_AMOUNT                     0x1000

#define MAX_NETWORK_INIT_RETRIES                           3
#define NETWORK_INITIALIZATION_TIMEOUT_SECS                (MAX_NETWORK_INIT_RETRIES * 30)
#define MAX_INITIAL_ZIGBEECORE_RESTARTS                    3

#define WILDCARD_EUI64                                     0xFFFFFFFFFFFFFFFF

#define DEFAULT_RSSI_QUALITY_CROSS_ABOVE_DB                6
#define DEFAULT_RSSI_QUALITY_CROSS_BELOW_DB                6

#define COMM_FAIL_POLL_THREAD_SLEEP_TIME_SECONDS           (60 * 60)

static ZigbeeWatchdogDelegate *watchdogDelegate;
static pthread_mutex_t watchdogDelegateMtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static zhalCallbacks callbacks;

static uint8_t crossAboveDb = 0;

static uint8_t crossBelowDb = 0;

static bool networkInitialized = false;

static pthread_mutex_t networkInitializedMtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static bool migrateDevices = false;

static pthread_cond_t networkInitializedCond = PTHREAD_COND_INITIALIZER;

static bool isChannelChangeInProgress = false;

static pthread_mutex_t channelChangeMutex = PTHREAD_MUTEX_INITIALIZER;

static bool initializeNetwork(void);

static uint64_t loadLocalEui64(void);

static void prematureClusterCommandsFreeFunc(void *key, void *value);

static void configureMonitors(void);

static void zigbeeCoreMonitorFunc(void *arg);

static void zigbeeCoreMonitorTickle(void);

static bool waitForNetworkInitialization(void);

static bool isDeviceAutoApsAcked(const icDevice *device);

static bool isDeviceUsingHashBasedLinkKey(const icDevice *device);

static bool shouldUpgradeToHashBasedLinkKey(const icDevice *device);

static bool setDeviceUsingHashBasedLinkKey(const icDevice *device, bool isUsingHashBasedKey, bool setMetadataOnly);

static void startChannelChangeDeviceWatchdog(uint8_t previousChannel, uint8_t targetedChannel);

static void restartChannelChangeDeviceWatchdogIfRequired(void);

static ZigbeeSubsystemLinkQualityLevel calculateLinkQuality(int8_t rssiSample);

static void zigbeeSubsystemSetUnready(void);

static void *createDeviceCallbacks(void);

static bool releaseIfDeviceCallbacksEmpty(void *item);

static void deviceCallbackDestroy(void *item);

static void *checkAllDevicesInCommThreadProc(void *arg);

static void responseHandler(const char *responseType, ZHAL_STATUS resultCode);

typedef struct
{
    uint64_t eui64;
    ZigbeeSubsystemDeviceCallbacks *callbacks;
    bool registered;
} DeviceCallbacksRegisterContext;

static void deviceCallbacksRegister(void *item, void *context);

typedef struct
{
    uint64_t eui64;
    bool unregistered;
} DeviceCallbacksUnregisterContext;

static void deviceCallbacksUnregister(void *item, void *context);

static void deviceCallbacksAttributeReport(const void *item, const void *context);

typedef struct
{
    uint64_t eui64;
    bool isSecure;
} DeviceCallbacksRejoinContext;

static void deviceCallbacksRejoined(const void *item, const void *context);

static void deviceCallbacksLeft(const void *item, const void *context);

typedef struct
{
    ReceivedClusterCommand *command;
    bool deviceFound;
} DeviceCallbacksClusterCommandReceivedContext;

typedef struct
{
    uint64_t eui64;
    zhalDeviceType deviceType;
    zhalPowerSource powerSource;
} DeviceAnnouncedContext;

static void deviceCallbacksClusterCommandReceived(const void *item, const void *context);

typedef struct
{
    OtaUpgradeEvent *otaEvent;
    bool isSentFromServer;
} DeviceCallbacksOtaUpgradeEventContext;

static void deviceCallbacksOtaUpgradeMessageSent(const void *item, const void *context);

static void deviceCallbacksOtaUpgradeMessageReceived(const void *item, const void *context);

static void zigbeeLinkQualityConfigure(void);

static int discoveringRefCount = 0; // when zero we are not discovering...
static pthread_mutex_t discoveringRefCountMutex = PTHREAD_MUTEX_INITIALIZER;

static icLinkedList *discoveringDeviceCallbacks = NULL;

static pthread_mutex_t discoveringDeviceCallbacksMutex = PTHREAD_MUTEX_INITIALIZER;

static icThreadSafeWrapper deviceCallbacksWrapper =
    THREAD_SAFE_WRAPPER_INIT(createDeviceCallbacks, releaseIfDeviceCallbacksEmpty, deviceCallbackDestroy);

// in order to support the pairing process for legacy sensors, which send a command to us
//  immediately after joining but before we have recognized it, we must hold on to commands
//  sent from devices that are not yet paired while we are in discovery.
static icHashMap *prematureClusterCommands = NULL; // eui64 to linked list of ReceivedClusterCommands
static pthread_mutex_t prematureClusterCommandsMtx = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t prematureClusterCommandsCond;

static subsystemInitializedFunc subsystemInitialized = NULL;
static subsystemDeInitializedFunc subsystemDeinitialized = NULL;

// ZigbeeCore monitor details
static uint32_t zigbeeCoreMonitorTask = 0;

static pthread_mutex_t commFailControlMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static pthread_cond_t commFailControlCond = PTHREAD_COND_INITIALIZER;
static bool fastCommFailTimer = false;
static pthread_t commFailMonitorThreadId;
static bool commfailMonitorThreadRunning = false;

// Set a default for monitor run interval, could make this configurable later
static uint32_t zigbeeCoreHeartbeatPetFrequencySecs = 60;

// A set of device UUIDs that are using hash based link keys which we were notified about before the device was saved
static icHashMap *earlyHashedBasedLinkKeyDevices = NULL;

static pthread_mutex_t earlyHashBasedLinkKeyDevicesMtx = PTHREAD_MUTEX_INITIALIZER;

// Set of devices that are in the process of being discovered.  We use this to know which devices are in the process of
// being discovered so that we can still route commands coming from them until they are persisted.
static icHashMap *devicesInDiscovery = NULL;

static pthread_mutex_t devicesInDiscoverMtx = PTHREAD_MUTEX_INITIALIZER;

// Set of devices that failed to process during discovery and need to be told to leave after discovery ends.
static icHashMap *unclaimedDevices = NULL;

static pthread_mutex_t unclaimedDevicesMtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

// From stock branding for Healthcheck
#define BAD_RSSI_LIMIT_DEFAULT  -75
#define WARN_RSSI_LIMIT_DEFAULT -65

// From stock branding for Healthcheck
#define BAD_LQI_LIMIT_DEFAULT   150
#define WARN_LQI_LIMIT_DEFAULT  236

#define WARN_THRESHOLD_INDEX    0
#define BAD_THRESHOLD_INDEX     1

static const char *linkQualityLevelLabels[] = {LINK_QUALITY_UNKNOWN,
                                               LINK_QUALITY_POOR,
                                               LINK_QUALITY_FAIR,
                                               LINK_QUALITY_GOOD};

static bool useLqiForLinkQuality = false;

static bool useRssiForLinkQuality = true;

static uint8_t lqiThresholds[] = {WARN_LQI_LIMIT_DEFAULT, BAD_LQI_LIMIT_DEFAULT};

static int8_t rssiThresholds[] = {WARN_RSSI_LIMIT_DEFAULT, BAD_RSSI_LIMIT_DEFAULT};

static pthread_mutex_t configMtx = PTHREAD_MUTEX_INITIALIZER;

static void zigbeeSubsystemPostRestoreConfig(void);
static void zigbeeSubsystemAllServicesAvailable(void);
static void zigbeeSubsystemAllDriversStarted(void);
static bool zigbeeSubsystemRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath);

static bool zigbeeSubsystemInitialize(subsystemInitializedFunc initializedCallback,
                                      subsystemDeInitializedFunc deinitializedCallback);

static void zigbeeSubsystemEnterLPM(void);
static void zigbeeSubsystemExitLPM(void);
static void zigbeeSubsystemSetOtaUpgradeDelay(uint32_t delaySeconds);

static void zigbeeSubsystemShutdown(void);
static cJSON *getStatusJson(void);

__attribute__((constructor)) static void registerSubsystem(void)
{
    g_autoptr(Subsystem) zigbeeSubsystem = createSubsystem();
    zigbeeSubsystem->name = ZIGBEE_SUBSYSTEM_NAME;
    zigbeeSubsystem->onPostRestoreConfig = zigbeeSubsystemPostRestoreConfig;
    zigbeeSubsystem->onAllServicesAvailable = zigbeeSubsystemAllServicesAvailable;
    zigbeeSubsystem->onAllDriversStarted = zigbeeSubsystemAllDriversStarted;
    zigbeeSubsystem->onRestoreConfig = zigbeeSubsystemRestoreConfig;
    zigbeeSubsystem->onLPMStart = zigbeeSubsystemEnterLPM;
    zigbeeSubsystem->onLPMEnd = zigbeeSubsystemExitLPM;
    zigbeeSubsystem->setOtaUpgradeDelay = zigbeeSubsystemSetOtaUpgradeDelay;
    zigbeeSubsystem->initialize = zigbeeSubsystemInitialize;
    zigbeeSubsystem->shutdown = zigbeeSubsystemShutdown;
    zigbeeSubsystem->getStatusJson = getStatusJson;

    subsystemManagerRegister(zigbeeSubsystem);
}

static void waitForInitialZigbeeCoreStartup(void)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    bool waitForZigbeeCore =
        b_core_property_provider_get_property_as_bool(propertyProvider, ZIGBEE_WATCHDOG_ENABLED_PROP, true);
    if (waitForZigbeeCore == false)
    {
        icLogDebug(LOG_TAG, "Zigbee Watchdog disabled, skipping wait for ZigbeeCore to start");
    }
    int zigbeeCoreRestartCount = 0;
    while (waitForZigbeeCore == true && zigbeeCoreRestartCount < MAX_INITIAL_ZIGBEECORE_RESTARTS)
    {
        int zhalHeartbeatRc;

        // start the timer with given seconds
        //
        timeTracker *timer = timeTrackerCreate();
        icLogDebug(LOG_TAG,
                   "Starting timer of %d seconds to wait for Zigbee startup",
                   BARTON_CONFIG_ZIGBEE_STARTUP_TIMEOUT_SECONDS);
        timeTrackerStart(timer, BARTON_CONFIG_ZIGBEE_STARTUP_TIMEOUT_SECONDS);
        while ((zhalHeartbeatRc = zhalHeartbeat(NULL, NULL)) != 0 && timeTrackerExpired(timer) == false)
        {
            icLogDebug(LOG_TAG, "Waiting for ZigbeeCore to be ready.");
            sleep(DELAY_BETWEEN_INITIAL_HEARTBEATS_SECONDS);
        }

        // cleanup timer
        //
        timeTrackerDestroy(timer);

        // We have seen an issue where a watchdog delegate says its restarting ZigbeeCore after a radio upgrade, but for some
        // reason ZigbeeCore doesn't really restart.  This is an attempt to catch that condition and let the watchdog delegate
        // try one more time
        if (zhalHeartbeatRc != 0)
        {
            g_autoptr(ZigbeeWatchdogDelegate) watchdogDelegateRef = zigbeeWatchdogDelegateAcquire(watchdogDelegate);

            if (watchdogDelegateRef)
            {
                if (watchdogDelegateRef->restartZhal() == ZHAL_RESTART_ACTIVE)
                {
                    zigbeeCoreRestartCount++;
                    icWarn("ZigbeeCore restart attempted, count %d", zigbeeCoreRestartCount);
                }
                else
                {
                    // Delegate says it can't/won't restart, stop trying
                    waitForZigbeeCore = false;
                }
            }
            else
            {
                // No delegate to initiate restart, can't recover
                waitForZigbeeCore = false;
            }
        }
        else
        {
            // Either we got a heartbeat response, or we timed out a second time.
            waitForZigbeeCore = false;
        }
    }
}

bool zigbeeSubsystemSetWatchdogDelegate(ZigbeeWatchdogDelegate *delegate)
{
    bool retVal = false;
    bool validDelegate = false;

    if (delegate)
    {
        // init and shutdown are optional, all others are mandatory
        validDelegate = delegate->setAllDevicesInCommFail && delegate->petZhal && delegate->restartZhal &&
                        delegate->zhalResponseHandler && delegate->getActionInProgress;
    }

    mutexLock(&watchdogDelegateMtx);
    if (!watchdogDelegate)
    {
        if (validDelegate)
        {
            watchdogDelegate = g_steal_pointer(&delegate);
            retVal = true;
        }
        else
        {
            icError("invalid watchdog implementation, rejecting");
        }
    }
    else
    {
        icError("watchdog delegate already set, rejecting");
    }
    mutexUnlock(&watchdogDelegateMtx);

    zigbeeWatchdogDelegateRelease(delegate);

    return retVal;
}

static bool zigbeeSubsystemInitialize(subsystemInitializedFunc initializedCallback,
                                      subsystemDeInitializedFunc deInitializedCallback)
{
    mutexLock(&networkInitializedMtx);
    if (networkInitialized)
    {
        icLogError(LOG_TAG, "%s: already initialized", __FUNCTION__);
        mutexUnlock(&networkInitializedMtx);
        return false;
    }
    mutexUnlock(&networkInitializedMtx);

    icLogDebug(LOG_TAG, "%s", __func__);

    initTimedWaitCond(&prematureClusterCommandsCond);
    initTimedWaitCond(&networkInitializedCond);

    // Initialize our callbacks
    subsystemInitialized = initializedCallback;
    subsystemDeinitialized = deInitializedCallback;

    zigbeeLinkQualityConfigure();

    scoped_generic char *ip = NULL;
    scoped_generic char *port = NULL;

    // coverity[check_return] value is optional and has a default
    deviceServiceGetSystemProperty(ZIGBEE_CORE_IP_PROPERTY_NAME, &ip);

    // coverity[check_return] value is optional and has a default
    deviceServiceGetSystemProperty(ZIGBEE_CORE_PORT_PROPERTY_NAME, &port);

    if (ip == NULL)
    {
        ip = strdup(DEFAULT_ZIGBEE_CORE_IP);
    }

    if (port == NULL)
    {
        port = strdup(DEFAULT_ZIGBEE_CORE_PORT);
    }

    uint16_t portNum;
    if (stringToUint16(port, &portNum) == false)
    {
        icLogError(LOG_TAG, "%s: port '%s' is not valid!", __func__, port);
        return false;
    }

    // Initialize the watchdog delegate early in the startup sequence if provided.
    // This is critical because subsequent initialization tasks (zhalInit,
    // configureMonitors, waitForInitialZigbeeCoreStartup) may trigger watchdog
    // operations that depend on the delegate being properly initialized.
    g_autoptr(ZigbeeWatchdogDelegate) watchdogDelegateRef = zigbeeWatchdogDelegateAcquire(watchdogDelegate);

    if (watchdogDelegateRef && watchdogDelegateRef->init)
    {
        watchdogDelegateRef->init();
    }

    memset(&callbacks, 0, sizeof(callbacks));
    zigbeeEventHandlerInit(&callbacks);

    zhalInit(ip, portNum, &callbacks, NULL, responseHandler);

    // wait here until ZigbeeCore is functional or we time out
    waitForInitialZigbeeCoreStartup();

    // configure our health check monitors/tasks
    configureMonitors();

    // wait here for the network to initialize completely
    waitForNetworkInitialization();

    // Cleanup any no longer needed firmware files
    zigbeeSubsystemCleanupFirmwareFiles();

    // Check to see if we were in the middle of a channel change, awaiting for devices to rejoin.  If so,
    //  start the watchdog up again.
    restartChannelChangeDeviceWatchdogIfRequired();

    return true;
}

static void zigbeeSubsystemAllDriversStarted(void)
{
    if (migrateDevices == true)
    {
        migrateDevices = false;
    }
}

static void zigbeeSubsystemAllServicesAvailable(void)
{
#ifdef BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY
    zigbeeTelemetryInitialize();
#endif
}

static void zigbeeSubsystemShutdown(void)
{
    icLogDebug(LOG_TAG, "zigbeeSubsystemShutdown");

#ifdef CONFIG_CAP_ZIGBEE_TELEMETRY
    zigbeeTelemetryShutdown();
#endif

    zigbeeHealthCheckStop();

    zigbeeSubsystemSetUnready();

    mutexLock(&networkInitializedMtx);
    zhalTerm();
    mutexUnlock(&networkInitializedMtx);

    if (zigbeeCoreMonitorTask > 0)
    {
        cancelRepeatingTask(zigbeeCoreMonitorTask);
        zigbeeCoreMonitorTask = 0;
    }

    // cancel the devices comm fail polling thread
    mutexLock(&commFailControlMutex);

    bool cfThreadJoinable = commfailMonitorThreadRunning;
    if (commfailMonitorThreadRunning)
    {
        commfailMonitorThreadRunning = false;
        pthread_cond_broadcast(&commFailControlCond);
    }

    mutexUnlock(&commFailControlMutex);

    if (cfThreadJoinable)
    {
        pthread_join(commFailMonitorThreadId, NULL);
    }

    mutexLock(&watchdogDelegateMtx);
    ZigbeeWatchdogDelegate *localWatchdogDelegate = g_steal_pointer(&watchdogDelegate);
    mutexUnlock(&watchdogDelegateMtx);

    zigbeeWatchdogDelegateRelease(g_steal_pointer(&localWatchdogDelegate));

    // clean up any premature cluster commands we may have received while in discovery
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    hashMapDestroy(prematureClusterCommands, prematureClusterCommandsFreeFunc);
    prematureClusterCommands = NULL;
    pthread_mutex_unlock(&prematureClusterCommandsMtx);

    // clean up any devices in our map/set that may have updated to the hash based link key
    //  before we were ready to save that fact.
    pthread_mutex_lock(&earlyHashBasedLinkKeyDevicesMtx);
    if (earlyHashedBasedLinkKeyDevices != NULL)
    {
        hashMapDestroy(earlyHashedBasedLinkKeyDevices, NULL);
        earlyHashedBasedLinkKeyDevices = NULL;
    }
    pthread_mutex_unlock(&earlyHashBasedLinkKeyDevicesMtx);

    pthread_cond_destroy(&prematureClusterCommandsCond);
    pthread_cond_destroy(&networkInitializedCond);
}

static cJSON *getStatusJson(void)
{
    cJSON *result = cJSON_CreateObject();

    mutexLock(&networkInitializedMtx);
    cJSON_AddBoolToObject(result, SUBSYSTEM_STATUS_COMMON_READY, networkInitialized);
    mutexUnlock(&networkInitializedMtx);

    return result;
}

static void incrementNetworkCountersIfRequired()
{
    char *incrementCounters = NULL;
    if (deviceServiceGetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, &incrementCounters) == true &&
        stringCompare(incrementCounters, "true", true) == 0)
    {
        // Do the counter increment
        if (zhalIncrementNetworkCounters(NONCE_COUNTER_INCREMENT_AMOUNT, FRAME_COUNTER_INCREMENT_AMOUNT) == false)
        {
            icLogWarn(LOG_TAG, "Failed to increment zigbee counters");
        }
        else
        {
            icLogDebug(LOG_TAG, "Successfully incremented zigbee counters");

            // Reset to not increment
            deviceServiceSetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, "false");
        }
    }
    free(incrementCounters);
}

/**
 * This must *not* be called by the zigbeeCoreMonitorFunc thread
 * @param ignored
 * @return
 */
static void zigbeeSubsystemSetReady(void)
{
    zigbeeEventHandlerSystemReady();

    // indicate that the network is initialized, but dont broadcast that yet.
    mutexLock(&networkInitializedMtx);
    networkInitialized = true;
    mutexUnlock(&networkInitializedMtx);

    // program device addresses
    zigbeeSubsystemSetAddresses();

    // configure the health check stuff in ZigbeeCore
    zigbeeHealthCheckStart();

    // configure the defender stuff in ZigbeeCore
    zigbeeDefenderConfigure();

    // this callback must be invoked after zigbeeSubsystemSetAddresses() for a device driver
    // to be able to make zhal requests with a device uuid else ZigbeeCore won't have record
    // of the device.
    if (subsystemInitialized != NULL)
    {
        subsystemInitialized(ZIGBEE_SUBSYSTEM_NAME);
    }

    // finally notify any waiters that the network is now initialized
    mutexLock(&networkInitializedMtx);
    pthread_cond_broadcast(&networkInitializedCond);
    mutexUnlock(&networkInitializedMtx);
}

static void zigbeeSubsystemSetUnready(void)
{
    bool wasReady;

    // Ideally we do a cmpxchg. Maybe with g_atomic_int :-)
    mutexLock(&networkInitializedMtx);
    if ((wasReady = networkInitialized) == true)
    {
        networkInitialized = false;
    }
    mutexUnlock(&networkInitializedMtx);

    if (wasReady == true && subsystemDeinitialized != NULL)
    {
        subsystemDeinitialized(ZIGBEE_SUBSYSTEM_NAME);
    }
}

// This should ONLY be called from zigbeeCoreMonitorFunc
static bool initializeNetwork(void)
{
    bool result = false;

    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    g_autoptr(GHashTable) allProps = b_core_property_provider_get_all_properties(propertyProvider);
    scoped_icStringHashMap *properties = stringHashMapCreate();

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, allProps);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        if (g_str_has_prefix(key, ZIGBEE_PROPS_PREFIX))
        {
            stringHashMapPutCopy(properties, key, value);
        }
    }

    /*
     * zhal receives properties with the ZIGBEE_PROPS_PREFIX chopped off, since
     * the cpe.zigbee namespace makes no sense to ZigbeeCore.
     */
    scoped_icStringHashMapIterator *it = stringHashMapIteratorCreate(properties);
    scoped_icStringHashMap *zhalProps = stringHashMapCreate();
    const size_t prefixLen = strlen(ZIGBEE_PROPS_PREFIX);

    while (stringHashMapIteratorHasNext(it) == true)
    {
        char *key = NULL;
        char *val = NULL;

        if (stringHashMapIteratorGetNext(it, &key, &val) == true && strlen(key) > prefixLen)
        {
            char *newKey = strdup(key + strlen(ZIGBEE_PROPS_PREFIX));
            stringHashMapPut(zhalProps, newKey, strdupOpt(val));
        }
    }

    scoped_generic char *blob = NULL;

    if (!deviceServiceGetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, &blob))
    {
        icLogWarn(LOG_TAG, "No network configuration loaded; a new network will be created");
    }

    uint64_t localEui64 = loadLocalEui64();
    scoped_generic char *region = b_core_property_provider_get_property_as_string(
        propertyProvider, CPE_REGION_CODE_PROPERTY_NAME, "US");

    for (int i = 0; i < MAX_NETWORK_INIT_RETRIES; ++i)
    {
        int initResult = zhalNetworkInit(localEui64, region, blob, zhalProps);

        if (initResult == 0)
        {
            incrementNetworkCountersIfRequired();

            zigbeeSubsystemSetReady();

            result = true;
            break;
        }
        else
        {
            icLogError(LOG_TAG,
                       "zhalNetworkInit failed(rc=%d)!!! Retries remaining = %d",
                       initResult,
                       MAX_NETWORK_INIT_RETRIES - i - 1);
        }
    }

    return result;
}

bool zigbeeSubsystemSetNetworkConfig(uint64_t eui64, const char *networkBlob)
{
    scoped_generic char *localEui64 = zigbeeSubsystemEui64ToId(eui64);

    bool allSet = true;
    allSet &= deviceServiceSetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, networkBlob);
    allSet &= deviceServiceSetSystemProperty(LOCAL_EUI64_PROP_KEY, localEui64);
    // Go ahead and increment counters to be safe
    allSet &= deviceServiceSetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, "true");

    return allSet;
}

int zigbeeSubsystemRegisterDiscoveryHandler(const char *name, ZigbeeSubsystemDeviceDiscoveredHandler *handler)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, name);

    pthread_mutex_lock(&discoveringDeviceCallbacksMutex);

    if (discoveringDeviceCallbacks == NULL)
    {
        discoveringDeviceCallbacks = linkedListCreate();
    }

    linkedListAppend(discoveringDeviceCallbacks, handler);
    pthread_mutex_unlock(&discoveringDeviceCallbacksMutex);

    return 0;
}

int zigbeeSubsystemUnregisterDiscoveryHandler(ZigbeeSubsystemDeviceDiscoveredHandler *handler)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, handler->driverName);

    pthread_mutex_lock(&discoveringDeviceCallbacksMutex);
    icLinkedListIterator *iterator = linkedListIteratorCreate(discoveringDeviceCallbacks);
    while (linkedListIteratorHasNext(iterator))
    {
        ZigbeeSubsystemDeviceDiscoveredHandler *item =
            (ZigbeeSubsystemDeviceDiscoveredHandler *) linkedListIteratorGetNext(iterator);
        if (handler == item)
        {
            linkedListIteratorDeleteCurrent(iterator, standardDoNotFreeFunc);
        }
    }
    linkedListIteratorDestroy(iterator);
    pthread_mutex_unlock(&discoveringDeviceCallbacksMutex);

    return 0;
}

int zigbeeSubsystemRegisterDeviceListener(uint64_t eui64, ZigbeeSubsystemDeviceCallbacks *cbs)
{
    DeviceCallbacksRegisterContext context = {.eui64 = eui64, .callbacks = cbs, .registered = false};
    threadSafeWrapperModifyItem(&deviceCallbacksWrapper, deviceCallbacksRegister, &context);

    return context.registered == true ? 0 : -1;
}

static void freeDeviceCallbackEntry(void *key, void *value)
{
    free(key);
}

int zigbeeSubsystemUnregisterDeviceListener(uint64_t eui64)
{
    DeviceCallbacksUnregisterContext context = {.eui64 = eui64, .unregistered = false};
    threadSafeWrapperModifyItem(&deviceCallbacksWrapper, deviceCallbacksUnregister, &context);

    return context.unregistered == true ? 0 : -1;
}

void zigbeeSubsystemDumpDeviceDiscovered(IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "IcDiscoveredDeviceDetails:");
    icLogDebug(LOG_TAG, "\tEUI64: %016" PRIx64, details->eui64);
    switch (details->deviceType)
    {
        case deviceTypeEndDevice:
            icLogDebug(LOG_TAG, "\tDevice Type: end device");
            break;
        case deviceTypeRouter:
            icLogDebug(LOG_TAG, "\tDevice Type: router");
            break;

        default:
            icLogDebug(LOG_TAG, "\tDevice Type: unknown");
            break;
    }
    switch (details->powerSource)
    {
        case powerSourceMains:
            icLogDebug(LOG_TAG, "\tPower Source: mains");
            break;
        case powerSourceBattery:
            icLogDebug(LOG_TAG, "\tPower Source: battery");
            break;

        default:
            icLogDebug(LOG_TAG, "\tPower Source: unknown");
            break;
    }
    icLogDebug(LOG_TAG, "\tManufacturer: %s", details->manufacturer);
    icLogDebug(LOG_TAG, "\tModel: %s", details->model);
    icLogDebug(LOG_TAG, "\tHardware Version: 0x%" PRIx64, details->hardwareVersion);
    icLogDebug(LOG_TAG, "\tFirmware Version: 0x%" PRIx64, details->firmwareVersion);
    icLogDebug(LOG_TAG, "\tNumber of endpoints: %d", details->numEndpoints);
    for (int i = 0; i < details->numEndpoints; i++)
    {
        icLogDebug(LOG_TAG, "\t\tEndpoint ID: %d", details->endpointDetails[i].endpointId);
        icLogDebug(LOG_TAG, "\t\tProfile ID: 0x%04" PRIx16, details->endpointDetails[i].appProfileId);
        icLogDebug(LOG_TAG, "\t\tDevice ID: 0x%04" PRIx16, details->endpointDetails[i].appDeviceId);
        icLogDebug(LOG_TAG, "\t\tDevice Version: %d", details->endpointDetails[i].appDeviceVersion);

        icLogDebug(LOG_TAG, "\t\tServer Cluster IDs:");
        for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails; j++)
        {
            icLogDebug(LOG_TAG, "\t\t\t0x%04" PRIx16, details->endpointDetails[i].serverClusterDetails[j].clusterId);

            icLogDebug(LOG_TAG, "\t\t\tAttribute IDs:");
            for (int k = 0; k < details->endpointDetails[i].serverClusterDetails[j].numAttributeIds; k++)
            {
                icLogDebug(LOG_TAG,
                           "\t\t\t\t0x%04" PRIx16,
                           details->endpointDetails[i].serverClusterDetails[j].attributeIds[k]);
            }
        }

        icLogDebug(LOG_TAG, "\t\tClient Cluster IDs:");
        for (int j = 0; j < details->endpointDetails[i].numClientClusterDetails; j++)
        {
            icLogDebug(LOG_TAG, "\t\t\t0x%04" PRIx16, details->endpointDetails[i].clientClusterDetails[j].clusterId);

            icLogDebug(LOG_TAG, "\t\t\tAttribute IDs:");
            for (int k = 0; k < details->endpointDetails[i].clientClusterDetails[j].numAttributeIds; k++)
            {
                icLogDebug(LOG_TAG,
                           "\t\t\t\t0x%04" PRIx16,
                           details->endpointDetails[i].clientClusterDetails[j].attributeIds[k]);
            }
        }
    }
}

bool zigbeeSubsystemClaimDiscoveredDevice(IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator)
{
    pthread_mutex_lock(&discoveringDeviceCallbacksMutex);
    icLinkedListIterator *iterator = linkedListIteratorCreate(discoveringDeviceCallbacks);
    bool deviceClaimed = false;

    while (linkedListIteratorHasNext(iterator) && deviceClaimed == false)
    {
        ZigbeeSubsystemDeviceDiscoveredHandler *item =
            (ZigbeeSubsystemDeviceDiscoveredHandler *) linkedListIteratorGetNext(iterator);
        deviceClaimed = item->callback(item->callbackContext, details, deviceMigrator);
    }
    linkedListIteratorDestroy(iterator);
    pthread_mutex_unlock(&discoveringDeviceCallbacksMutex);

    return deviceClaimed;
}

void zigbeeSubsystemHandleZhalStartupEvent(void)
{
    // we received a zhal startup event.  Indicate that the network needs to be initialized and tickle our zigbee core
    //  monitor to run now in case init is required
    zigbeeSubsystemSetUnready();

    zigbeeCoreMonitorTickle();
}

void zigbeeSubsystemDeviceDiscovered(IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zigbeeSubsystemDumpDeviceDiscovered(details);

    // Mark this device as being in discovery, so we know not to reject commands from it
    pthread_mutex_lock(&devicesInDiscoverMtx);
    if (devicesInDiscovery == NULL)
    {
        devicesInDiscovery = hashMapCreate();
    }
    hashMapPutCopy(devicesInDiscovery, &details->eui64, sizeof(details->eui64), NULL, 0);
    pthread_mutex_unlock(&devicesInDiscoverMtx);

    bool deviceClaimed = zigbeeSubsystemClaimDiscoveredDevice(details, NULL);

    // All done, its either out now, or persisted
    pthread_mutex_lock(&devicesInDiscoverMtx);
    hashMapDelete(devicesInDiscovery, &details->eui64, sizeof(details->eui64), NULL);
    if (hashMapCount(devicesInDiscovery) == 0)
    {
        hashMapDestroy(devicesInDiscovery, NULL);
        devicesInDiscovery = NULL;
    }
    pthread_mutex_unlock(&devicesInDiscoverMtx);

    if (deviceClaimed == false)
    {
        // An unclaimed device needs to be told to leave. If we tell it to leave mid discovery
        // cycle, then it might attempt to rejoin and would just need to be kicked out again, so
        // we keep track of unclaimed devices so that they can be told to leave after discovery stops.
        // If a device was claimed by a driver but failed to commission, then it should not be
        // tracked here as it was already told to leave and is allowed to try comissioning again
        // this discovery cycle.
        scoped_generic char *uuid = zigbeeSubsystemEui64ToId(details->eui64);
        if (deviceServiceDidDeviceFailToPair(uuid) == false)
        {
            pthread_mutex_lock(&unclaimedDevicesMtx);
            if (unclaimedDevices == NULL)
            {
                unclaimedDevices = hashMapCreate();
            }
            hashMapPutCopy(unclaimedDevices, &details->eui64, sizeof(details->eui64), NULL, 0);
            pthread_mutex_unlock(&unclaimedDevicesMtx);
        }
    }
}

void zigbeeSubsystemSetRejectUnknownDevices(bool doReject)
{
    deviceServiceSetSystemProperty(ZIGBEE_REJECT_UNKNOWN_DEVICES, doReject ? "true" : "false");
}

static bool deviceShouldBeRejected(uint64_t eui64, bool *discovering)
{
    bool result = false;

    pthread_mutex_lock(&discoveringRefCountMutex);
    *discovering = discoveringRefCount > 0;
    pthread_mutex_unlock(&discoveringRefCountMutex);

    if (deviceServiceIsReadyForPairing() == false)
    {
        return false;
    }

    // First check if we are rejecting these devices is enabled.  If the property isn't there we assume its enabled
    bool rejectEnabled = true;
    char *value;
    if (deviceServiceGetSystemProperty(ZIGBEE_REJECT_UNKNOWN_DEVICES, &value) == true)
    {
        rejectEnabled = (value != NULL && stringCompare(value, "true", true) == 0);
        free(value);
    }

    if (rejectEnabled == true)
    {
        // if we are discovering, allow device to talk to us, otherwise see if we know it
        if (*discovering == false)
        {
            // Discovery might have ended but the device might not yet be persisted, so check if its still in process
            bool deviceInDiscoveryProcess = false;
            pthread_mutex_lock(&devicesInDiscoverMtx);
            if (devicesInDiscovery != NULL)
            {
                deviceInDiscoveryProcess = hashMapContains(devicesInDiscovery, &eui64, sizeof(eui64));
            }
            pthread_mutex_unlock(&devicesInDiscoverMtx);

            // If its not known to be in discovery, check whether its already persisted in device service
            if (deviceInDiscoveryProcess == false)
            {
                char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
                if (!deviceServiceIsDeviceKnown(deviceUuid))
                {
                    icLogWarn(LOG_TAG, "%s: received message from unknown device %s!", __FUNCTION__, deviceUuid);
                    result = true;
                }
                free(deviceUuid);
            }
        }
    }

    return result;
}

void zigbeeSubsystemAttributeReportReceived(ReceivedAttributeReport *report)
{
    bool discovering;
    if (deviceShouldBeRejected(report->eui64, &discovering))
    {
        zhalRequestLeave(report->eui64, false, false);
    }
    else
    {
        threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksAttributeReport, report);

        // add event to tracker after device driver(s) have a chance with it
        zigbeeEventTrackerAddAttributeReportEvent(report);
    }
}

void zigbeeSubsystemClusterCommandReceived(ReceivedClusterCommand *command)
{
    bool discovering;
    if (deviceShouldBeRejected(command->eui64, &discovering))
    {
        zhalRequestLeave(command->eui64, false, false);
    }
    else
    {
        DeviceCallbacksClusterCommandReceivedContext context = {.command = command, .deviceFound = false};
        threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksClusterCommandReceived, &context);

        bool repairing = deviceServiceIsInRecoveryMode();
        if (context.deviceFound == false || repairing == true)
        {
            // we got a command for a device we do not know or we are doing repairing.  If we are in discovery,
            // save this command in case we need it (legacy security devices)
            if (discovering)
            {
                icLogDebug(LOG_TAG,
                           "%s: saving premature cluster command for uuid %" PRIx64
                           " device while repairing = %s and device found = %s",
                           __FUNCTION__,
                           command->eui64,
                           stringValueOfBool(repairing),
                           stringValueOfBool(context.deviceFound));

                zigbeeSubsystemAddPrematureClusterCommand(command);
            }
        }

        // add event to tracker after device driver(s) have a chance with it
        zigbeeEventTrackerAddClusterCommandEvent(command);
    }
}

void zigbeeSubsystemDeviceLeft(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "zigbeeSubsystemDeviceLeft: NOT IMPLEMENTED");

    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksLeft, &eui64);
}

void zigbeeSubsystemDeviceRejoined(uint64_t eui64, bool isSecure)
{
    DeviceCallbacksRejoinContext context = {.eui64 = eui64, .isSecure = isSecure};
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksRejoined, &context);

    // add event to tracker after device driver(s) have a chance with it
    zigbeeEventTrackerAddRejoinEvent(eui64, isSecure);
}

void zigbeeSubsystemLinkKeyUpdated(uint64_t eui64, bool isUsingHashBasedKey)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (eui64 == WILDCARD_EUI64)
    {
        // We need to perform action for all the devices.
        icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
        icLinkedListIterator *iterator = linkedListIteratorCreate(devices);

        while (linkedListIteratorHasNext(iterator))
        {
            icDevice *device = linkedListIteratorGetNext(iterator);
            setDeviceUsingHashBasedLinkKey(device, isUsingHashBasedKey, true);
        }
        linkedListIteratorDestroy(iterator);
        linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
        icDevice *device = deviceServiceGetDevice(uuid);
        if (device != NULL)
        {
            setDeviceUsingHashBasedLinkKey(device, isUsingHashBasedKey, false);
            deviceDestroy(device);
        }
        // if its call to clear and we don't have device yet, do not worry about it for now.
        // only keep if its call to set.
        else if (isUsingHashBasedKey == true)
        {
            // device service doesnt yet know about this device which must be in the middle of discovery.  Save
            //  off this device for use later when we set the devices with their flags.
            pthread_mutex_lock(&earlyHashBasedLinkKeyDevicesMtx);
            if (earlyHashedBasedLinkKeyDevices == NULL)
            {
                earlyHashedBasedLinkKeyDevices = hashMapCreate();
            }
            hashMapPut(earlyHashedBasedLinkKeyDevices, strdup(uuid), strlen(uuid), NULL);
            pthread_mutex_unlock(&earlyHashBasedLinkKeyDevicesMtx);
        }
    }
}

void zigbeeSubsystemApsAckFailure(uint64_t eui64)
{
    // TODO: notify any device drivers??

    // add event to tracker after device driver(s) have a chance with it
    zigbeeEventTrackerAddApsAckFailureEvent(eui64);
}

void zigbeeSubsystemDeviceOtaUpgradeMessageSent(OtaUpgradeEvent *otaEvent)
{
    DeviceCallbacksOtaUpgradeEventContext context = {
        .otaEvent = otaEvent,
        .isSentFromServer = true,
    };
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksOtaUpgradeMessageSent, &context);
}

void zigbeeSubsystemDeviceOtaUpgradeMessageReceived(OtaUpgradeEvent *otaEvent)
{
    DeviceCallbacksOtaUpgradeEventContext context = {
        .otaEvent = otaEvent,
        .isSentFromServer = false,
    };
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksOtaUpgradeMessageReceived, &context);
}

void zigbeeSubsystemDeviceBeaconReceived(uint64_t eui64,
                                         uint16_t panId,
                                         bool isOpen,
                                         bool hasEndDeviceCapacity,
                                         bool hasRouterCapability,
                                         uint8_t depth)
{
    scoped_generic char *uuid = zigbeeSubsystemEui64ToId(eui64);
    scoped_icDevice *device = deviceServiceGetDevice(uuid);

    if (device != NULL)
    {
        // first lets log something if there is a capacity issue so something like telemetry can pick it up
        if (!hasEndDeviceCapacity || !hasRouterCapability)
        {
            icLogWarn(LOG_TAG,
                      "%s: zigbee router has no capacity! (end device cap = %s, router cap = %s)",
                      uuid,
                      stringValueOfBool(hasEndDeviceCapacity),
                      stringValueOfBool(hasRouterCapability));
        }

        scoped_cJSON *beacon = cJSON_CreateObject();

        cJSON_AddNumberToObject(beacon, "ts", (double) getCurrentUnixTimeMillis()); // precision loss is safe here
        cJSON_AddNumberToObject(beacon, "panId", panId);
        cJSON_AddBoolToObject(beacon, "isOpen", isOpen);
        cJSON_AddBoolToObject(beacon, "endDevCap", hasEndDeviceCapacity);
        cJSON_AddBoolToObject(beacon, "routerCap", hasRouterCapability);
        cJSON_AddNumberToObject(beacon, "depth", depth);

        scoped_generic char *beaconStr = cJSON_PrintUnformatted(beacon);
        scoped_generic char *uri = createDeviceMetadataUri(device->uuid, DEVICE_LAST_BEACON_METADATA);

        if (!deviceServiceSetMetadata(uri, beaconStr))
        {
            icLogWarn(LOG_TAG, "%s: failed to set beacon metadata", __func__);
        }
    }
}

static void *enableJoinThreadProc(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalNetworkEnableJoin();

    return NULL;
}

/*
 * Tells unclaimed devices to leave. Should be called after discovery stops.
 */
void zigbeeSubsystemRequestUnclaimedDevicesLeave(void)
{
    pthread_mutex_lock(&unclaimedDevicesMtx);
    scoped_icHashMapIterator *iterator = hashMapIteratorCreate(unclaimedDevices);
    while (hashMapIteratorHasNext(iterator) == true)
    {
        uint64_t *deviceUuid;
        int *val;
        uint16_t keyLen = 0;

        hashMapIteratorGetNext(iterator, (void **) &deviceUuid, &keyLen, (void **) &val);
        zhalRequestLeave(*deviceUuid, false, false);
    }

    hashMapDestroy(unclaimedDevices, NULL);
    unclaimedDevices = NULL;
    pthread_mutex_unlock(&unclaimedDevicesMtx);
}

/*
 * Enter discovery mode if we are not already.  Increment discovery counter and send the enable join command if we are
 * starting for the first time.
 */
int zigbeeSubsystemStartDiscoveringDevices(void)
{
    bool enableJoin = false;

    pthread_mutex_lock(&discoveringRefCountMutex);

    icLogDebug(LOG_TAG, "%s: discoveringRefCount = %d", __FUNCTION__, discoveringRefCount);

    if (discoveringRefCount++ == 0)
    {
        enableJoin = true;

        // clean up any premature cluster commands we may have received while in prior discovery
        pthread_mutex_lock(&prematureClusterCommandsMtx);
        hashMapDestroy(prematureClusterCommands, prematureClusterCommandsFreeFunc);
        pthread_mutex_unlock(&prematureClusterCommandsMtx);
    }
    pthread_mutex_unlock(&discoveringRefCountMutex);

    if (enableJoin)
    {
        zigbeeEventHandlerDiscoveryRunning(true);

        pthread_mutex_lock(&prematureClusterCommandsMtx);
        prematureClusterCommands = hashMapCreate();
        pthread_mutex_unlock(&prematureClusterCommandsMtx);

        // this can block for a while... put it in the background
        createDetachedThread(enableJoinThreadProc, NULL, "zbEnableJoin");
    }

    return 0;
}

/*
 * Decrement our discovery counter and stop discovery if we are at zero.
 */
int zigbeeSubsystemStopDiscoveringDevices(void)
{
    bool disableJoin = false;

    pthread_mutex_lock(&discoveringRefCountMutex);

    icLogDebug(LOG_TAG, "%s: discoveringRefCount = %d", __FUNCTION__, discoveringRefCount);

    if (--discoveringRefCount == 0)
    {
        disableJoin = true;
    }

    if (discoveringRefCount < 0)
    {
        icLogError(
            LOG_TAG, "zigbeeSubsystemStopDiscoveringDevices: discoveringRefCount is negative! %d", discoveringRefCount);
    }

    pthread_mutex_unlock(&discoveringRefCountMutex);

    if (disableJoin)
    {
        // no more devices being discovered... we can stop
        zhalNetworkDisableJoin();

        zigbeeSubsystemRequestUnclaimedDevicesLeave();

        zigbeeEventHandlerDiscoveryRunning(false);
    }

    return 0;
}

int zigbeeSubsystemSendCommand(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint8_t commandId,
                               uint8_t *message,
                               uint16_t messageLen)
{
    // for now just pass through
    return zhalSendCommand(eui64, endpointId, clusterId, toServer, false, false, commandId, message, messageLen);
}

int zigbeeSubsystemSendCommandDefaultResponse(uint64_t eui64,
                                              uint8_t endpointId,
                                              uint16_t clusterId,
                                              bool toServer,
                                              uint8_t commandId,
                                              uint8_t *message,
                                              uint16_t messageLen)
{
    // for now just pass through
    return zhalSendCommand(eui64, endpointId, clusterId, toServer, false, true, commandId, message, messageLen);
}

int zigbeeSubsystemSendCommandWithEncryption(uint64_t eui64,
                                             uint8_t endpointId,
                                             uint16_t clusterId,
                                             bool toServer,
                                             uint8_t commandId,
                                             uint8_t *message,
                                             uint16_t messageLen)
{
    // for now just pass through
    return zhalSendCommand(eui64, endpointId, clusterId, toServer, true, false, commandId, message, messageLen);
}

int zigbeeSubsystemSendMfgCommand(uint64_t eui64,
                                  uint8_t endpointId,
                                  uint16_t clusterId,
                                  bool toServer,
                                  uint8_t commandId,
                                  uint16_t mfgId,
                                  uint8_t *message,
                                  uint16_t messageLen)
{
    // for now just pass through
    return zhalSendMfgCommand(
        eui64, endpointId, clusterId, toServer, false, false, commandId, mfgId, message, messageLen);
}

int zigbeeSubsystemSendMfgCommandDefaultResponse(uint64_t eui64,
                                                 uint8_t endpointId,
                                                 uint16_t clusterId,
                                                 bool toServer,
                                                 uint8_t commandId,
                                                 uint16_t mfgId,
                                                 uint8_t *message,
                                                 uint16_t messageLen)
{
    // for now just pass through
    return zhalSendMfgCommand(
        eui64, endpointId, clusterId, toServer, false, true, commandId, mfgId, message, messageLen);
}

int zigbeeSubsystemSendMfgCommandWithEncryption(uint64_t eui64,
                                                uint8_t endpointId,
                                                uint16_t clusterId,
                                                bool toServer,
                                                uint8_t commandId,
                                                uint16_t mfgId,
                                                uint8_t *message,
                                                uint16_t messageLen)
{
    // for now just pass through
    return zhalSendMfgCommand(
        eui64, endpointId, clusterId, toServer, true, false, commandId, mfgId, message, messageLen);
}

int zigbeeSubsystemSendViaApsAck(uint64_t eui64,
                                 uint8_t endpointId,
                                 uint16_t clusterId,
                                 uint8_t sequenceNum,
                                 uint8_t *message,
                                 uint16_t messageLen)
{
    // for now just pass through
    return zhalSendViaApsAck(eui64, endpointId, clusterId, sequenceNum, message, messageLen);
}

static int readString(uint64_t eui64,
                      uint8_t endpointId,
                      uint16_t clusterId,
                      bool isMfgSpecific,
                      uint16_t mfgId,
                      bool toServer,
                      uint16_t attributeId,
                      char **value)
{
    int result = -1;

    if (value == NULL)
    {
        icLogError(LOG_TAG, "zigbeeSubsystemReadString: invalid arguments");
        return -1;
    }

    if (toServer)
    {
        uint16_t attributeIds[1] = {attributeId};
        zhalAttributeData attributeData[1];
        memset(attributeData, 0, sizeof(zhalAttributeData));

        if (isMfgSpecific)
        {
            result = zhalAttributesReadMfgSpecific(
                eui64, endpointId, clusterId, mfgId, toServer, attributeIds, 1, attributeData);
        }
        else
        {
            result = zhalAttributesRead(eui64, endpointId, clusterId, toServer, attributeIds, 1, attributeData);
        }

        if (result == 0 && attributeData[0].dataLen > 0)
        {
            *value = (char *) calloc(1, attributeData[0].data[0] + 1);
            memcpy(*value, attributeData[0].data + 1, attributeData[0].data[0]);
        }
        else
        {
            icLogError(LOG_TAG, "zigbeeSubsystemReadString: zhal failed to read attribute");
            result = -1;
        }

        free(attributeData[0].data);
    }
    else
    {
        icLogError(LOG_TAG, "zigbeeSubsystemReadString: reading client attributes not yet supported");
    }

    return result;
}

int zigbeeSubsystemReadString(uint64_t eui64,
                              uint8_t endpointId,
                              uint16_t clusterId,
                              bool toServer,
                              uint16_t attributeId,
                              char **value)
{
    return readString(eui64, endpointId, clusterId, false, 0xFFFF, toServer, attributeId, value);
}

int zigbeeSubsystemReadStringMfgSpecific(uint64_t eui64,
                                         uint8_t endpointId,
                                         uint16_t clusterId,
                                         uint16_t mfgId,
                                         bool toServer,
                                         uint16_t attributeId,
                                         char **value)
{
    return readString(eui64, endpointId, clusterId, true, mfgId, toServer, attributeId, value);
}

static int readNumbers(uint64_t eui64,
                       uint8_t endpointId,
                       uint16_t clusterId,
                       bool isMfgSpecific,
                       uint16_t mfgId,
                       bool toServer,
                       uint8_t numAttributes,
                       const uint16_t *attributeIds,
                       uint64_t *values,
                       bool *readSuccesses)
{
    int result = 0;

    if (values == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __func__);
        return -1;
    }

    memset(values, 0, sizeof(uint64_t) * numAttributes);

    zhalAttributeData *attributeData = calloc(numAttributes, sizeof(zhalAttributeData));

    if (isMfgSpecific)
    {
        result = zhalAttributesReadMfgSpecific(
            eui64, endpointId, clusterId, mfgId, toServer, attributeIds, numAttributes, attributeData);
    }
    else
    {
        result = zhalAttributesRead(eui64, endpointId, clusterId, toServer, attributeIds, numAttributes, attributeData);
    }

    if (result == 0)
    {
        for (uint8_t attIdx = 0; attIdx < numAttributes; attIdx++)
        {
            if (attributeData[attIdx].dataLen > 0 && attributeData[attIdx].dataLen <= 8) // these will fit in uint64_t
            {
                readSuccesses[attIdx] = true; // we got data for this attribute, so mark it as successful

                for (uint16_t i = 0; i < attributeData[attIdx].dataLen; i++)
                {
                    values[attIdx] += ((uint64_t) attributeData[attIdx].data[i]) << (i * 8u);
                }
            }
            else
            {
                icLogError(
                    LOG_TAG, "%s: error, no data returned for attributeId %" PRIu16, __func__, attributeIds[attIdx]);
                readSuccesses[attIdx] = false;
            }

            free(attributeData[attIdx].data);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: zhal failed to read attribute", __func__);
    }

    free(attributeData);

    return result;
}

int zigbeeSubsystemReadNumber(uint64_t eui64,
                              uint8_t endpointId,
                              uint16_t clusterId,
                              bool toServer,
                              uint16_t attributeId,
                              uint64_t *value)
{
    uint16_t attributeIds[1] = {attributeId};
    bool readSuccesses[1];

    if (readNumbers(eui64, endpointId, clusterId, false, 0xFFFF, toServer, 1, attributeIds, value, readSuccesses) ==
            0 &&
        readSuccesses[0] == true)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int zigbeeSubsystemReadNumbers(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint8_t numAttributes,
                               const uint16_t *attributeIds,
                               uint64_t *values,
                               bool *readSuccesses)
{
    return readNumbers(
        eui64, endpointId, clusterId, false, 0xFFFF, toServer, numAttributes, attributeIds, values, readSuccesses);
}

int zigbeeSubsystemReadNumberMfgSpecific(uint64_t eui64,
                                         uint8_t endpointId,
                                         uint16_t clusterId,
                                         uint16_t mfgId,
                                         bool toServer,
                                         uint16_t attributeId,
                                         uint64_t *value)
{
    uint16_t attributeIds[1] = {attributeId};
    bool readSuccesses[1];

    if (readNumbers(eui64, endpointId, clusterId, true, mfgId, toServer, 1, attributeIds, value, readSuccesses) == 0 &&
        readSuccesses[0] == true)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int zigbeeSubsystemReadNumbersMfgSpecific(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          uint16_t mfgId,
                                          bool toServer,
                                          uint8_t numAttributes,
                                          const uint16_t *attributeIds,
                                          uint64_t *values,
                                          bool *readSuccesses)
{
    return readNumbers(
        eui64, endpointId, clusterId, true, mfgId, toServer, numAttributes, attributeIds, values, readSuccesses);
}

static int writeNumber(uint64_t eui64,
                       uint8_t endpointId,
                       uint16_t clusterId,
                       bool isMfgSpecific,
                       uint16_t mfgId,
                       bool toServer,
                       uint16_t attributeId,
                       ZigbeeAttributeType attributeType,
                       uint64_t value,
                       uint8_t numBytes)
{
    zhalAttributeData attributeData;
    memset(&attributeData, 0, sizeof(zhalAttributeData));
    attributeData.attributeInfo.id = attributeId;
    attributeData.attributeInfo.type = attributeType;
    attributeData.data = (uint8_t *) calloc(numBytes, 1);

    int retVal = 0;

    // note that this implementation only supports writing up to 8 bytes (what fits in uint64_t)
    for (int i = 0; i < numBytes; i++)
    {
        attributeData.data[i] = (uint8_t) ((value >> 8 * i) & 0xff);
    }
    attributeData.dataLen = numBytes;

    if (isMfgSpecific)
    {
        retVal = zhalAttributesWriteMfgSpecific(eui64, endpointId, clusterId, mfgId, toServer, &attributeData, 1);
    }
    else
    {
        retVal = zhalAttributesWrite(eui64, endpointId, clusterId, toServer, &attributeData, 1);
    }

    // cleanup
    free(attributeData.data);

    return retVal;
}

int zigbeeSubsystemWriteNumber(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint16_t attributeId,
                               ZigbeeAttributeType attributeType,
                               uint64_t value,
                               uint8_t numBytes)
{
    return writeNumber(
        eui64, endpointId, clusterId, false, 0xFFFF, toServer, attributeId, attributeType, value, numBytes);
}

int zigbeeSubsystemWriteNumberMfgSpecific(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          uint16_t mfgId,
                                          bool toServer,
                                          uint16_t attributeId,
                                          ZigbeeAttributeType attributeType,
                                          uint64_t value,
                                          uint8_t numBytes)
{
    return writeNumber(
        eui64, endpointId, clusterId, true, mfgId, toServer, attributeId, attributeType, value, numBytes);
}

int zigbeeSubsystemBindingSet(uint64_t eui64, uint8_t endpointId, uint16_t clusterId)
{
    return zhalBindingSet(eui64, endpointId, clusterId);
}

icLinkedList *zigbeeSubsystemBindingGet(uint64_t eui64)
{
    return zhalBindingGet(eui64);
}

int zigbeeSubsystemBindingClear(uint64_t eui64, uint8_t endpointId, uint16_t clusterId)
{
    return zhalBindingClear(eui64, endpointId, clusterId);
}

int zigbeeSubsystemBindingClearTarget(uint64_t eui64,
                                      uint8_t endpointId,
                                      uint16_t clusterId,
                                      uint64_t targetEui64,
                                      uint8_t targetEndpointId)
{
    return zhalBindingClearTarget(eui64, endpointId, clusterId, targetEui64, targetEndpointId);
}


int zigbeeSubsystemAttributesSetReporting(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          zhalAttributeReportingConfig *configs,
                                          uint8_t numConfigs)
{
    return zhalAttributesSetReporting(eui64, endpointId, clusterId, configs, numConfigs);
}

int zigbeeSubsystemAttributesSetReportingMfgSpecific(uint64_t eui64,
                                                     uint8_t endpointId,
                                                     uint16_t clusterId,
                                                     uint16_t mfgId,
                                                     zhalAttributeReportingConfig *configs,
                                                     uint8_t numConfigs)
{
    return zhalAttributesSetReportingMfgSpecific(eui64, endpointId, clusterId, mfgId, configs, numConfigs);
}

int zigbeeSubsystemGetEndpointIds(uint64_t eui64, uint8_t **endpointIds, uint8_t *numEndpointIds)
{
    return zhalGetEndpointIds(eui64, endpointIds, numEndpointIds);
}

int zigbeeSubsystemDiscoverAttributes(uint64_t eui64,
                                      uint8_t endpointId,
                                      uint16_t clusterId,
                                      bool toServer,
                                      zhalAttributeInfo **infos,
                                      uint16_t *numInfos)
{
    return zhalGetAttributeInfos(eui64, endpointId, clusterId, toServer, infos, numInfos);
}

uint64_t getLocalEui64(void)
{
    return loadLocalEui64();
}

static uint64_t loadLocalEui64(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    uint64_t localEui64 = 0;

    g_autoptr(BCorePropertyProvider) propProvider = deviceServiceConfigurationGetPropertyProvider();
    g_autofree gchar *localEui64String =
        b_core_property_provider_get_property_as_string(propProvider, LOCAL_EUI64_PROP_KEY, NULL);

    errno = 0;
    gchar *endptr = NULL;
    localEui64 = g_ascii_strtoull(localEui64String, &endptr, 16);
    if (errno > 0 || (localEui64 == 0 && endptr == localEui64String))
    {
        icLogWarn(LOG_TAG,
                  "Error converting property value \"%s\" to uint64: %s",
                  localEui64String,
                  errno > 0 ? g_strerror(errno) : "Not a number");
    }

    return localEui64;
}

int zigbeeSubsystemSetAddresses(void)
{
    // get all paired zigbee devices and set their addresses in zigbee core
    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);

    uint16_t numEui64s = linkedListCount(devices);

    if (numEui64s > 0)
    {
        zhalDeviceEntry *deviceEntries = calloc(numEui64s, sizeof(zhalDeviceEntry));

        int i = 0;

        icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
        while (linkedListIteratorHasNext(iterator))
        {
            icDevice *device = linkedListIteratorGetNext(iterator);

            deviceEntries[i].eui64 = zigbeeSubsystemIdToEui64(device->uuid);

            if (isDeviceAutoApsAcked(device))
            {
                deviceEntries[i].flags.bits.isAutoApsAcked = 1;
            }

            if (isDeviceUsingHashBasedLinkKey(device))
            {
                deviceEntries[i].flags.bits.useHashBasedLinkKey = 1;
            }
            else
            {
                // check to see if we got an early notification that this device uses hash based link key
                pthread_mutex_lock(&earlyHashBasedLinkKeyDevicesMtx);
                if (hashMapDelete(earlyHashedBasedLinkKeyDevices, device->uuid, strlen(device->uuid), NULL) == true)
                {
                    deviceEntries[i].flags.bits.useHashBasedLinkKey = 1;

                    // since we found it here, that means that it has not yet been set as metadata on the device
                    //  set it now.  Don't send to zhal from here since we do it below.
                    //  we only add device to earlyHashedBasedLinkKeyDevices if it has flag set. so mark as using it.
                    setDeviceUsingHashBasedLinkKey(device, true, true);
                }
                pthread_mutex_unlock(&earlyHashBasedLinkKeyDevicesMtx);
            }

            if (shouldUpgradeToHashBasedLinkKey(device) == false)
            {
                deviceEntries[i].flags.bits.doNotUpgradeToHashBasedLinkKey = 1;
            }

            i++;
        }
        linkedListIteratorDestroy(iterator);

        zhalSetDevices(deviceEntries, numEui64s);

        free(deviceEntries);
    }

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    return 0;
}

int zigbeeSubsystemRemoveDeviceAddress(uint64_t eui64)
{
    return zhalRemoveDeviceAddress(eui64);
}

static void prematureClusterCommandsFreeFunc(void *key, void *value)
{
    icLinkedList *list = (icLinkedList *) value;
    linkedListDestroy(list, (linkedListItemFreeFunc) freeReceivedClusterCommand);
    free(key);
}

char *zigbeeSubsystemEui64ToId(uint64_t eui64)
{
    char *result = (char *) malloc(21); // max uint64_t
    sprintf(result, "%016" PRIx64, eui64);
    return result;
}

char *zigbeeSubsystemEndpointIdAsString(uint8_t epId)
{
    /* max uint8_t + NULL */
    char *endpoinId = malloc(4);
    sprintf(endpoinId, "%" PRIu8, epId);
    return endpoinId;
}

uint64_t zigbeeSubsystemIdToEui64(const char *uuid)
{
    uint64_t eui64 = 0;

    if (sscanf(uuid, "%016" PRIx64, &eui64) != 1)
    {
        icLogError(LOG_TAG, "idToEui64: failed to parse %s", uuid);
    }

    return eui64;
}

/*
 * Create (if necessary) the directory where firmware files are stored, and return the path.
 * Caller is responsible for freeing the result
 */
char *zigbeeSubsystemGetAndCreateFirmwareFileDirectory(DeviceFirmwareType firmwareType)
{
    g_autofree gchar *firmwareFileRootDir = deviceServiceConfigurationGetFirmwareFileDir();

    char *lastSubdir =
        firmwareType == DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY ? LEGACY_FIRMWARE_SUBDIR : OTA_FIRMWARE_SUBDIR;
    // 2 path delimiters and NULL terminator
    int directoryLen = 3 + strlen(firmwareFileRootDir) + strlen(ZIGBEE_FIRMWARE_SUBDIR) + strlen(lastSubdir);
    char *firmwareFileDirectory = (char *) malloc(sizeof(char) * directoryLen);

    sprintf(firmwareFileDirectory, "%s/%s/%s", firmwareFileRootDir, ZIGBEE_FIRMWARE_SUBDIR, lastSubdir);

    struct stat buffer;
    if (stat(firmwareFileDirectory, &buffer) != 0)
    {
        // Create directories if they don't exist
        if (mkdir_p(firmwareFileDirectory, 0777) != 0)
        {
            icLogError(LOG_TAG, "Failed to create firmware directory %s with error %d", firmwareFileDirectory, errno);
            // Cleanup and return NULL to signal error
            free(firmwareFileDirectory);
            firmwareFileDirectory = NULL;
        }
    }

    return firmwareFileDirectory;
}

IcDiscoveredDeviceDetails *createIcDiscoveredDeviceDetails(void)
{
    IcDiscoveredDeviceDetails *details = calloc(1, sizeof(IcDiscoveredDeviceDetails));

    return details;
}

static void cloneIcDiscoveredClusterDetails(const IcDiscoveredClusterDetails *original,
                                            IcDiscoveredClusterDetails *target)
{
    // copy the static stuff
    memcpy(target, original, sizeof(IcDiscoveredClusterDetails));
    target->numAttributeIds = original->numAttributeIds;
    // Passing 0 for calloc nmemb param can actually cause memory to be allocated.  Skip that if nothing is there.
    if (original->numAttributeIds > 0 && original->attributeIds != NULL)
    {
        target->attributeIds = (uint16_t *) calloc(original->numAttributeIds, sizeof(uint16_t));
        memcpy(target->attributeIds, original->attributeIds, original->numAttributeIds * sizeof(uint16_t));
        // Copy attribute values if they are there
        if (original->attributeValues != NULL)
        {
            target->attributeValues =
                (IcDiscoveredAttributeValue *) calloc(original->numAttributeIds, sizeof(IcDiscoveredAttributeValue));
            for (int i = 0; i < target->numAttributeIds; ++i)
            {
                target->attributeValues[i].attributeType = original->attributeValues[i].attributeType;
                target->attributeValues[i].dataLen = original->attributeValues[i].dataLen;
                if (target->attributeValues[i].dataLen > 0)
                {
                    target->attributeValues[i].data = malloc(original->attributeValues[i].dataLen);
                    memcpy(target->attributeValues[i].data,
                           original->attributeValues[i].data,
                           original->attributeValues[i].dataLen);
                }
            }
        }
        else
        {
            target->attributeValues = NULL;
        }
    }
    else
    {
        target->numAttributeIds = 0;
        target->attributeIds = NULL;
        target->attributeValues = NULL;
    }
}

static void cloneIcDiscoveredEndpointDetails(const IcDiscoveredEndpointDetails *original,
                                             IcDiscoveredEndpointDetails *target)
{
    // copy the static stuff
    memcpy(target, original, sizeof(IcDiscoveredEndpointDetails));

    // deal with dynamic parts

    // server clusters
    target->numServerClusterDetails = original->numServerClusterDetails;
    // Passing 0 for calloc nmemb param can actually cause memory to be allocated.  Skip that if nothing is there.
    if (original->numServerClusterDetails > 0 && original->serverClusterDetails != NULL)
    {
        target->serverClusterDetails = (IcDiscoveredClusterDetails *) calloc(original->numServerClusterDetails,
                                                                             sizeof(IcDiscoveredClusterDetails));
        memcpy(target->serverClusterDetails,
               original->serverClusterDetails,
               original->numServerClusterDetails * sizeof(IcDiscoveredClusterDetails));

        for (int i = 0; i < original->numServerClusterDetails; i++)
        {
            cloneIcDiscoveredClusterDetails(&original->serverClusterDetails[i], &target->serverClusterDetails[i]);
        }
    }
    else
    {
        target->numServerClusterDetails = 0;
        target->serverClusterDetails = NULL;
    }

    // client clusters
    target->numClientClusterDetails = original->numClientClusterDetails;
    // Passing 0 for calloc nmemb param can actually cause memory to be allocated.  Skip that if nothing is there.
    if (original->numClientClusterDetails > 0 && original->clientClusterDetails != NULL)
    {
        target->clientClusterDetails = (IcDiscoveredClusterDetails *) calloc(original->numClientClusterDetails,
                                                                             sizeof(IcDiscoveredClusterDetails));
        memcpy(target->clientClusterDetails,
               original->clientClusterDetails,
               original->numClientClusterDetails * sizeof(IcDiscoveredClusterDetails));

        for (int i = 0; i < original->numClientClusterDetails; i++)
        {
            cloneIcDiscoveredClusterDetails(&original->clientClusterDetails[i], &target->clientClusterDetails[i]);
        }
    }
    else
    {
        target->numClientClusterDetails = 0;
        target->clientClusterDetails = NULL;
    }
}

IcDiscoveredDeviceDetails *cloneIcDiscoveredDeviceDetails(const IcDiscoveredDeviceDetails *original)
{
    if (original == NULL)
    {
        return NULL;
    }

    IcDiscoveredDeviceDetails *result = (IcDiscoveredDeviceDetails *) calloc(1, sizeof(IcDiscoveredDeviceDetails));
    memcpy(result, original, sizeof(IcDiscoveredDeviceDetails));
    if (original->manufacturer != NULL)
    {
        result->manufacturer = strdup(original->manufacturer);
    }
    if (original->model != NULL)
    {
        result->model = strdup(original->model);
    }
    if (original->numEndpoints > 0 && original->endpointDetails != NULL)
    {
        result->endpointDetails =
            (IcDiscoveredEndpointDetails *) calloc(original->numEndpoints, sizeof(IcDiscoveredEndpointDetails));
        result->numEndpoints = original->numEndpoints;

        for (int i = 0; i < original->numEndpoints; i++)
        {
            cloneIcDiscoveredEndpointDetails(&original->endpointDetails[i], &result->endpointDetails[i]);
        }
    }
    else
    {
        result->numEndpoints = 0;
        result->endpointDetails = NULL;
    }

    return result;
}

void freeIcDiscoveredDeviceDetails(IcDiscoveredDeviceDetails *details)
{
    if (details != NULL)
    {
        free(details->manufacturer);
        free(details->model);

        for (int i = 0; i < details->numEndpoints; i++)
        {
            for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails; j++)
            {
                free(details->endpointDetails[i].serverClusterDetails[j].attributeIds);
                if (details->endpointDetails[i].serverClusterDetails[j].attributeValues != NULL)
                {
                    for (int k = 0; k < details->endpointDetails[i].serverClusterDetails[j].numAttributeIds; ++k)
                    {
                        free(details->endpointDetails[i].serverClusterDetails[j].attributeValues[k].data);
                    }
                    free(details->endpointDetails[i].serverClusterDetails[j].attributeValues);
                }
            }
            free(details->endpointDetails[i].serverClusterDetails);

            for (int j = 0; j < details->endpointDetails[i].numClientClusterDetails; j++)
            {
                free(details->endpointDetails[i].clientClusterDetails[j].attributeIds);
                if (details->endpointDetails[i].clientClusterDetails[j].attributeValues != NULL)
                {
                    for (int k = 0; k < details->endpointDetails[i].clientClusterDetails[j].numAttributeIds; ++k)
                    {
                        free(details->endpointDetails[i].clientClusterDetails[j].attributeValues[k].data);
                    }
                    free(details->endpointDetails[i].clientClusterDetails[j].attributeValues);
                }
            }
            free(details->endpointDetails[i].clientClusterDetails);
        }

        free(details->endpointDetails);
        free(details);
    }
}

static cJSON *icDiscoveredClusterDetailsToJson(const IcDiscoveredClusterDetails *details)
{
    cJSON *cluster = cJSON_CreateObject();

    cJSON_AddNumberToObject(cluster, ID_JSON_PROP, details->clusterId);
    cJSON_AddBoolToObject(cluster, IS_SERVER_JSON_PROP, details->isServer);

    cJSON *attributeIds = cJSON_CreateArray();
    for (int i = 0; i < details->numAttributeIds; i++)
    {
        cJSON_AddItemToArray(attributeIds, cJSON_CreateNumber(details->attributeIds[i]));
    }
    cJSON_AddItemToObject(cluster, ATTRIBUTE_IDS_JSON_PROP, attributeIds);

    return cluster;
}

static cJSON *icDiscoveredEndpointDetailsToJson(const IcDiscoveredEndpointDetails *details)
{
    cJSON *endpoint = cJSON_CreateObject();

    cJSON_AddNumberToObject(endpoint, ID_JSON_PROP, details->endpointId);
    cJSON_AddNumberToObject(endpoint, PROFILEID_JSON_PROP, details->appProfileId);
    cJSON_AddNumberToObject(endpoint, DEVICEID_JSON_PROP, details->appDeviceId);
    cJSON_AddNumberToObject(endpoint, DEVICEVER_JSON_PROP, details->appDeviceVersion);

    cJSON *serverClusterInfos = cJSON_CreateArray();
    for (int j = 0; j < details->numServerClusterDetails; j++)
    {
        cJSON_AddItemToArray(serverClusterInfos, icDiscoveredClusterDetailsToJson(&details->serverClusterDetails[j]));
    }
    cJSON_AddItemToObject(endpoint, SERVERCLUSTERINFOS_JSON_PROP, serverClusterInfos);

    cJSON *clientClusterInfos = cJSON_CreateArray();
    for (int j = 0; j < details->numClientClusterDetails; j++)
    {
        cJSON_AddItemToArray(clientClusterInfos, icDiscoveredClusterDetailsToJson(&details->clientClusterDetails[j]));
    }
    cJSON_AddItemToObject(endpoint, CLIENTCLUSTERINFOS_JSON_PROP, clientClusterInfos);

    return endpoint;
}

cJSON *icDiscoveredDeviceDetailsToJson(const IcDiscoveredDeviceDetails *details)
{
    if (details == NULL)
    {
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();

    char *eui64 = zigbeeSubsystemEui64ToId(details->eui64);
    cJSON_AddStringToObject(result, EUI64_JSON_PROP, eui64);
    free(eui64);
    cJSON_AddStringToObject(result, MANUF_JSON_PROP, details->manufacturer);
    cJSON_AddStringToObject(result, MODEL_JSON_PROP, details->model);
    cJSON_AddNumberToObject(result, HWVER_JSON_PROP, details->hardwareVersion);
    cJSON_AddNumberToObject(result, FWVER_JSON_PROP, details->firmwareVersion);
    cJSON_AddNumberToObject(result, APPVER_JSON_PROP, details->appVersion);

    cJSON *endpoints = cJSON_CreateArray();

    for (int i = 0; i < details->numEndpoints; i++)
    {
        cJSON_AddItemToArray(endpoints, icDiscoveredEndpointDetailsToJson(&details->endpointDetails[i]));
    }

    char *deviceType;
    switch (details->deviceType)
    {
        case deviceTypeEndDevice:
            deviceType = ENDDEVICE_JSON_PROP;
            break;

        case deviceTypeRouter:
            deviceType = ROUTERDEVICE_JSON_PROP;
            break;

        case deviceTypeUnknown:
        default:
            deviceType = UNKNOWN_JSON_PROP;
            break;
    }
    cJSON_AddStringToObject(result, DEVICETYPE_JSON_PROP, deviceType);

    char *powerSource;
    switch (details->powerSource)
    {
        case powerSourceMains:
            powerSource = MAINS_JSON_PROP;
            break;

        case powerSourceBattery:
            powerSource = BATT_JSON_PROP;
            break;

        case powerSourceUnknown:
        default:
            powerSource = UNKNOWN_JSON_PROP;
            break;
    }
    cJSON_AddStringToObject(result, POWERSOURCE_JSON_PROP, powerSource);

    cJSON_AddItemToObject(result, ENDPOINTS_JSON_PROP, endpoints);

    return result;
}

static bool icDiscoveredClusterDetailsFromJson(const cJSON *detailsJson, IcDiscoveredClusterDetails *details)
{
    bool success = true;

    cJSON *attributeIds = NULL;
    cJSON *attributeId = NULL;
    int j = 0;

    int tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, ID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->clusterId = (uint16_t) tmpInt;

    if (!(success = getCJSONBool(detailsJson, IS_SERVER_JSON_PROP, &details->isServer)))
    {
        goto exit;
    }

    attributeIds = cJSON_GetObjectItem(detailsJson, ATTRIBUTE_IDS_JSON_PROP);
    if (attributeIds == NULL)
    {
        success = false;
        goto exit;
    }

    details->numAttributeIds = (uint8_t) cJSON_GetArraySize(attributeIds);
    details->attributeIds = (uint16_t *) calloc(details->numAttributeIds, sizeof(uint16_t));
    cJSON_ArrayForEach(attributeId, attributeIds)
    {
        details->attributeIds[j++] = (uint16_t) attributeId->valueint;
    }

exit:
    return success;
}

static bool icDiscoveredEndpointDetailsFromJson(const cJSON *detailsJson, IcDiscoveredEndpointDetails *details)
{
    bool success = true;

    cJSON *serverClusterInfos = NULL;
    cJSON *serverClusterInfo = NULL;
    cJSON *clientClusterInfos = NULL;
    cJSON *clientClusterInfo = NULL;
    int j = 0;

    int tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, ID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->endpointId = (uint8_t) tmpInt;

    tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, PROFILEID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->appProfileId = (uint16_t) tmpInt;

    tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, DEVICEID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->appDeviceId = (uint16_t) tmpInt;

    tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, DEVICEVER_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->appDeviceVersion = (uint8_t) tmpInt;

    // server cluster infos
    serverClusterInfos = cJSON_GetObjectItem(detailsJson, SERVERCLUSTERINFOS_JSON_PROP);
    if (serverClusterInfos == NULL)
    {
        success = false;
        goto exit;
    }
    details->numServerClusterDetails = (uint8_t) cJSON_GetArraySize(serverClusterInfos);
    details->serverClusterDetails =
        (IcDiscoveredClusterDetails *) calloc(details->numServerClusterDetails, sizeof(IcDiscoveredClusterDetails));
    j = 0;
    cJSON_ArrayForEach(serverClusterInfo, serverClusterInfos)
    {
        if (!(success = icDiscoveredClusterDetailsFromJson(serverClusterInfo, &details->serverClusterDetails[j++])))
        {
            goto exit;
        }
    }

    // client cluster infos
    clientClusterInfos = cJSON_GetObjectItem(detailsJson, CLIENTCLUSTERINFOS_JSON_PROP);
    if (clientClusterInfos == NULL)
    {
        success = false;
        goto exit;
    }
    details->numClientClusterDetails = (uint8_t) cJSON_GetArraySize(clientClusterInfos);
    details->clientClusterDetails =
        (IcDiscoveredClusterDetails *) calloc(details->numClientClusterDetails, sizeof(IcDiscoveredClusterDetails));
    j = 0;
    cJSON_ArrayForEach(clientClusterInfo, clientClusterInfos)
    {
        if (!(success = icDiscoveredClusterDetailsFromJson(clientClusterInfo, &details->clientClusterDetails[j++])))
        {
            goto exit;
        }
    }

exit:
    return success;
}

IcDiscoveredDeviceDetails *icDiscoveredDeviceDetailsFromJson(const cJSON *detailsJson)
{
    bool success = true;
    cJSON *endpoints = NULL;
    cJSON *endpoint = NULL;
    char *deviceType = NULL;
    char *powerSource = NULL;
    double tmpVal = 0;
    int i = 0;

    if (detailsJson == NULL)
    {
        return NULL;
    }

    IcDiscoveredDeviceDetails *result = (IcDiscoveredDeviceDetails *) calloc(1, sizeof(IcDiscoveredDeviceDetails));

    char *eui64Str = getCJSONString(detailsJson, EUI64_JSON_PROP);
    if (eui64Str == NULL)
    {
        success = false;
        goto exit;
    }
    result->eui64 = zigbeeSubsystemIdToEui64(eui64Str);
    free(eui64Str);

    result->manufacturer = getCJSONString(detailsJson, MANUF_JSON_PROP);
    if (result->manufacturer == NULL)
    {
        success = false;
        goto exit;
    }

    result->model = getCJSONString(detailsJson, MODEL_JSON_PROP);
    if (result->model == NULL)
    {
        success = false;
        goto exit;
    }

    tmpVal = 0;
    if (!(success = getCJSONDouble(detailsJson, HWVER_JSON_PROP, &tmpVal)))
    {
        goto exit;
    }
    result->hardwareVersion = (uint64_t) tmpVal;

    tmpVal = 0;
    if (!(success = getCJSONDouble(detailsJson, FWVER_JSON_PROP, &tmpVal)))
    {
        goto exit;
    }
    result->firmwareVersion = (uint64_t) tmpVal;

    tmpVal = 0;
    if (!(success = getCJSONDouble(detailsJson, APPVER_JSON_PROP, &tmpVal)))
    {
        goto exit;
    }
    result->appVersion = (uint64_t) tmpVal;

    endpoints = cJSON_GetObjectItem(detailsJson, ENDPOINTS_JSON_PROP);
    if (endpoints == NULL)
    {
        success = false;
        goto exit;
    }

    result->numEndpoints = (uint8_t) cJSON_GetArraySize(endpoints);
    result->endpointDetails =
        (IcDiscoveredEndpointDetails *) calloc(result->numEndpoints, sizeof(IcDiscoveredEndpointDetails));
    i = 0;
    cJSON_ArrayForEach(endpoint, endpoints)
    {
        if (!(success = icDiscoveredEndpointDetailsFromJson(endpoint, &result->endpointDetails[i])))
        {
            goto exit;
        }

        i++;
    }

    deviceType = getCJSONString(detailsJson, DEVICETYPE_JSON_PROP);
    if (deviceType == NULL)
    {
        success = false;
        goto exit;
    }
    if (strcmp(deviceType, ENDDEVICE_JSON_PROP) == 0)
    {
        result->deviceType = deviceTypeEndDevice;
    }
    else if (strcmp(deviceType, ROUTERDEVICE_JSON_PROP) == 0)
    {
        result->deviceType = deviceTypeRouter;
    }
    else
    {
        result->deviceType = deviceTypeUnknown;
    }
    free(deviceType);

    powerSource = getCJSONString(detailsJson, POWERSOURCE_JSON_PROP);
    if (powerSource == NULL)
    {
        success = false;
        goto exit;
    }
    if (strcmp(powerSource, MAINS_JSON_PROP) == 0)
    {
        result->powerSource = powerSourceMains;
    }
    else if (strcmp(powerSource, BATT_JSON_PROP) == 0)
    {
        result->powerSource = powerSourceBattery;
    }
    else
    {
        result->powerSource = powerSourceUnknown;
    }
    free(powerSource);

exit:
    if (!success)
    {
        icLogError(LOG_TAG, "%s: failed to parse", __FUNCTION__);
        freeIcDiscoveredDeviceDetails(result);
        result = NULL;
    }
    else
    {
        zigbeeSubsystemDumpDeviceDiscovered(result);
    }
    return result;
}

static void freeFirmwareFilesMap(void *key, void *value)
{
    // key is the firmware file, which is contained within the device descriptor which is the value
    DeviceDescriptor *dd = (DeviceDescriptor *) value;
    deviceDescriptorFree(dd);
}

static void cleanupFirmwareFilesByType(DeviceFirmwareType deviceFirmwareType, icHashMap *allFirmwareFiles)
{
    // Get the path to the firmware files for this type
    char *dirPath = zigbeeSubsystemGetAndCreateFirmwareFileDirectory(deviceFirmwareType);

    icLogDebug(LOG_TAG, "Checking for firmware files to cleanup in %s", dirPath);

    DIR *dir = opendir(dirPath);

    if (dir != NULL)
    {
        struct dirent *dp;
        while ((dp = readdir(dir)) != NULL)
        {
            char *filePath = malloc(sizeof(char) * (strlen(dirPath) + 1 + strlen(dp->d_name) + 1));
            sprintf(filePath, "%s/%s", dirPath, dp->d_name);
            struct stat statBuf;
            if (lstat(filePath, &statBuf) == 0)
            {
                // Skip directories and symlinks
                if ((statBuf.st_mode & S_IFMT) != S_IFDIR && (statBuf.st_mode & S_IFMT) != S_IFLNK)
                {
                    // Check if we still need the file
                    DeviceDescriptor *dd =
                        (DeviceDescriptor *) hashMapGet(allFirmwareFiles, dp->d_name, strlen(dp->d_name) + 1);
                    // Either it wasn't found, or the file is of a different firmware type(and as such is in another
                    // directory) This second case is weird, but just covers the fact that you could technically have
                    // legacy and OTA firmware files with the same name
                    if (dd == NULL || dd->latestFirmware->type != deviceFirmwareType)
                    {
                        if (remove(filePath) == 0)
                        {
                            icLogInfo(LOG_TAG, "Removed unused firmware file %s", filePath);
                        }
                        else
                        {
                            icLogError(LOG_TAG, "Failed to remove firmware file %s", filePath);
                        }
                    }
                    else
                    {
                        icLogDebug(LOG_TAG, "Firmware file %s is still needed", filePath);
                    }
                }
            }
            else
            {
                icLogDebug(LOG_TAG, "Unable to stat file at path %s", filePath);
            }
            free(filePath);
        }
        closedir(dir);
    }
    else
    {
        icLogError(LOG_TAG, "Could not read firmware files in directory %s", dirPath);
    }


    free(dirPath);
}

static bool findDeviceResource(void *searchVal, void *item)
{
    icDeviceResource *resourceItem = (icDeviceResource *) item;
    return strcmp(searchVal, resourceItem->id) == 0;
}

/*
 * Cleanup any unused firmware files
 */
void zigbeeSubsystemCleanupFirmwareFiles(void)
{
    icLogDebug(LOG_TAG, "Scanning for unused firmware files...");

    // Get all our devices
    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);

    // Build all firmware files to retain
    icHashMap *allFirmwareFiles = hashMapCreate();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
        DeviceDescriptor *dd = deviceServiceGetDeviceDescriptorForDevice(device);
        if (dd != NULL && dd->latestFirmware != NULL && dd->latestFirmware->fileInfos != NULL &&
            dd->latestFirmware->version != NULL)
        {
            icLogDebug(LOG_TAG, "For device %s, found device descriptor uuid %s", device->uuid, dd->uuid);
            // Get the device's current firmware version, we only want to keep what we still need, e.g.
            // if there is a newer firmware version that we haven't upgraded to yet.
            icDeviceResource *firmwareVersionResource = (icDeviceResource *) linkedListFind(
                device->resources, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, findDeviceResource);
            if (firmwareVersionResource != NULL && firmwareVersionResource->value != NULL)
            {
                icLogDebug(LOG_TAG,
                           "For device %s we are at version %s, latest version is %s",
                           device->uuid,
                           firmwareVersionResource->value,
                           dd->latestFirmware->version);
                // Check if latest version is newer than our version
                int versionComparison =
                    compareVersionStrings(dd->latestFirmware->version, firmwareVersionResource->value);
                if (versionComparison == -1)
                {
                    icLinkedListIterator *fileIter = linkedListIteratorCreate(dd->latestFirmware->fileInfos);
                    while (linkedListIteratorHasNext(fileIter) == true)
                    {
                        DeviceFirmwareFileInfo *info = (DeviceFirmwareFileInfo *) linkedListIteratorGetNext(fileIter);

                        icLogDebug(LOG_TAG, "For device %s we need firmware file %s", device->uuid, info->fileName);
                        // Have to put clones in the map in case there is more than one filename
                        DeviceDescriptor *ddClone = deviceDescriptorClone(dd);
                        // Have to use the clone's filename as the key for memory reasons.
                        icLinkedListIterator *cloneFileIter =
                            linkedListIteratorCreate(ddClone->latestFirmware->fileInfos);
                        while (linkedListIteratorHasNext(cloneFileIter) == true)
                        {
                            DeviceFirmwareFileInfo *cloneInfo =
                                (DeviceFirmwareFileInfo *) linkedListIteratorGetNext(cloneFileIter);
                            if (stringCompare(info->fileName, cloneInfo->fileName, true) == 0)
                            {
                                hashMapPut(allFirmwareFiles, cloneInfo->fileName, strlen(info->fileName) + 1, ddClone);
                                ddClone = NULL;
                            }
                        }
                        linkedListIteratorDestroy(cloneFileIter);
                        deviceDescriptorFree(ddClone);
                    }
                    linkedListIteratorDestroy(fileIter);
                }
            }
        }
        // The map will clean up the clones, but we need to free this copy here.
        deviceDescriptorFree(dd);
    }
    // Go ahead and cleanup devices as we don't need those anymore
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    // Cleanup both types
    cleanupFirmwareFilesByType(DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY, allFirmwareFiles);
    cleanupFirmwareFilesByType(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA, allFirmwareFiles);

    // Cleanup our hashmap
    hashMapDestroy(allFirmwareFiles, freeFirmwareFilesMap);
}

static void prematureClusterCommandFreeKeyFunc(void *key, void *value)
{
    (void) value; // unused
    // only free the key (an eui64)
    free(key);
}

icLinkedList *zigbeeSubsystemGetPrematureClusterCommands(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *result = NULL;
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    result = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
    hashMapDelete(prematureClusterCommands, &eui64, sizeof(uint64_t), prematureClusterCommandFreeKeyFunc);
    pthread_mutex_unlock(&prematureClusterCommandsMtx);
    return result;
}

void zigbeeSubsystemDestroyPrematureClusterCommands(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *result = NULL;
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    result = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
    if (result)
    {
        hashMapDelete(prematureClusterCommands, &eui64, sizeof(uint64_t), prematureClusterCommandFreeKeyFunc);
        linkedListDestroy(result, (linkedListItemFreeFunc) freeReceivedClusterCommand);
    }
    pthread_mutex_unlock(&prematureClusterCommandsMtx);
}

ReceivedClusterCommand *
zigbeeSubsystemGetPrematureClusterCommand(uint64_t eui64, uint8_t commandId, uint32_t timeoutSeconds)
{
    icLogDebug(LOG_TAG,
               "%s: looking for command 0x%02x for %016" PRIx64 " for %" PRIu32 " seconds",
               __FUNCTION__,
               commandId,
               eui64,
               timeoutSeconds);

    ReceivedClusterCommand *result = NULL;
    uint32_t iterations = 0;

    pthread_mutex_lock(&prematureClusterCommandsMtx);
    while (result == NULL && iterations++ < timeoutSeconds)
    {
        icLinkedList *commands = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
        if (commands)
        {
            icLinkedListIterator *it = linkedListIteratorCreate(commands);
            while (linkedListIteratorHasNext(it))
            {
                ReceivedClusterCommand *command = linkedListIteratorGetNext(it);
                if (command->commandId == commandId)
                {
                    icLogDebug(LOG_TAG, "%s: found the command", __FUNCTION__);
                    result = command;
                    break;
                }
            }
            linkedListIteratorDestroy(it);
        }

        if (result == NULL)
        {
            incrementalCondTimedWait(&prematureClusterCommandsCond, &prematureClusterCommandsMtx, 1);
        }
    }
    pthread_mutex_unlock(&prematureClusterCommandsMtx);

    return result;
}

void zigbeeSubsystemRemovePrematureClusterCommand(uint64_t eui64, uint8_t commandId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *result = NULL;
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    result = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
    if (result)
    {
        icLinkedListIterator *it = linkedListIteratorCreate(result);
        while (linkedListIteratorHasNext(it))
        {
            ReceivedClusterCommand *command = linkedListIteratorGetNext(it);
            if (command->commandId == commandId)
            {
                linkedListIteratorDeleteCurrent(it, (linkedListItemFreeFunc) freeReceivedClusterCommand);
            }
        }
        linkedListIteratorDestroy(it);

        // if the list is now empty, go ahead and destroy the whole enchilada
        if (linkedListCount(result) == 0)
        {
            hashMapDelete(prematureClusterCommands, &eui64, sizeof(uint64_t), prematureClusterCommandFreeKeyFunc);
            linkedListDestroy(result, (linkedListItemFreeFunc) freeReceivedClusterCommand);
        }
    }
    pthread_mutex_unlock(&prematureClusterCommandsMtx);
}

/**
 * Add a premature cluster command
 * @param command the command to add
 */
void zigbeeSubsystemAddPrematureClusterCommand(const ReceivedClusterCommand *command)
{
    if (command != NULL)
    {
        icLogDebug(LOG_TAG, "Adding premature cluster command for device %016" PRIx64, command->eui64);

        // save it
        pthread_mutex_lock(&prematureClusterCommandsMtx);

        if (prematureClusterCommands == NULL)
        {
            prematureClusterCommands = hashMapCreate();
        }

        uint64_t eui64 = command->eui64;

        icLinkedList *list = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
        if (list == NULL)
        {
            list = linkedListCreate();
            uint64_t *eui = (uint64_t *) malloc(sizeof(uint64_t));
            *eui = command->eui64;
            hashMapPut(prematureClusterCommands, eui, sizeof(uint64_t), list);
        }
        linkedListAppend(list, receivedClusterCommandClone(command));

        pthread_cond_broadcast(&prematureClusterCommandsCond);
        pthread_mutex_unlock(&prematureClusterCommandsMtx);
    }
}

bool icDiscoveredDeviceDetailsGetAttributeEndpoint(const IcDiscoveredDeviceDetails *details,
                                                   uint16_t clusterId,
                                                   uint16_t attributeId,
                                                   uint8_t *endpointId)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; i < details->numEndpoints && !result; i++)
        {
            for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails && !result; j++)
            {
                IcDiscoveredClusterDetails *clusterDetails = &details->endpointDetails[i].serverClusterDetails[j];

                if (clusterDetails->clusterId == clusterId && clusterDetails->isServer)
                {
                    for (int k = 0; k < clusterDetails->numAttributeIds; k++)
                    {
                        if (clusterDetails->attributeIds[k] == attributeId)
                        {
                            if (endpointId != NULL)
                            {
                                *endpointId = details->endpointDetails[i].endpointId;
                            }
                            result = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsGetClusterEndpoint(const IcDiscoveredDeviceDetails *details,
                                                 uint16_t clusterId,
                                                 uint8_t *endpointId)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; i < details->numEndpoints && !result; i++)
        {
            for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails; j++)
            {
                IcDiscoveredClusterDetails *clusterDetails = &details->endpointDetails[i].serverClusterDetails[j];

                if (clusterDetails->clusterId == clusterId && clusterDetails->isServer)
                {
                    if (endpointId != NULL)
                    {
                        *endpointId = details->endpointDetails[i].endpointId;
                    }
                    result = true;
                    break;
                }
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsEndpointHasCluster(const IcDiscoveredDeviceDetails *details,
                                                 uint8_t endpointId,
                                                 uint16_t clusterId,
                                                 bool wantServer)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; i < details->numEndpoints; i++)
        {
            IcDiscoveredEndpointDetails epDetails = details->endpointDetails[i];
            if (epDetails.endpointId == endpointId)
            {
                int detailsLen = wantServer ? epDetails.numServerClusterDetails : epDetails.numClientClusterDetails;
                for (int j = 0; j < detailsLen; j++)
                {
                    IcDiscoveredClusterDetails *clusterDetails =
                        wantServer ? &epDetails.serverClusterDetails[j] : &epDetails.clientClusterDetails[j];

                    if (clusterDetails->clusterId == clusterId && clusterDetails->isServer == wantServer)
                    {
                        result = true;
                        break;
                    }
                }

                break;
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsClusterHasAttribute(const IcDiscoveredDeviceDetails *details,
                                                  uint8_t endpointId,
                                                  uint16_t clusterId,
                                                  bool wantServer,
                                                  uint16_t attributeId)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; result == false && i < details->numEndpoints; i++)
        {
            if (details->endpointDetails[i].endpointId == endpointId)
            {
                IcDiscoveredEndpointDetails epDetails = details->endpointDetails[i];
                int detailsLen = wantServer ? epDetails.numServerClusterDetails : epDetails.numClientClusterDetails;
                for (int j = 0; j < detailsLen; j++)
                {
                    IcDiscoveredClusterDetails *clusterDetails =
                        wantServer ? &epDetails.serverClusterDetails[j] : &epDetails.clientClusterDetails[j];

                    if (clusterDetails->clusterId == clusterId)
                    {
                        for (int k = 0; k < clusterDetails->numAttributeIds; k++)
                        {
                            if (clusterDetails->attributeIds[k] == attributeId)
                            {
                                result = true;
                                break;
                            }
                        }
                        // Found the cluster we want, we are done
                        break;
                    }
                }
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsClusterGetAttributeValue(const IcDiscoveredDeviceDetails *details,
                                                       uint8_t endpointId,
                                                       uint16_t clusterId,
                                                       bool wantServer,
                                                       uint16_t attributeId,
                                                       IcDiscoveredAttributeValue **attributeValue)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; result == false && i < details->numEndpoints; i++)
        {
            if (details->endpointDetails[i].endpointId == endpointId)
            {
                IcDiscoveredEndpointDetails epDetails = details->endpointDetails[i];
                int detailsLen = wantServer ? epDetails.numServerClusterDetails : epDetails.numClientClusterDetails;
                for (int j = 0; j < detailsLen; j++)
                {
                    IcDiscoveredClusterDetails *clusterDetails =
                        wantServer ? &epDetails.serverClusterDetails[j] : &epDetails.clientClusterDetails[j];

                    if (clusterDetails->clusterId == clusterId)
                    {
                        for (int k = 0; k < clusterDetails->numAttributeIds; k++)
                        {
                            if (clusterDetails->attributeIds[k] == attributeId)
                            {
                                if (clusterDetails->attributeValues != NULL)
                                {
                                    *(attributeValue) = &clusterDetails->attributeValues[k];
                                    result = true;
                                }
                                break;
                            }
                        }
                        // Found the cluster we want, we are done
                        break;
                    }
                }
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsEndpointGetDeviceId(const IcDiscoveredDeviceDetails *details,
                                                  uint8_t endpointId,
                                                  uint16_t *deviceId)
{
    bool result = false;

    if (details != NULL && deviceId != NULL)
    {
        for (int i = 0; i < details->numEndpoints; i++)
        {
            if (details->endpointDetails[i].endpointId == endpointId)
            {
                *deviceId = details->endpointDetails[i].appDeviceId;
                result = true;
                break;
            }
        }
    }

    return result;
}

int zigbeeSubsystemGetSystemStatus(zhalSystemStatus *status)
{
    if (status == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return -1;
    }

    return zhalGetSystemStatus(status);
}

cJSON *zigbeeSubsystemGetAndClearCounters(void)
{
    return zhalGetAndClearCounters();
}

static uint8_t calculateBestChannel(void)
{
    uint8_t result = 0; // invalid channel default
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    uint32_t scanDurationMillis = b_core_property_provider_get_property_as_uint32(
        propertyProvider, CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DURATION_MS, DEFAULT_ZIGBEE_CHANNEL_SCAN_DUR_MILLIS);

    uint32_t scanCount =
        b_core_property_provider_get_property_as_uint32(propertyProvider,
                                                                  CPE_DIAGNOSTIC_ZIGBEEDATA_PER_CHANNEL_NUMBER_OF_SCANS,
                                                                  DEFAULT_ZIGBEE_PER_CHANNEL_NUMBER_OF_SCANS);

    uint8_t channels[] = {15, 19, 20, 25};
    int8_t bestScore = 0;

    icLinkedList *scanResults = zhalPerformEnergyScan(channels, sizeof(channels), scanDurationMillis, scanCount);
    if (scanResults != NULL)
    {
        sbIcLinkedListIterator *it = linkedListIteratorCreate(scanResults);
        while (linkedListIteratorHasNext(it))
        {
            zhalEnergyScanResult *scanResult = linkedListIteratorGetNext(it);

            if (scanResult->score > bestScore)
            {
                bestScore = scanResult->score;
                result = scanResult->channel;
                icLogDebug(LOG_TAG, "%s: channel %" PRIu8 " is now the best channel", __FUNCTION__, result);
            }
        }

        if (bestScore == 0)
        {
            icLogWarn(LOG_TAG, "%s: no channel had non-zero score, returning invalid channel", __FUNCTION__);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to perform energy scan, returning invalid channel", __FUNCTION__);
    }

    return result;
}

static icHashMap *getDeviceIdsInCommFail()
{
    icHashMap *result = hashMapCreate();

    icLinkedList *allDevices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    sbIcLinkedListIterator *it = linkedListIteratorCreate(allDevices);
    while (linkedListIteratorHasNext(it))
    {
        icDevice *device = linkedListIteratorGetNext(it);

        icDeviceResource *commFailResource =
            deviceServiceGetResourceById(device->uuid, NULL, COMMON_DEVICE_RESOURCE_COMM_FAIL);

        if (commFailResource != NULL && strcmp(commFailResource->value, "true") == 0)
        {
            hashMapPut(result, strdup(device->uuid), strlen(device->uuid), NULL);
        }

        resourceDestroy(commFailResource);
    }

    linkedListDestroy(allDevices, (linkedListItemFreeFunc) deviceDestroy);

    return result;
}

static bool isTimedOut(uint64_t dateOfLastContactMillis, uint64_t maxRejoinTimeoutMillis)
{
    return ((getCurrentUnixTimeMillis() - dateOfLastContactMillis) > maxRejoinTimeoutMillis);
}

typedef struct
{
    icHashMap *deviceIdsPreviouslyInCommFail;
    uint8_t previousChannel;
    uint8_t targetedChannel;
    uint64_t maxRejoinTimeoutMillis;
} ChannelChangeDeviceWatchdogArg;

static void channelChangeDeviceWatchdogTask(void *arg)
{
    uint8_t previousChannel = 0;
    ChannelChangeDeviceWatchdogArg *myArg = (ChannelChangeDeviceWatchdogArg *) arg;
    bool needsToFallBackToPreviousChannel = false;

    icLogDebug(LOG_TAG, "%s: checking to see if all zigbee devices rejoined", __FUNCTION__);

    icLinkedList *allDevices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    pthread_mutex_lock(&channelChangeMutex);

    sbIcLinkedListIterator *it = linkedListIteratorCreate(allDevices);
    while (linkedListIteratorHasNext(it))
    {
        icDevice *device = linkedListIteratorGetNext(it);

        // if we have not heard from this device and it was not in comm fail before the channel
        //  change, then we have a problem and need to change back to the original channel.

        if (hashMapContains(myArg->deviceIdsPreviouslyInCommFail, device->uuid, strlen(device->uuid)) == true)
        {
            icLogDebug(LOG_TAG, "%s: device %s was previously in comm fail -- ignoring", __FUNCTION__, device->uuid);
            continue;
        }

        if (isTimedOut(getDeviceDateLastContacted(device->uuid), myArg->maxRejoinTimeoutMillis) == true)
        {
            icLogWarn(LOG_TAG,
                      "%s: device %s has not joined back in time.  Reverting to previous channel.",
                      __FUNCTION__,
                      device->uuid);

            needsToFallBackToPreviousChannel = true;
            break;
        }
    }

    if (needsToFallBackToPreviousChannel)
    {
        char *channelStr = NULL;
        if (deviceServiceGetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, &channelStr) && channelStr != NULL &&
            strlen(channelStr) > 0 && stringToUint8(channelStr, &previousChannel))
        {
            zhalNetworkChangeRequest networkChangeRequest;
            memset(&networkChangeRequest, 0, sizeof(zhalNetworkChangeRequest));
            networkChangeRequest.channel = previousChannel;

            if (zhalNetworkChange(&networkChangeRequest) == 0)
            {
                icLogDebug(LOG_TAG, "%s: successfully reverted back to channel %d", __FUNCTION__, previousChannel);
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to change back to previous channel", __FUNCTION__);
            }
        }
        else
        {
            icLogError(
                LOG_TAG, "%s: needed to change back to previous channel, but no previous channel found!", __FUNCTION__);
        }
    }
    else
    {
        icLogDebug(LOG_TAG, "%s: channel change request fully completed successfully", __FUNCTION__);
    }

    // set the previous channel to empty string
    deviceServiceSetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, "");
    isChannelChangeInProgress = false;

    sendZigbeeChannelChangedEvent(!needsToFallBackToPreviousChannel,
                                  needsToFallBackToPreviousChannel ? previousChannel : myArg->targetedChannel,
                                  myArg->targetedChannel);

    hashMapDestroy(myArg->deviceIdsPreviouslyInCommFail, NULL);
    free(myArg);

    linkedListDestroy(allDevices, (linkedListItemFreeFunc) deviceDestroy);

    pthread_mutex_unlock(&channelChangeMutex);
}

static void startChannelChangeDeviceWatchdog(uint8_t previousChannel, uint8_t targetedChannel)
{
    // Get the list of device ids for devices that are in comm fail before we even try to change
    //  channels.  These devices wont prevent a channel change if they don't follow to the new
    //  channel.
    icHashMap *deviceIdsInCommFail = getDeviceIdsInCommFail();
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    uint32_t rejoinTimeoutMinutes =
        b_core_property_provider_get_property_as_uint32(propertyProvider,
                                                                  CPE_ZIGBEE_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES,
                                                                  DEFAULT_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES);

    // the task will clean up the deviceIdsInCommFail and the arg instance
    ChannelChangeDeviceWatchdogArg *arg = calloc(1, sizeof(*arg));
    arg->deviceIdsPreviouslyInCommFail = deviceIdsInCommFail;
    arg->previousChannel = previousChannel;
    arg->targetedChannel = targetedChannel;
    arg->maxRejoinTimeoutMillis = rejoinTimeoutMinutes * 60ULL * 1000ULL;
    // read fast timers property
    bool fastTimersForTesting =
        b_core_property_provider_get_property_as_bool(propertyProvider, TEST_FASTTIMERS_PROP, false);
    scheduleDelayTask(
        rejoinTimeoutMinutes, fastTimersForTesting ? DELAY_MILLIS : DELAY_MINS, channelChangeDeviceWatchdogTask, arg);
}

static void restartChannelChangeDeviceWatchdogIfRequired(void)
{
    char *channelStr = NULL;
    uint8_t previousChannel = 0;
    if (deviceServiceGetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, &channelStr) && channelStr != NULL &&
        strlen(channelStr) > 0 && stringToUint8(channelStr, &previousChannel))
    {
        icLogInfo(
            LOG_TAG, "%s: a channel change was in progress, starting channel change watchdog again", __FUNCTION__);

        zhalSystemStatus status;
        memset(&status, 0, sizeof(zhalSystemStatus));
        if (zhalGetSystemStatus(&status) == 0)
        {
            startChannelChangeDeviceWatchdog(previousChannel, status.channel);
        }
        else
        {
            icLogError(LOG_TAG, "%s: unable to restart channel change watchdog", __FUNCTION__);
        }
    }
}

ChannelChangeResponse zigbeeSubsystemChangeChannel(uint8_t channel, bool dryRun)
{
    ChannelChangeResponse result = {.channelNumber = 0, .responseCode = channelChangeFailed};
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    pthread_mutex_lock(&channelChangeMutex);

#ifdef B_CORE_SUPPORT_ALARMS
    AUTO_CLEAN(securityStateDestroy__auto) SecurityState *state = deviceServiceGetSecurityState();
#endif

    if (b_core_property_provider_get_property_as_bool(
            propertyProvider, CPE_ZIGBEE_CHANNEL_CHANGE_ENABLED_KEY, true) == false)
    {
        icLogWarn(LOG_TAG,
                  "%s: attempt to change to channel while %s=false.  Denied",
                  __FUNCTION__,
                  CPE_ZIGBEE_CHANNEL_CHANGE_ENABLED_KEY);
        result.responseCode = channelChangeNotAllowed;
    }
#ifdef B_CORE_SUPPORT_ALARMS
    else if (supportAlarms() == true && state->panelStatus != PANEL_STATUS_DISARMED &&
             state->panelStatus != PANEL_STATUS_UNREADY)
    {
        icLogWarn(LOG_TAG,
                  "%s: attempt to change to channel while panel status %s denied",
                  __FUNCTION__,
                  PanelStatusLabels[state->panelStatus]);
        result.responseCode = channelChangeNotAllowed;
    }
#endif
    else if (isChannelChangeInProgress)
    {
        result.responseCode = channelChangeInProgress;
    }
    else if (channel != 0 && (channel < MIN_ZIGBEE_CHANNEL || channel > MAX_ZIGBEE_CHANNEL)) // 0 means 'calculate'
    {
        icLogWarn(LOG_TAG, "%s: attempt to change to channel out of range %d", __FUNCTION__, channel);
        result.responseCode = channelChangeInvalidChannel;
        result.channelNumber = channel;
    }
    else
    {
        if (channel == 0)
        {
            icLogDebug(LOG_TAG, "%s: no channel given, so calculate the 'best' one", __FUNCTION__);
            channel = calculateBestChannel();
        }

        result.channelNumber = channel;

        if (channel == 0) // we did not find a good channel
        {
            result.responseCode = channelChangeUnableToCalculate;
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: attempting channel change to %d", __FUNCTION__, channel);

            zhalSystemStatus status;
            memset(&status, 0, sizeof(zhalSystemStatus));
            if (zhalGetSystemStatus(&status) == 0)
            {
                // We are already at that channel, so nothing to do
                if (status.channel == channel)
                {
                    result.responseCode = channelChangeSuccess;
                    icLogDebug(LOG_TAG, "%s: we are already on channel %d", __FUNCTION__, channel);
                }
                else if (dryRun == false)
                {
                    // Record the previous version so we can swap back if needed
                    char buf[4];
                    snprintf(buf, sizeof(buf), "%" PRIu8, status.channel);
                    deviceServiceSetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, buf);
                    zhalNetworkChangeRequest networkChangeRequest;
                    memset(&networkChangeRequest, 0, sizeof(zhalNetworkChangeRequest));
                    networkChangeRequest.channel = channel;

                    if (zhalNetworkChange(&networkChangeRequest) == 0)
                    {
                        icLogDebug(LOG_TAG,
                                   "%s: successfully changed channel, now we wait for devices to move.",
                                   __FUNCTION__);

                        isChannelChangeInProgress = true;

                        result.responseCode = channelChangeSuccess;

                        startChannelChangeDeviceWatchdog(status.channel, channel);
                    }
                }
                else
                {
                    icLogDebug(LOG_TAG, "%s: channel change was a dry run.", __FUNCTION__);
                    result.responseCode = channelChangeSuccess;
                }
            }
        }
    }

    pthread_mutex_unlock(&channelChangeMutex);

    return result;
}

static int32_t findLqiInTable(uint64_t eui64, icLinkedList *lqiTable)
{
    int32_t lqi = -1;
    if (lqiTable != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(lqiTable);
        while (linkedListIteratorHasNext(iter))
        {
            zhalLqiData *item = (zhalLqiData *) linkedListIteratorGetNext(iter);
            if (item->eui64 == eui64)
            {
                lqi = item->lqi;
                break;
            }
        }
        linkedListIteratorDestroy(iter);
    }

    return lqi;
}

static void freeNextCloserHopToLqi(void *key, void *value)
{
    (void) key; // Not owned by map
    icLinkedList *lqiTable = (icLinkedList *) value;
    linkedListDestroy(lqiTable, NULL);
}

/*
 * Populate the zigbee network map
 * @return linked list of ZigbeeSubsystemNetworkMapEntry
 */
icLinkedList *zigbeeSubsystemGetNetworkMap(void)
{
    icLinkedList *addressTable = zhalGetAddressTable();
    if (addressTable == NULL)
    {
        return NULL;
    }

    icLinkedList *networkMap = linkedListCreate();
    icHashMap *nextCloserHopToLqi = hashMapCreate();

    zhalSystemStatus status;
    memset(&status, 0, sizeof(zhalSystemStatus));
    zhalGetSystemStatus(&status);

    uint64_t ourEui64 = status.eui64;
    uint64_t blankEui64 = UINT64_MAX;

    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    icLinkedListIterator *iter = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iter))
    {
        icDevice *item = (icDevice *) linkedListIteratorGetNext(iter);
        uint64_t deviceEui64 = zigbeeSubsystemIdToEui64(item->uuid);
        // Default is the device is child of ours
        uint64_t nextCloserHop = ourEui64;

        if (zhalDeviceIsChild(deviceEui64) == false)
        {
            // Discover the nextCloserHop
            icLinkedList *hops = zhalGetSourceRoute(deviceEui64);
            if (hops == NULL)
            {
                icLogInfo(LOG_TAG, "Device %s is not a child or in the source route table", item->uuid);
                nextCloserHop = blankEui64;
            }
            else if (linkedListCount(hops) > 0)
            {
                uint64_t *hop = (uint64_t *) linkedListGetElementAt(hops, 0);
                nextCloserHop = *hop;
            }
            // Otherwise its our child, which is the default

            // Cleanup
            linkedListDestroy(hops, NULL);
        }

        ZigbeeSubsystemNetworkMapEntry *entry =
            (ZigbeeSubsystemNetworkMapEntry *) calloc(1, sizeof(ZigbeeSubsystemNetworkMapEntry));
        entry->address = deviceEui64;
        entry->nextCloserHop = nextCloserHop;
        // Get the lqiTable
        icLinkedList *lqiTable = hashMapGet(nextCloserHopToLqi, &entry->nextCloserHop, sizeof(uint64_t));
        if (lqiTable == NULL)
        {
            // Fetch it if we haven't gotten it yet
            lqiTable = zhalGetLqiTable(entry->nextCloserHop);
            if (lqiTable != NULL)
            {
                hashMapPut(nextCloserHopToLqi, &entry->nextCloserHop, sizeof(uint64_t), lqiTable);
            }
            else
            {
                icLogWarn(LOG_TAG, "getLqiTable return NULL lqiTable");
            }
        }
        // Populate the lqi from our entry in the table
        entry->lqi = findLqiInTable(entry->address, lqiTable);

        // add short ID
        icLinkedListIterator *addressTableIter = linkedListIteratorCreate(addressTable);
        while (linkedListIteratorHasNext(addressTableIter) == true)
        {
            const zhalAddressTableEntry *tableEntry =
                (const zhalAddressTableEntry *) linkedListIteratorGetNext(addressTableIter);
            if (stringCompare(tableEntry->strEui64, item->uuid, false) == 0)
            {
                entry->nodeId = tableEntry->nodeId;
                break;
            }
        }
        linkedListIteratorDestroy(addressTableIter);

        linkedListAppend(networkMap, entry);
    }
    linkedListIteratorDestroy(iter);

    // Cleanup
    linkedListDestroy(addressTable, (linkedListItemFreeFunc) zhalAddressTableEntryDestroy);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
    hashMapDestroy(nextCloserHopToLqi, freeNextCloserHopToLqi);

    return networkMap;
}

bool zigbeeSubsystemUpgradeDeviceFirmwareLegacy(uint64_t eui64,
                                                uint64_t routerEui64,
                                                const char *appFilename,
                                                const char *bootloaderFilename)
{
    return zhalUpgradeDeviceFirmwareLegacy(eui64, routerEui64, appFilename, bootloaderFilename) == 0;
}

/**
 * This function is called periodically to check on the health of ZigbeeCore with a heartbeat request.  The response
 * includes the current PID of ZigbeeCore and whether or not it has been initialized.  Initialization is required
 * whenever ZigbeeCore restarts or if the radio restarts (in this case the ZigbeeCore PID would remain the same).  The
 * important part here is the boolean indicating if it is initialized.
 *
 * This is the only place ZigbeeCore initialization occurs.  If the radio restarts or other processing requires a
 * re-initialization of ZigbeeCore, this recurring task can be triggered to run immediately.
 *
 * @param arg
 */
static void zigbeeCoreMonitorFunc(void *arg)
{
    pid_t pid = -1;
    bool zigbeeCoreInitialized = false; // indicates if ZigbeeCore thinks initialized

    if (zhalHeartbeat(&pid, &zigbeeCoreInitialized) == 0)
    {
        g_autoptr(ZigbeeWatchdogDelegate) watchdogDelegateRef = zigbeeWatchdogDelegateAcquire(watchdogDelegate);

        if (watchdogDelegateRef)
        {
            watchdogDelegateRef->petZhal();
        }

        mutexLock(&networkInitializedMtx);
        bool ourInitialized = networkInitialized; // indicates if we think the network is initialized
        mutexUnlock(&networkInitializedMtx);

        if (zigbeeCoreInitialized == false || ourInitialized == false)
        {
            // we need to initialize!
            initializeNetwork();
        }
    }
}

/**
 * Wait until the network completes initialization and is ready for use or until that operation times out.  If
 * the network is already initialized it will return immediately.
 *
 * @return true on success or false on fail/timeout
 */
static bool waitForNetworkInitialization(void)
{
    bool result = false;

    LOCK_SCOPE(networkInitializedMtx);

    if (networkInitialized == false && incrementalCondTimedWait(&networkInitializedCond,
                                                                &networkInitializedMtx,
                                                                NETWORK_INITIALIZATION_TIMEOUT_SECS) != ETIMEDOUT)
    {
        result = networkInitialized;
    }

    return result;
}

static void zigbeeCoreMonitorTickle(void)
{
    shortCircuitRepeatingTask(zigbeeCoreMonitorTask);
}

static bool isDeviceAutoApsAcked(const icDevice *device)
{
    // siren repeaters and PIMs.  For the siren/repeater we have to key off of the driver name since other devices
    //  share its device class.  Yuck.
    if (stringCompare(PIM_DC, device->deviceClass, false) == 0
#ifdef LEGACY_SIREN_REPEATER_DRIVER_NAME
        || stringCompare(LEGACY_SIREN_REPEATER_DRIVER_NAME, device->managingDeviceDriver, false) == 0
#endif
    )
    {
        return true;
    }

    return false;
}

static bool isDeviceUsingHashBasedLinkKey(const icDevice *device)
{
    bool result = false;

    const char *item = deviceGetMetadata(device, DEVICE_USES_HASH_BASED_LINK_KEY_METADATA);
    if (item != NULL && strcmp(item, "true") == 0)
    {
        result = true;
    }

    return result;
}

static bool shouldUpgradeToHashBasedLinkKey(const icDevice *device)
{
    bool result = true;

    const char *item = deviceGetMetadata(device, DO_NOT_UPGRADE_TO_HASH_BASED_LINK_KEY_METADATA);
    // we do not want link key for keyfobs which are explicitly out of scope
    if ((stringCompare(device->deviceClass, KEYFOB_DC, false) == 0) || (stringToBool(item) == true))
    {
        result = false;
    }

    return result;
}

static bool setDeviceUsingHashBasedLinkKey(const icDevice *device, bool isUsingHashBasedKey, bool setMetadataOnly)
{
    bool result = true;

    const char *item = deviceGetMetadata(device, DEVICE_USES_HASH_BASED_LINK_KEY_METADATA);
    const char *isUsingHashBasedKeyStr = stringValueOfBool(isUsingHashBasedKey);
    bool shouldSetMetadata = true;
    // only update if it has changed.
    if (item != NULL && stringCompare(item, isUsingHashBasedKeyStr, true) == 0)
    {
        shouldSetMetadata = false;
    }

    if (shouldSetMetadata == true)
    {
        icLogDebug(LOG_TAG,
                   "%s : Setting metadata %s for device %s to %s",
                   __FUNCTION__,
                   DEVICE_USES_HASH_BASED_LINK_KEY_METADATA,
                   device->uuid,
                   isUsingHashBasedKeyStr);
        AUTO_CLEAN(free_generic__auto)
        char *uri = createDeviceMetadataUri(device->uuid, DEVICE_USES_HASH_BASED_LINK_KEY_METADATA);
        result = deviceServiceSetMetadata(uri, isUsingHashBasedKeyStr);

        if (setMetadataOnly == false)
        {
            // update the device flags in ZigbeeCore/xNCP
            zigbeeSubsystemSetAddresses();
        }
    }

    return result;
}

IcDiscoveredDeviceDetails *zigbeeSubsystemDiscoverDeviceDetails(uint64_t eui64)
{
    bool basicDiscoverySucceeded = true;
    IcDiscoveredDeviceDetails *details = NULL;
    uint8_t *endpointIds = NULL;
    uint8_t numEndpointIds;
    if (zhalGetEndpointIds(eui64, &endpointIds, &numEndpointIds) == 0 && numEndpointIds > 0)
    {
        details = createIcDiscoveredDeviceDetails();

        details->eui64 = eui64;

        for (uint8_t i = 0; i < numEndpointIds; i++)
        {
            zhalEndpointInfo endpointInfo;
            if (zhalGetEndpointInfo(eui64, endpointIds[i], &endpointInfo) == 0)
            {
                bool hasOtaCluster = false;

                if (endpointInfo.appProfileId != HA_PROFILE_ID)
                {
                    icLogInfo(LOG_TAG, "%s: ignoring non HA profile endpoint", __func__);
                    continue;
                }

                // allocate room for another endpoint details
                details->numEndpoints++;
                details->endpointDetails =
                    realloc(details->endpointDetails, details->numEndpoints * sizeof(IcDiscoveredEndpointDetails));

                IcDiscoveredEndpointDetails *epDetails = &details->endpointDetails[details->numEndpoints - 1];
                memset(epDetails, 0, sizeof(*epDetails));

                epDetails->endpointId = endpointInfo.endpointId;
                epDetails->appProfileId = endpointInfo.appProfileId;
                epDetails->appDeviceId = endpointInfo.appDeviceId;
                epDetails->appDeviceVersion = endpointInfo.appDeviceVersion;
                epDetails->numServerClusterDetails = endpointInfo.numServerClusterIds;
                epDetails->serverClusterDetails = (IcDiscoveredClusterDetails *) calloc(
                    endpointInfo.numServerClusterIds, sizeof(IcDiscoveredClusterDetails));
                for (uint8_t j = 0; j < endpointInfo.numServerClusterIds; j++)
                {
                    epDetails->serverClusterDetails[j].clusterId = endpointInfo.serverClusterIds[j];
                    epDetails->serverClusterDetails[j].isServer = true;
                }

                epDetails->numClientClusterDetails = endpointInfo.numClientClusterIds;
                epDetails->clientClusterDetails = (IcDiscoveredClusterDetails *) calloc(
                    endpointInfo.numClientClusterIds, sizeof(IcDiscoveredClusterDetails));
                for (uint8_t j = 0; j < endpointInfo.numClientClusterIds; j++)
                {
                    if (endpointInfo.clientClusterIds[j] == OTA_UPGRADE_CLUSTER_ID)
                    {
                        hasOtaCluster = true;
                    }

                    epDetails->clientClusterDetails[j].clusterId = endpointInfo.clientClusterIds[j];
                    epDetails->clientClusterDetails[j].isServer = false;
                }

                if (endpointInfo.appDeviceId != ICONTROL_BOGUS_DEVICE_ID)
                {
                    // we will get the manufacturer and model from the first endpoint.  We currently have never heard of
                    //  a device with different manufacturer and models on different endpoints, and that doesnt really
                    //  make sense anyway.  The complexity to handle that scenario is not worth it at this time.
                    if (details->manufacturer == NULL)
                    {
                        // if it fails we will just try again on the next endpoint
                        // coverity[check_return]
                        zigbeeSubsystemReadString(eui64,
                                                  endpointIds[i],
                                                  BASIC_CLUSTER_ID,
                                                  true,
                                                  BASIC_MANUFACTURER_NAME_ATTRIBUTE_ID,
                                                  &details->manufacturer);
                    }
                    if (details->model == NULL)
                    {
                        // if it fails we will just try again on the next endpoint
                        // coverity[check_return]
                        zigbeeSubsystemReadString(eui64,
                                                  endpointIds[i],
                                                  BASIC_CLUSTER_ID,
                                                  true,
                                                  BASIC_MODEL_IDENTIFIER_ATTRIBUTE_ID,
                                                  &details->model);
                    }
                    if (details->hardwareVersion == 0)
                    {
                        // if it fails we will just try again on the next endpoint
                        // coverity[check_return]
                        zigbeeSubsystemReadNumber(eui64,
                                                  endpointIds[i],
                                                  BASIC_CLUSTER_ID,
                                                  true,
                                                  BASIC_HARDWARE_VERSION_ATTRIBUTE_ID,
                                                  &details->hardwareVersion);
                    }
                    if (details->appVersion == 0)
                    {
                        // if it fails we will just try again on the next endpoint
                        // coverity[check_return]
                        zigbeeSubsystemReadNumber(eui64,
                                                  endpointIds[i],
                                                  BASIC_CLUSTER_ID,
                                                  true,
                                                  BASIC_APPLICATION_VERSION_ATTRIBUTE_ID,
                                                  &details->appVersion);
                    }
                    if (details->firmwareVersion == 0 && hasOtaCluster)
                    {
                        // if it fails we will just try again on the next endpoint
                        // coverity[check_return]
                        zigbeeSubsystemReadNumber(eui64,
                                                  endpointIds[i],
                                                  OTA_UPGRADE_CLUSTER_ID,
                                                  false,
                                                  OTA_CURRENT_FILE_VERSION_ATTRIBUTE_ID,
                                                  &details->firmwareVersion);
                    }
                }

                basicDiscoverySucceeded = true;
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to get endpoint info for %d", __FUNCTION__, i);
                basicDiscoverySucceeded = false;
                break;
            }
        }
    }
    else
    {
        icLogError(LOG_TAG, "failed to get endpoint ids for device");
        basicDiscoverySucceeded = false;
    }

    if (basicDiscoverySucceeded == false)
    {
        if (details != NULL)
        {
            freeIcDiscoveredDeviceDetails(details);
            details = NULL;
        }
    }

    // Cleanup
    free(endpointIds);

    return details;
}

/*
 * Get the zigbee module's firmware version.
 * @return the firmware version, or NULL on failure.  Caller must free.
 */
char *zigbeeSubsystemGetFirmwareVersion(void)
{
    return zhalGetFirmwareVersion();
}

/*
 * Restore config for RMA
 */
static bool zigbeeSubsystemRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    bool result = true;

    // Set property to increment counters on next init
    deviceServiceSetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, "true");

    return result;
}

/*
 * Helper function to determine if a device is monitored at all in LPM.
 *
 * returns - true if we should consider monitoring this device while in LPM
 */
static bool isLPMMonitoredDevice(const char *deviceUUID)
{
    bool result = false;

    // get the LPM policy metadata value
    //
    char *deviceMetadataUri = createDeviceMetadataUri(deviceUUID, LPM_POLICY_METADATA);
    char *deviceMetadataValue = NULL;
    if (deviceServiceGetMetadata(deviceMetadataUri, &deviceMetadataValue) && deviceMetadataValue != NULL)
    {
        result = true;
    }

    // cleanup
    free(deviceMetadataValue);
    free(deviceMetadataUri);

    return result;
}

/*
 * Helper function to determine if a Device's LPM policy and
 * the current state of the system allow for a device to
 * wake the system from LPM.
 *
 * returns - the message handling type for device
 */
static zhalMessageHandlingType determineLPMDeviceMessage(const char *deviceUUID, PanelStatus panelStatus)
{
    zhalMessageHandlingType retVal = MESSAGE_HANDLING_IGNORE_ALL;

    // get the LPM policy metadata value
    //
    char *deviceMetadataUri = createDeviceMetadataUri(deviceUUID, LPM_POLICY_METADATA);
    char *deviceMetadataValue = NULL;
    if (deviceServiceGetMetadata(deviceMetadataUri, &deviceMetadataValue) == true && deviceMetadataValue != NULL)
    {
        // look at the metadata data value and the current state of the system,
        // to determine if device needs to be added
        //
        if (stringCompare(deviceMetadataValue, lpmPolicyPriorityLabels[LPM_POLICY_ALWAYS], false) == 0)
        {
            retVal = MESSAGE_HANDLING_NORMAL;
        }
        else if (stringCompare(deviceMetadataValue, lpmPolicyPriorityLabels[LPM_POLICY_ARMED_NIGHT], false) == 0)
        {
            if (panelStatus == PANEL_STATUS_ARMING_NIGHT || panelStatus == PANEL_STATUS_ARMED_NIGHT ||
                panelStatus == PANEL_STATUS_ARMING_AWAY || panelStatus == PANEL_STATUS_ARMED_AWAY)
            {
                retVal = MESSAGE_HANDLING_NORMAL;
            }
        }
        else if (stringCompare(deviceMetadataValue, lpmPolicyPriorityLabels[LPM_POLICY_ARMED_AWAY], false) == 0)
        {
            if (panelStatus == PANEL_STATUS_ARMING_AWAY || panelStatus == PANEL_STATUS_ARMED_AWAY)
            {
                retVal = MESSAGE_HANDLING_NORMAL;
            }
        }

        // cleanup
        free(deviceMetadataValue);
    }
    else
    {
        icLogWarn(LOG_TAG,
                  "%s: unable to find the metadata value for %s on device %s",
                  __FUNCTION__,
                  LPM_POLICY_METADATA,
                  deviceUUID);
    }

    // cleanup
    free(deviceMetadataUri);

    return retVal;
}

/*
 * Set Zigbee OTA firmware upgrade delay
 */
static void zigbeeSubsystemSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    zhalSetOtaUpgradeDelay(delaySeconds);
}

/*
 * Tell Zigbee Subsystem to enter LPM
 */
static void zigbeeSubsystemEnterLPM(void)
{
    zigbeeHealthCheckStop();

    scoped_icLinkedListGeneric *lpmDevices = linkedListCreate();
#ifdef B_CORE_SUPPORT_ALARMS
    scoped_SecurityState *state = deviceServiceGetSecurityState();
#endif
    uint32_t minCommFailTimeoutSecs = UINT32_MAX;

    // Assure all commfail states are up to date before monitoring anything.
    // Some devices may have recently timed out, and may no longer need to be monitored.
    deviceCommunicationWatchdogCheckDevices();

    scoped_icDeviceList *deviceList = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);

    if (deviceList != NULL)
    {
        // loop and create items for the monitored device info for LPM if we find a device that we care about
        //
        scoped_icLinkedListIterator *listIter = linkedListIteratorCreate(deviceList);

        while (linkedListIteratorHasNext(listIter))
        {
            icDevice *device = linkedListIteratorGetNext(listIter);

            if (device == NULL || device->uuid == NULL)
            {
                continue;
            }

            if (isLPMMonitoredDevice(device->uuid) == true)
            {
#ifdef B_CORE_SUPPORT_ALARMS
                zhalMessageHandlingType messageHandlingType =
                    determineLPMDeviceMessage(device->uuid, state->panelStatus);
#else
                zhalMessageHandlingType messageHandlingType = MESSAGE_HANDLING_NORMAL;
#endif
                DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
                uint32_t deviceCommFailSecs = zigbeeDriverCommonGetDeviceCommFailTimeout(driver);

                if (deviceCommFailSecs != 0)
                {
                    minCommFailTimeoutSecs = MIN(deviceCommFailSecs, minCommFailTimeoutSecs);
                }

                // if the device is in already comm fail then send -1 as secRemaining
                // xNCP will not start timer of the already comm failed devices
                //
                int32_t secsRemaining =
                    deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(device->uuid, deviceCommFailSecs);

                zhalLpmMonitoredDeviceInfo *tmpDeviceInfo = malloc(sizeof(zhalLpmMonitoredDeviceInfo));
                tmpDeviceInfo->eui64 = zigbeeSubsystemIdToEui64(device->uuid);
                tmpDeviceInfo->messageHandling = messageHandlingType;
                tmpDeviceInfo->timeoutSeconds = secsRemaining;

                linkedListAppend(lpmDevices, tmpDeviceInfo);
            }
            else
            {
                icLogDebug(LOG_TAG,
                           "%s: not monitoring device %s since it's not an LPM monitored device",
                           __func__,
                           device->uuid);
            }
        }
    }

#ifdef CONFIG_CAP_ZIGBEE_TELEMETRY
    zigbeeTelemetryShutdown();
#endif

    /*
     * Beware: The xNCP treats comm-fail timeouts as a global state.
     * In practice, all LPM monitored drivers want the same commfail timeout.
     * If a DDL change comes along that appears to cause unexpected commfails,
     * this is why.
     */
    zhalSetCommunicationFailTimeout(minCommFailTimeoutSecs);
    zhalEnterLowPowerMode(lpmDevices);
}

/*
 * Tell Zigbee Subsystem to exit LPM
 */
static void zigbeeSubsystemExitLPM(void)
{
    // tell zhal to exit LPM first
    //
    zhalExitLowPowerMode();

    // get the monitored devices info
    // we will be using timeout seconds sent by xNCP as the communication
    // failure time out value for the devices.
    //
    icLinkedList *monitoredDevicesInfoList = zhalGetMonitoredDevicesInfo();
    if (monitoredDevicesInfoList != NULL)
    {
        icLinkedListIterator *listIter = linkedListIteratorCreate(monitoredDevicesInfoList);
        while (linkedListIteratorHasNext(listIter) == true)
        {
            zhalLpmMonitoredDeviceInfo *monitoredDeviceInfo =
                (zhalLpmMonitoredDeviceInfo *) linkedListIteratorGetNext(listIter);

            AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(monitoredDeviceInfo->eui64);

            // update the commFail time remaining for the device
            //
            deviceCommunicationWatchdogSetTimeRemainingForDeviceFromLPM(uuid, monitoredDeviceInfo->timeoutSeconds);
        }

        // cleanup
        //
        linkedListIteratorDestroy(listIter);
        linkedListDestroy(monitoredDevicesInfoList, NULL);
    }

    // Resume zigbee network health monitoring
    //
    zigbeeHealthCheckStart();

    // Resume the telemetry system
    //
#ifdef CONFIG_CAP_ZIGBEE_TELEMETRY
    zigbeeTelemetryInitialize();
#endif
}

void zigbeeSubsystemHandlePropertyChange(const char *prop, const char *value)
{
    icLogDebug(LOG_TAG, "%s: prop=%s, value=%s", __FUNCTION__, prop, value);
    if (prop == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid args", __FUNCTION__);
        return;
    }

    if (strncmp(prop, ZIGBEE_HEALTH_CHECK_PROPS_PREFIX, strlen(ZIGBEE_HEALTH_CHECK_PROPS_PREFIX)) == 0)
    {
        // some property related to zigbee network health check changed, let that code determine what to do about it
        zigbeeHealthCheckStart();
    }
    else if (strncmp(prop, ZIGBEE_DEFENDER_PROPS_PREFIX, strlen(ZIGBEE_DEFENDER_PROPS_PREFIX)) == 0)
    {
        // some property related to zigbee defender changed, let that code determine what to do about it
        zigbeeDefenderConfigure();
    }
    else if (strncmp(prop, TELEMETRY_PROPS_PREFIX, strlen(TELEMETRY_PROPS_PREFIX)) == 0)
    {
#ifdef CONFIG_CAP_ZIGBEE_TELEMETRY
        zigbeeTelemetrySetProperty(prop, value);
#endif
    }
    else if (strncmp(prop, ZIGBEE_LINK_QUALITY_PROPS_PREFIX, strlen(ZIGBEE_LINK_QUALITY_PROPS_PREFIX)) == 0)
    {
        zigbeeLinkQualityConfigure();
    }
    else if (stringStartsWith(prop, ZIGBEE_PROPS_PREFIX, false) == true)
    {
        // pass all other properties down to the stack, chopping the prefix off.
        const size_t prefixLen = strlen(ZIGBEE_PROPS_PREFIX);
        if (strlen(prop) > prefixLen)
        {
            zhalSetProperty(prop + prefixLen, value);
        }
        else
        {
            icLogWarn(LOG_TAG, "Property has no name, ignoring!");
        }
    }
    else if (stringCompare(prop, FAST_COMM_FAIL_PROP, false) == 0)
    {
        mutexLock(&commFailControlMutex);

        fastCommFailTimer = stringToBool(value);

        // FOR COMM FAIL TEST ONLY: If event is received and fastCommFailTimer value is false, it means test is in
        // progress and server has received the software trouble, which was required. There is no need to send signal
        // now as that would lead to conditional wait function to return and next action of "restart all services" would
        // be triggered, which will lead to failure of the test. Check comm fail monitoring task for more information.
        if (fastCommFailTimer)
        {
            pthread_cond_broadcast(&commFailControlCond);
        }
        mutexUnlock(&commFailControlMutex);
    }
}

icLinkedList *zigbeeSubsystemPerformEnergyScan(const uint8_t *channelsToScan,
                                               uint8_t numChannelsToScan,
                                               uint32_t scanDurationMillis,
                                               uint32_t numScans)
{
    return zhalPerformEnergyScan(channelsToScan, numChannelsToScan, scanDurationMillis, numScans);
}

void zigbeeSubsystemNotifyDeviceCommRestored(icDevice *device)
{
    g_autoptr(ZigbeeWatchdogDelegate) watchdogDelegateRef = zigbeeWatchdogDelegateAcquire(watchdogDelegate);

    if (watchdogDelegateRef)
    {
        watchdogDelegateRef->setAllDevicesInCommFail(false);
    }
}

void zigbeeSubsystemNotifyDeviceCommFail(icDevice *device)
{
    // Currently unused
    (void) device;

    // signal comm fail checking thread to check if all devices are in comm fail
    //
    mutexLock(&commFailControlMutex);

    pthread_cond_broadcast(&commFailControlCond);

    mutexUnlock(&commFailControlMutex);
}

static void *checkAllDevicesInCommThreadProc(void *arg)
{
    while (true)
    {
        bool devicesInCommFail = false;

        {
            LOCK_SCOPE(commFailControlMutex);
            if (commfailMonitorThreadRunning == false)
            {
                icLogInfo(LOG_TAG, "%s exiting", __FUNCTION__);
                break;
            }

            if (fastCommFailTimer)
            {
                // Each "all devices are in comm fail" status would trigger recovery. So, we need to provide
                // ample time for trouble to be sent to server, before reboot recovery is started.
                //
                incrementalCondTimedWaitMillis(
                    &commFailControlCond, &commFailControlMutex, COMM_FAIL_POLL_THREAD_SLEEP_TIME_SECONDS);
            }
            else
            {
                incrementalCondTimedWait(
                    &commFailControlCond, &commFailControlMutex, COMM_FAIL_POLL_THREAD_SLEEP_TIME_SECONDS);
            }

            // We may get signalled from shutdown function, so check again if we can proceed
            //
            if (commfailMonitorThreadRunning == false)
            {
                icLogInfo(LOG_TAG, "%s stop checking devices for commFail and exit", __FUNCTION__);
                break;
            }
        }

        // We can be in a state where ZigbeeCore is responding to heartbeats, but for whatever reason all of
        // our devices are in comm fail. When this happens, we need to bounce ZigbeeCore.

        icLinkedList *allDevices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
        sbIcLinkedListIterator *it = linkedListIteratorCreate(allDevices);

        // Iterate over devices until we find one not in comm fail.
        while (linkedListIteratorHasNext(it))
        {
            icDevice *device = linkedListIteratorGetNext(it);
            if (deviceCommunicationWatchdogIsDeviceMonitored(device->uuid))
            {
                devicesInCommFail = deviceServiceIsDeviceInCommFail(device->uuid);
                if (devicesInCommFail == false)
                {
                    // If a single monitored device is not in comm fail, bail out
                    break;
                }
            }
        }

        linkedListDestroy(allDevices, (linkedListItemFreeFunc) deviceDestroy);

        if (devicesInCommFail)
        {
            bool recoveryInProgress = false;

            g_autoptr(ZigbeeWatchdogDelegate) watchdogDelegateRef = zigbeeWatchdogDelegateAcquire(watchdogDelegate);

            if (watchdogDelegateRef)
            {
                // The order of the operations here is important. We must first check if a recovery is in progress
                // before notifying the delegate about comm fail status since setAllDevicesInCommFail(true) can
                // trigger recovery actions.
                recoveryInProgress = watchdogDelegateRef->getActionInProgress();
                watchdogDelegateRef->setAllDevicesInCommFail(true);
            }

            if (!recoveryInProgress)
            {
                // FOR COMM FAIL TEST ONLY: Set fast comm fail to false to resume normal comm failure timeout to
                // avoid fast "restart all services" recovery as next recovery step for our test due to fast comm
                // fail timeout, as that would be invalid case for the test.
                //
                mutexLock(&commFailControlMutex);
                if (fastCommFailTimer)
                {
                    fastCommFailTimer = false;
                    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
                    b_core_property_provider_set_property_bool(
                        propertyProvider, FAST_COMM_FAIL_PROP, fastCommFailTimer);
                }
                mutexUnlock(&commFailControlMutex);
            }
        }
    }

    return NULL;
}

static void *createDeviceCallbacks(void)
{
    return hashMapCreate();
}

static bool releaseIfDeviceCallbacksEmpty(void *item)
{
    icHashMap *map = (icHashMap *) item;
    return hashMapCount(map) == 0;
}

static void deviceCallbackDestroy(void *item)
{
    icHashMap *map = (icHashMap *) item;
    hashMapDestroy(map, freeDeviceCallbackEntry);
}

static void deviceCallbacksRegister(void *item, void *context)
{
    icHashMap *map = (icHashMap *) item;
    DeviceCallbacksRegisterContext *ctx = (DeviceCallbacksRegisterContext *) context;

    if (hashMapGet(map, &ctx->eui64, sizeof(uint64_t)) != NULL)
    {
        icLogError(LOG_TAG,
                   "zigbeeSubsystemRegisterDeviceListener: listener already registered for %016" PRIx64 "!",
                   ctx->eui64);
        ctx->registered = false;
    }
    else
    {
        uint64_t *key = (uint64_t *) malloc(sizeof(uint64_t));
        *key = ctx->eui64;
        hashMapPut(map, key, sizeof(uint64_t), ctx->callbacks);
        ctx->registered = true;
    }
}

static void deviceCallbacksUnregister(void *item, void *context)
{
    icHashMap *map = (icHashMap *) item;
    DeviceCallbacksUnregisterContext *ctx = (DeviceCallbacksUnregisterContext *) context;

    if (hashMapDelete(map, &ctx->eui64, sizeof(uint64_t), freeDeviceCallbackEntry) == false)
    {
        icLogError(
            LOG_TAG, "zigbeeSubsystemUnregisterDeviceListener: no listener registered for %016" PRIx64 "!", ctx->eui64);
        ctx->unregistered = false;
    }
    else
    {
        ctx->unregistered = true;
    }
}

static void deviceCallbacksAttributeReport(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *) item;
    ReceivedAttributeReport *report = (ReceivedAttributeReport *) context;

    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &report->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->attributeReportReceived != NULL)
        {
            cbs->attributeReportReceived(cbs->callbackContext, report);
        }
    }
}

static void deviceCallbacksRejoined(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *) item;
    DeviceCallbacksRejoinContext *rejoinContext = (DeviceCallbacksRejoinContext *) context;

    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &rejoinContext->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->deviceRejoined != NULL)
        {
            cbs->deviceRejoined(cbs->callbackContext, rejoinContext->eui64, rejoinContext->isSecure);
        }
    }
}

static void deviceCallbacksLeft(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *) item;
    uint64_t eui64 = *(uint64_t *) context;

    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->deviceLeft != NULL)
        {
            cbs->deviceLeft(cbs->callbackContext, eui64);
        }
    }
}

static void deviceCallbacksOtaUpgradeMessageSent(const void *item, const void *context)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    icHashMap *map = (icHashMap *) item;
    DeviceCallbacksOtaUpgradeEventContext *otaEventContext = (DeviceCallbacksOtaUpgradeEventContext *) context;
    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;

    if ((cbs = hashMapGet(map, &otaEventContext->otaEvent->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->otaUpgradeMessageSent != NULL)
        {
            cbs->otaUpgradeMessageSent(cbs->callbackContext, otaEventContext->otaEvent);
        }
    }
}

static void deviceCallbacksOtaUpgradeMessageReceived(const void *item, const void *context)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    icHashMap *map = (icHashMap *) item;
    DeviceCallbacksOtaUpgradeEventContext *otaEventContext = (DeviceCallbacksOtaUpgradeEventContext *) context;
    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;

    if ((cbs = hashMapGet(map, &otaEventContext->otaEvent->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->otaUpgradeMessageReceived != NULL)
        {
            cbs->otaUpgradeMessageReceived(cbs->callbackContext, otaEventContext->otaEvent);
        }
    }
}

static void deviceCallbacksClusterCommandReceived(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *) item;
    DeviceCallbacksClusterCommandReceivedContext *ctx = (DeviceCallbacksClusterCommandReceivedContext *) context;
    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &ctx->command->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->clusterCommandReceived != NULL)
        {
            cbs->clusterCommandReceived(cbs->callbackContext, ctx->command);
        }
        ctx->deviceFound = true;
    }
    else
    {
        ctx->deviceFound = false;
    }
}

static void deviceCallbacksDeviceAnnounced(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *) item;
    DeviceAnnouncedContext *ctx = (DeviceAnnouncedContext *) context;
    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &ctx->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->deviceAnnounced != NULL)
        {
            cbs->deviceAnnounced(cbs->callbackContext, ctx->eui64, ctx->deviceType, ctx->powerSource);
        }
    }
}

static void zigbeeSubsystemPostRestoreConfig(void)
{
    // mark the network uninitialized so it will re-init
    zigbeeSubsystemSetUnready();

    // make the monitor run now so the initialization will occur
    zigbeeCoreMonitorTickle();

    // wait here for the network to initialize completely (or time out)
    waitForNetworkInitialization();
}

static void zigbeeLinkQualityConfigure(void)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    pthread_mutex_lock(&configMtx);
    // determine link quality value based on given rssi and lqi, once this property is set to true
    useLqiForLinkQuality = b_core_property_provider_get_property_as_bool(
        propertyProvider, ZIGBEE_LINK_QUALITY_LQI_ENABLED_PROP, false);
    useRssiForLinkQuality = b_core_property_provider_get_property_as_bool(
        propertyProvider, ZIGBEE_LINK_QUALITY_RSSI_ENABLED_PROP, true);
    crossAboveDb = b_core_property_provider_get_property_as_uint8(
        propertyProvider, ZIGBEE_LINK_QUALITY_RSSI_CROSS_ABOVE_DB_PROP, DEFAULT_RSSI_QUALITY_CROSS_ABOVE_DB);
    crossBelowDb = b_core_property_provider_get_property_as_uint8(
        propertyProvider, ZIGBEE_LINK_QUALITY_RSSI_CROSS_BELOW_DB_PROP, DEFAULT_RSSI_QUALITY_CROSS_BELOW_DB);
    lqiThresholds[WARN_THRESHOLD_INDEX] = b_core_property_provider_get_property_as_uint8(
        propertyProvider, ZIGBEE_LINK_QUALITY_LQI_WARN_THRESHOLD_PROP, WARN_LQI_LIMIT_DEFAULT);
    lqiThresholds[BAD_THRESHOLD_INDEX] = b_core_property_provider_get_property_as_uint8(
        propertyProvider, ZIGBEE_LINK_QUALITY_LQI_BAD_THRESHOLD_PROP, BAD_LQI_LIMIT_DEFAULT);
    rssiThresholds[WARN_THRESHOLD_INDEX] = b_core_property_provider_get_property_as_int8(
        propertyProvider, ZIGBEE_LINK_QUALITY_RSSI_WARN_THRESHOLD_PROP, WARN_RSSI_LIMIT_DEFAULT);
    rssiThresholds[BAD_THRESHOLD_INDEX] = b_core_property_provider_get_property_as_int8(
        propertyProvider, ZIGBEE_LINK_QUALITY_RSSI_BAD_THRESHOLD_PROP, BAD_RSSI_LIMIT_DEFAULT);
    icLogDebug(LOG_TAG,
               "linkQuality configuration: lqiEnabled=%s, lqiWarn=%" PRIu8 ", lqiBad=%" PRIu8
               ", rssiEnabled=%s, rssiWarn=%" PRId8 ", rssiBad=%" PRId8 ",crossAboveDb=%" PRIu8
               ", crossBelowDb=%" PRIu8,
               stringValueOfBool(useLqiForLinkQuality),
               lqiThresholds[WARN_THRESHOLD_INDEX],
               lqiThresholds[BAD_THRESHOLD_INDEX],
               stringValueOfBool(useRssiForLinkQuality),
               rssiThresholds[WARN_THRESHOLD_INDEX],
               rssiThresholds[BAD_THRESHOLD_INDEX],
               crossAboveDb,
               crossBelowDb);
    pthread_mutex_unlock(&configMtx);
}

/*
 * determine the link state based on given rssiSample
 */
static ZigbeeSubsystemLinkQualityLevel calculateLinkQuality(int8_t rssiSample)
{
    ZigbeeSubsystemLinkQualityLevel rssiHealth = LINK_QUALITY_LEVEL_UNKNOWN;

    if (rssiSample < rssiThresholds[BAD_THRESHOLD_INDEX])
    {
        rssiHealth = LINK_QUALITY_LEVEL_POOR;
    }
    else if (rssiSample < rssiThresholds[WARN_THRESHOLD_INDEX])
    {
        rssiHealth = LINK_QUALITY_LEVEL_FAIR;
    }
    else
    {
        rssiHealth = LINK_QUALITY_LEVEL_GOOD;
    }

    return rssiHealth;
}

/*
 * convert string formatted linkQuality to enum
 */
ZigbeeSubsystemLinkQualityLevel zigbeeSubsystemLinkQualityStringToEnum(const char *linkQuality)
{
    ZigbeeSubsystemLinkQualityLevel result = LINK_QUALITY_LEVEL_UNKNOWN;
    if (linkQuality != NULL)
    {
        int i;
        for (i = 0; i < ARRAY_LENGTH(linkQualityLevelLabels); i++)
        {
            if (stringCompare(linkQualityLevelLabels[i], linkQuality, false) == 0)
            {
                break;
            }
        }

        // if we didnt exceed the label list, that means we found a match.  Otherwise the input was bogus
        if (i < ARRAY_LENGTH(linkQualityLevelLabels))
        {
            result = i;
        }
    }

    return result;
}

/*
 * Determine a link quality value given rssi and lqi values.
 * prevLinkQuality is a hint to the algorithm for what we had before this latest check or LINK_QUALITY_LEVEL_UNKNOWN
 * if we dont have a previous value.
 */
const char *zigbeeSubsystemDetermineLinkQuality(int8_t neRssi,
                                                int8_t feRssi,
                                                uint8_t neLqi,
                                                uint8_t feLqi,
                                                ZigbeeSubsystemLinkQualityLevel qualityHint)
{
    pthread_mutex_lock(&configMtx);
    bool useRssi = useRssiForLinkQuality;
    bool useLqi = useLqiForLinkQuality;
    uint8_t aboveLimit = crossAboveDb;
    uint8_t belowLimit = crossBelowDb;
    pthread_mutex_unlock(&configMtx);

    int8_t worstRssi = INT8_MIN;
    if (neRssi != INT8_MIN && feRssi != INT8_MIN)
    {
        worstRssi = neRssi < feRssi ? neRssi : feRssi;
    }
    else if (neRssi != INT8_MIN)
    {
        worstRssi = neRssi;
    }
    else if (feRssi != INT8_MIN)
    {
        worstRssi = feRssi;
    }
    else
    {
        useRssi = false;
    }

    uint8_t worstLqi = 0;
    if (neLqi != 0 && feLqi != 0)
    {
        worstLqi = neLqi < feLqi ? neLqi : feLqi;
    }
    else if (neLqi != 0)
    {
        worstLqi = neLqi;
    }
    else if (feLqi != 0)
    {
        worstLqi = feLqi;
    }
    else
    {
        useLqi = false;
    }

    ZigbeeSubsystemLinkQualityLevel rssiHealth = LINK_QUALITY_LEVEL_UNKNOWN;
    if (useRssi == true)
    {
        ZigbeeSubsystemLinkQualityLevel linkQualNow = calculateLinkQuality(worstRssi);
        if (linkQualNow < qualityHint)
        {
            // Negate below limit since we are going towards poorer (more negative) rssi
            rssiHealth = calculateLinkQuality(worstRssi + belowLimit);
        }
        else if (linkQualNow > qualityHint)
        {
            rssiHealth = calculateLinkQuality(worstRssi - aboveLimit);
        }
        else
        {
            rssiHealth = linkQualNow;
        }
    }

    ZigbeeSubsystemLinkQualityLevel lqiHealth = LINK_QUALITY_LEVEL_UNKNOWN;
    if (useLqi == true)
    {
        if (worstLqi < lqiThresholds[BAD_THRESHOLD_INDEX])
        {
            lqiHealth = LINK_QUALITY_LEVEL_POOR;
        }
        else if (worstLqi < lqiThresholds[WARN_THRESHOLD_INDEX])
        {
            lqiHealth = LINK_QUALITY_LEVEL_FAIR;
        }
        else
        {
            lqiHealth = LINK_QUALITY_LEVEL_GOOD;
        }
    }

    const char *linkQuality = LINK_QUALITY_UNKNOWN;
    if (lqiHealth != LINK_QUALITY_LEVEL_UNKNOWN && rssiHealth != LINK_QUALITY_LEVEL_UNKNOWN)
    {
        // Return whichever is worse
        linkQuality = rssiHealth < lqiHealth ? linkQualityLevelLabels[rssiHealth] : linkQualityLevelLabels[lqiHealth];
    }
    else if (lqiHealth != LINK_QUALITY_LEVEL_UNKNOWN)
    {
        linkQuality = linkQualityLevelLabels[lqiHealth];
    }
    else if (rssiHealth != LINK_QUALITY_LEVEL_UNKNOWN)
    {
        linkQuality = linkQualityLevelLabels[rssiHealth];
    }

    if (isIcLogPriorityTrace() == true)
    {
        icLogTrace(LOG_TAG,
                   "neRssi = %" PRId8 ", feRssi= %" PRId8 ", neLqi = %" PRIu8 ", feLqi = %" PRIu8 ", linkQuality=%s",
                   neRssi,
                   feRssi,
                   neLqi,
                   feLqi,
                   linkQuality);
    }

    return linkQuality;
}

bool zigbeeSubsystemRequestDeviceLeave(uint64_t eui64, bool withRejoin, bool isEndDevice)
{
    return zhalRequestLeave(eui64, withRejoin, isEndDevice) == ZHAL_STATUS_OK;
}

/*
 * Notify zigbeeSubsystem that a device announced
 * @param eui64
 * @param deviceType
 * @param powerSource
 */
void zigbeeSubsystemDeviceAnnounced(uint64_t eui64, zhalDeviceType deviceType, zhalPowerSource powerSource)
{
    DeviceAnnouncedContext ctx = {.eui64 = eui64, .deviceType = deviceType, .powerSource = powerSource};
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksDeviceAnnounced, &ctx);
}

static void responseHandler(const char *responseType, ZHAL_STATUS resultCode)
{
    if (responseType == NULL)
    {
        return;
    }

    if (stringCompare(responseType, ZHAL_RESPONSE_TYPE_ATTRIBUTES_READ, false) == 0 ||
        stringCompare(responseType, ZHAL_RESPONSE_TYPE_SEND_COMMAND, false) == 0)
    {
        bool operationRejected = (resultCode == ZHAL_STATUS_NETWORK_BUSY);

        g_autoptr(ZigbeeWatchdogDelegate) watchdogDelegateRef = zigbeeWatchdogDelegateAcquire(watchdogDelegate);

        if (watchdogDelegateRef)
        {
            watchdogDelegateRef->zhalResponseHandler(operationRejected);
        }
    }
}

/**
 * configure monitors for zigbee system health
 */
static void configureMonitors(void)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    if (b_core_property_provider_get_property_as_bool(propertyProvider, ZIGBEE_WATCHDOG_ENABLED_PROP, true))
    {
        // Start our repeating task which, when successful, monitors ZigbeeCore and ensures the
        // network is initialized.
        zigbeeCoreMonitorTask =
            createRepeatingTask(zigbeeCoreHeartbeatPetFrequencySecs, DELAY_SECS, zigbeeCoreMonitorFunc, NULL);


        // Start thread to check if all devices are in comm fail. We are not checking it only when
        // deviceCommunicationWatchdog reports a device is in comm fail, because deviceCommunicationWatchdog
        // does not report comm failure again for devices if they are already in comm fail. Zigbee self healing requires
        // at-least comm fail to be reported two times for all devices, once to trigger restart and then to reboot if
        // issue persists. This task also takes care of the case when zigbee recovery is already in progress for any
        // other reason and comm fail also occurs meanwhile, but we do not the update the status due to recovery in
        // progress. It shall check again after its repeating interval and detect if devices are still in comm failure
        // and trigger recovery. It was not possible using deviceCommunicationWatchdog reports only.
        //
        mutexLock(&commFailControlMutex);
        fastCommFailTimer =
            b_core_property_provider_get_property_as_bool(propertyProvider, FAST_COMM_FAIL_PROP, false);
        initTimedWaitCond(&commFailControlCond);
        commfailMonitorThreadRunning =
            createThread(&commFailMonitorThreadId, checkAllDevicesInCommThreadProc, NULL, "commFailPoll");
        mutexUnlock(&commFailControlMutex);
    }
    else
    {
        // we still need to perform a valid startup for this case, which is only used for buildtime tests where
        // there is no ZigbeeCore
        icLogWarn(
            LOG_TAG,
            "%s: watchdog is DISABLED which prevents normal operation.  Proceeding with faked startup for test mode",
            __func__);

        zigbeeSubsystemSetReady();
    }
}
