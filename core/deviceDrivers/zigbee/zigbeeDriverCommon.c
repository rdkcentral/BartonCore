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
// Created by tlea on 2/13/19.
//
#include "deviceDrivers/zigbeeDriverCommon.h"
#include "device/icDevice.h"
#include "devicePrivateProperties.h"
#include "deviceServiceCommFail.h"
#include "deviceServiceConfiguration.h"
#include "icTypes/icLinkedList.h"
#include "provider/barton-core-property-provider.h"
#include "zigbeeClusters/alarmsCluster.h"
#include "zigbeeClusters/pollControlCluster.h"
#include <commonDeviceDefs.h>
#include <curl/curl.h>
#include <device/deviceModelHelper.h>
#include <deviceCommunicationWatchdog.h>
#include <deviceDescriptors.h>
#include <deviceDriverManager.h>
#include <deviceHelper.h>
#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <errno.h>
#include <event/deviceEventProducer.h>
#include <glib.h>
#include <icConcurrent/delayedTask.h>
#include <icConcurrent/repeatingTask.h>
#include <icConcurrent/threadPool.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icCrypto/digest.h>
#include <icLog/telemetryMarkers.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <memory.h>
#include <pthread.h>
#include <resourceTypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <sys/stat.h>
#include <unistd.h>
#include <urlHelper/urlHelper.h>
#include <versionUtils.h>
#include <zigbeeClusters/diagnosticsCluster.h>
#include <zigbeeClusters/helpers/comcastBatterySavingHelper.h>
#include <zigbeeClusters/otaUpgradeCluster.h>
#include <zigbeeClusters/powerConfigurationCluster.h>
#include <zigbeeClusters/remoteCliCluster.h>
#include <zigbeeClusters/temperatureMeasurementCluster.h>

#define LOG_TAG     "zigbeeDriverCommon"
#define logFmt(fmt) "%s: " fmt, __func__
#include <icLog/logging.h>

#ifdef BARTON_CONFIG_ZIGBEE

#define ZIGBEE_COMMON_VERSION_METADATA              "zigbeeCommonVersion"
#define ZIGBEE_COMMON_VERSION                       2
#define FIRMWARE_FORMAT_STRING                      "0x%08" PRIx32
#define DISCOVERED_DEVICE_DETAILS                   "discoveredDetails"
#define ZIGBEE_ENDPOINT_ID_METADATA_NAME            "zigbee_epid"
#define FIRMWARE_UPGRADE_RETRYDELAYSECS             "firmware.upgrade.retryDelaySecs"
#define FIRMWARE_UPGRADE_RETRYDELAYSECS_DEFAULT     (60 * 60)
#define FIRMWARE_UPGRADE_DELAYSECS                  "firmware.upgrade.delaySecs"
#define FIRMWARE_UPGRADE_DELAYSECS_DEFAULT          (2 * 60 * 60)
#define OTA_UPGRADE_INFO_METADATA_NAME              "otaUpgradeInfo"
#define DIAGNOSTICS_COLLECTION_INTERVAL_MINS        30
#define DIAGNOSTICS_COLLECTION_DELAY_SECS           5
#define RECONFIGURATION_DELAY_SECS                  60 // Time a Zigbee device would take to reboot after a firmware upgrade

// These properties and defaults are used for battery savings during poll control checkin processing
// to determine if/when we should go out to the device to read them.
#define DEFAULT_BATTERY_VOLTAGE_REFRESH_MIN_SECONDS (24 * 60 * 60) // 1 day
#define BATTERY_VOLTAGE_REFRESH_MIN_SECS_PROP       "BatteryVoltageRefreshMinSecs"
#define DEFAULT_RSSI_REFRESH_MIN_SECONDS            (25 * 60) // 25 minutes
#define RSSI_REFRESH_MIN_SECS_PROP                  "FeRssiRefreshMinSecs"
#define DEFAULT_LQI_REFRESH_MIN_SECONDS             (25 * 60) // 25 minutes
#define LQI_REFRESH_MIN_SECS_PROP                   "FeLqiRefreshMinSecs"
#define DEFAULT_TEMP_REFRESH_MIN_SECONDS            (50 * 60) // 50 minutes
#define TEMP_REFRESH_MIN_SECS_PROP                  "TempRefreshMinSecs"

#define ZIGBEE_DEVICE_MODEL_VERSION                 1

// This is stored within the DeviceDriver's callbackContext
struct ZigbeeDriverCommon
{
    // the main driver structure at the top
    DeviceDriver baseDriver;

    char *deviceClass;
    uint8_t deviceClassVersion;
    uint16_t *deviceIds;
    uint8_t numDeviceIds;
    zigbeeReceiverMode rxMode;
    ZigbeeSubsystemDeviceDiscoveredHandler discoveredHandler;
    ZigbeeSubsystemDeviceCallbacks *deviceCallbacks;
    icHashMap *clusters; // clusterId to ZigbeeCluster
    bool discoveryActive;
    icHashMap *discoveredDeviceDetails;         // maps eui64 to IcDiscoveredDeviceDetails
    pthread_mutex_t discoveredDeviceDetailsMtx; // lock for discoveredDeviceDetails
    const ZigbeeDriverCommonCallbacks *commonCallbacks;
    uint32_t commFailTimeoutSeconds; // number of seconds of no communication with a device before it comes in commfail
    bool skipConfiguration; // if true, the common driver will not perform any discovery/configuration during pairing
    bool batteryBackedUp;   // if true, configure battery related resources
    bool readInitialBatteryThresholds;  // if true, read all battery alarm thresholds
    icHashMap *pendingFirmwareUpgrades; // delayed task handle to FirmwareUpgradeContext
    pthread_mutex_t pendingFirmwareUpgradesMtx;
    bool diganosticsCollectionEnabled; // if true, periodic collection of diagnostics data will be enabled
    void *driverPrivate;               // Private data for the higher level device driver
};

static void startup(void *ctx);

static void driverShutdown(void *ctx);

static void driverDestroy(void *ctx);

static void deviceRemoved(void *ctx, icDevice *device);

static void communicationFailed(void *ctx, icDevice *device);

static void communicationRestored(void *ctx, icDevice *device);

static bool processDeviceDescriptor(void *ctx, icDevice *device, DeviceDescriptor *dd);

static bool discoverStart(void *ctx, const char *deviceClass);

static void discoverStop(void *ctx, const char *deviceClass);

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);

static bool fetchInitialResourceValues(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);

static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);

static bool devicePersisted(void *ctx, icDevice *device);

static bool readResource(void *ctx, icDeviceResource *resource, char **value);

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);

static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response);

static void attributeReportReceived(void *ctx, ReceivedAttributeReport *report);

static void clusterCommandReceived(void *ctx, ReceivedClusterCommand *command);

static void firmwareUpdateStarted(void *ctx, uint64_t eui64);

static void firmwareUpdateCompleted(void *ctx, uint64_t eui64);

static void firmwareUpdateFailed(void *ctx, uint64_t eui64);

static bool validateOtaLegacyBootloadUpgradeStartedMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaLegacyBootloadUpgradeFailedMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaLegacyBootloadUpgradeCompletedMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaImageNotifyMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaQueryNextImageRequestMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaQueryNextImageResponseMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaUpgradeStartedMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaUpgradeEndRequestMessage(uint8_t *buffer, uint16_t bufferLen);

static bool validateOtaUpgradeEndResponseMessage(uint8_t *buffer, uint16_t bufferLen);

static void otaUpgradeMessageSent(void *ctx, OtaUpgradeEvent *otaEvent);

static void otaUpgradeMessageReceived(void *ctx, OtaUpgradeEvent *otaEvent);

static void otaLegacyBootloadUpgradeStarted(void *ctx, uint64_t eui64, uint64_t timestamp);

static void otaLegacyBootloadUpgradeFailed(void *ctx, uint64_t eui64, uint64_t timestamp);

static void otaLegacyBootloadUpgradeCompleted(void *ctx, uint64_t eui64, uint64_t timestamp);

static void otaImageNotifySent(void *ctx, uint64_t eui64, uint64_t timestamp, ZHAL_STATUS sentStatus);

static void otaQueryNextImageRequest(void *ctx,
                                     uint64_t eui64,
                                     uint64_t timestamp,
                                     uint16_t mfgCode,
                                     uint16_t imageType,
                                     uint32_t currentVersion,
                                     uint16_t hardwareVersion);

static void otaQueryNextImageResponseSent(void *ctx,
                                          uint64_t eui64,
                                          uint64_t timestamp,
                                          uint8_t statusCode,
                                          ZHAL_STATUS sentStatus);

static void otaUpgradeStarted(void *ctx, uint64_t eui64, uint64_t timestamp);

static void otaUpgradeEndRequest(void *ctx,
                                 uint64_t eui64,
                                 uint64_t timestamp,
                                 uint8_t upgradeStatus,
                                 uint16_t mfgCode,
                                 uint16_t imageType,
                                 uint32_t nextVersion);

static void otaUpgradeEndResponseSent(void *ctx,
                                      uint64_t eui64,
                                      uint64_t timestamp,
                                      uint32_t mfgCode,
                                      uint32_t newVersion,
                                      uint32_t currentTime,
                                      uint32_t upgradeTime,
                                      ZHAL_STATUS sentStatus);

static void deviceRejoined(void *ctx, uint64_t eui64, bool isSecure);

static void deviceLeft(void *ctx, uint64_t eui64);

static void deviceAnnounced(void *ctx, uint64_t eui64, zhalDeviceType deviceType, zhalPowerSource powerSource);

static void synchronizeDevice(void *ctx, icDevice *device);

static void endpointDisabled(void *ctx, icDeviceEndpoint *endpoint);

static bool deviceDiscoveredCallback(void *ctx, IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator);

static void discoveredDeviceDetailsFreeFunc(void *key, void *value);

static bool findDeviceResource(void *searchVal, void *item);

static void updateNeRssiAndLqi(void *ctx, uint64_t eui64, int8_t rssi, uint8_t lqi);

static void handleAlarmCommand(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx);

static void
handleAlarmClearedCommand(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx);

static void handlePollControlCheckin(uint64_t eui64,
                                     uint8_t endpointId,
                                     const ComcastBatterySavingData *batterySavingData,
                                     const void *ctx);

static void triggerBackgroundResetToFactory(uint8_t epid, uint64_t eui64);

static void handleRssiLqiUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int8_t rssi, uint8_t lqi);

static void
handleTemperatureMeasurementMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int16_t value);

static void handleBatteryVoltageUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t decivolts);

static void handleBatteryPercentageRemainingUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t percent);

static void handleBatteryChargeStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryLow);

static void handleBatteryBadStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryBad);

static void handleBatteryMissingStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryMissing);

static void handleBatteryTemperatureStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isHigh);

static void handleBatteryRechargeCyclesChanged(void *ctx, uint64_t eui64, uint16_t rechargeCycles);

static void handleAcMainsStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool acMainsConnected);

static void handleCliCommandResponse(uint64_t eui64, uint8_t endpointId, const char *response, const void *ctx);

static void destroyMapCluster(void *key, void *value);

static IcDiscoveredDeviceDetails *getDiscoveredDeviceDetails(uint64_t eui64, ZigbeeDriverCommon *commonDriver);

static void systemPowerEvent(void *ctx, DeviceServiceSystemPowerEventType powerEvent);

static void propertyChanged(void *ctx, const char *propKey, const char *propValue);

static void fetchRuntimeStats(void *ctx, icStringHashMap *output);

static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version);

static void handleSubsystemInitialized(void *ctx);

static void serviceStatusChanged(void *ctx, DeviceServiceStatus *status);

static void handleBatteryRechargeCyclesChanged(void *ctx, uint64_t eui64, uint16_t rechargeCycles);

static void updateLinkQuality(ZigbeeDriverCommon *commonDriver, const char *deviceUuid);

static void setCurrentLinkQuality(uint64_t eui64, const char *linkQuality);

static ZigbeeSubsystemLinkQualityLevel getPrevLinkQuality(uint64_t eui64);

static bool validateMD5Checksum(const char *originalMD5Checksum, const char *filePath);

static void diagnosticsCollectionTaskStart(void);
static void diagnosticsCollectionTaskStop(void);
static void diagnosticsCollectionTaskFunc(void *arg);

static bool executeCommonResource(void *ctx, icDeviceResource *resource, const char *arg, char **response);

static void updateJsonMetadata(const char *deviceUuid,
                               const char *endpointId,
                               const char *name,
                               const char *key,
                               const char *value);

// FIXME: Copy Paste Tech Debt (duplicated in deviceDescriptorHandler)
static sslVerify convertVerifyPropValToModeBarton(const char *strVal);
static const char *sslVerifyPropKeyForCategoryBarton(sslVerifyCategory cat);

typedef struct
{
    uint64_t eui64;
    uint8_t epid;
} resetToFactoryData;

typedef struct
{
    DeviceDescriptor *dd;
    struct ZigbeeDriverCommon *commonDriver;
    char *deviceUuid;
    char *endpointId;
    uint32_t *taskHandle;
} FirmwareUpgradeContext;

static pthread_mutex_t linkQualityMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

// eui64 to link quality value
static icHashMap *devicesLinkQualityMap = NULL;

static icHashMap *blockingUpgrades = NULL; // eui64 to NULL (a set)

static pthread_mutex_t blockingUpgradesMtx = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t blockingUpgradesCond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t diagCollectorMtx = PTHREAD_MUTEX_INITIALIZER;
static uint32_t diagCollectorTask = 0;

static pthread_mutex_t deviceOtaUpgradeEventMtx = PTHREAD_MUTEX_INITIALIZER;

static void destroyFirmwareUpgradeContext(FirmwareUpgradeContext *ctx);

static void
processDeviceDescriptorMetadata(ZigbeeDriverCommon *commonDriver, icDevice *device, icStringHashMap *metadata);

static void doFirmwareUpgrade(void *arg);

static void
processDeviceDescriptorMetadata(ZigbeeDriverCommon *commonDriver, icDevice *device, icStringHashMap *metadata);


static void pendingFirmwareUpgradeFreeFunc(void *key, void *value);


static void waitForUpgradesToComplete();

static void setDriverCommFailTimeout(DeviceDriver *driver, const icDevice *device, uint32_t commFailSeconds);

static const PollControlClusterCallbacks pollControlClusterCallbacks = {
    .checkin = handlePollControlCheckin,
};

static const AlarmsClusterCallbacks alarmsClusterCallbacks = {.alarmReceived = handleAlarmCommand,
                                                              .alarmCleared = handleAlarmClearedCommand};

static const DiagnosticsClusterCallbacks diagnosticsClusterCallbacks = {
    .lastMessageRssiLqiUpdated = handleRssiLqiUpdated,
};

static const TemperatureMeasurementClusterCallbacks temperatureMeasurementClusterCallbacks = {
    .measuredValueUpdated = handleTemperatureMeasurementMeasuredValueUpdated,
};

static const PowerConfigurationClusterCallbacks powerConfigurationClusterCallbacks = {
    .batteryVoltageUpdated = handleBatteryVoltageUpdated,
    .batteryPercentageRemainingUpdated = handleBatteryPercentageRemainingUpdated,
    .batteryChargeStatusUpdated = handleBatteryChargeStatusUpdated,
    .batteryBadStatusUpdated = handleBatteryBadStatusUpdated,
    .batteryMissingStatusUpdated = handleBatteryMissingStatusUpdated,
    .acMainsStatusUpdated = handleAcMainsStatusUpdated,
    .batteryTemperatureStatusUpdated = handleBatteryTemperatureStatusUpdated,
    .batteryRechargeCyclesChanged = handleBatteryRechargeCyclesChanged,
};

static const RemoteCliClusterCallbacks remoteCliClusterCallbacks = {.handleCliCommandResponse =
                                                                        handleCliCommandResponse};

DeviceDriver *zigbeeDriverCommonCreateDeviceDriver(const char *driverName,
                                                   const char *deviceClass,
                                                   uint8_t deviceClassVersion,
                                                   const uint16_t *deviceIds,
                                                   uint8_t numDeviceIds,
                                                   zigbeeReceiverMode rxMode,
                                                   const ZigbeeDriverCommonCallbacks *commonCallbacks,
                                                   bool customCommFail)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) calloc(1, sizeof(ZigbeeDriverCommon));
    commonDriver->commonCallbacks = commonCallbacks;

    // standard DeviceDriver stuff
    commonDriver->baseDriver.driverName = strdup(driverName);
    commonDriver->baseDriver.subsystemName = strdup(ZIGBEE_SUBSYSTEM_NAME);
    commonDriver->baseDriver.startup = startup;
    commonDriver->baseDriver.shutdown = driverShutdown;
    commonDriver->baseDriver.destroy = driverDestroy;
    commonDriver->baseDriver.discoverDevices = discoverStart;
    commonDriver->baseDriver.recoverDevices = discoverStart;
    commonDriver->baseDriver.stopDiscoveringDevices = discoverStop;
    commonDriver->baseDriver.configureDevice = configureDevice;
    commonDriver->baseDriver.fetchInitialResourceValues = fetchInitialResourceValues;
    commonDriver->baseDriver.registerResources = registerResources;
    commonDriver->baseDriver.devicePersisted = devicePersisted;
    commonDriver->baseDriver.readResource = readResource;
    commonDriver->baseDriver.writeResource = writeResource;
    commonDriver->baseDriver.executeResource = executeResource;
    commonDriver->baseDriver.deviceRemoved = deviceRemoved;
    commonDriver->baseDriver.communicationFailed = communicationFailed;
    commonDriver->baseDriver.communicationRestored = communicationRestored;
    commonDriver->baseDriver.supportedDeviceClasses = linkedListCreate();
    linkedListAppend(commonDriver->baseDriver.supportedDeviceClasses, strdup(deviceClass));
    commonDriver->baseDriver.processDeviceDescriptor = processDeviceDescriptor;
    commonDriver->baseDriver.synchronizeDevice = synchronizeDevice;
    commonDriver->baseDriver.endpointDisabled = endpointDisabled;
    commonDriver->baseDriver.systemPowerEvent = systemPowerEvent;
    commonDriver->baseDriver.propertyChanged = propertyChanged;
    commonDriver->baseDriver.fetchRuntimeStats = fetchRuntimeStats;
    commonDriver->baseDriver.getDeviceClassVersion = getDeviceClassVersion;
    commonDriver->baseDriver.subsystemInitialized = handleSubsystemInitialized;
    commonDriver->baseDriver.serviceStatusChanged = serviceStatusChanged;
    commonDriver->baseDriver.callbackContext = commonDriver;
    commonDriver->baseDriver.neverReject = false;
    commonDriver->baseDriver.commFailTimeoutSecsChanged = setDriverCommFailTimeout;
    commonDriver->baseDriver.customCommFail = customCommFail;

    commonDriver->deviceIds = (uint16_t *) calloc(numDeviceIds, sizeof(uint16_t));
    memcpy(commonDriver->deviceIds, deviceIds, numDeviceIds * sizeof(uint16_t));
    commonDriver->numDeviceIds = numDeviceIds;
    commonDriver->clusters = hashMapCreate();

    // Device discovered handler
    commonDriver->discoveredHandler.callback = deviceDiscoveredCallback;
    commonDriver->discoveredHandler.driverName = strdup(driverName);
    commonDriver->discoveredHandler.callbackContext = commonDriver;

    commonDriver->discoveryActive = false;
    commonDriver->discoveredDeviceDetails = hashMapCreate();
    pthread_mutex_init(&commonDriver->discoveredDeviceDetailsMtx, NULL);
    commonDriver->deviceClass = strdup(deviceClass);
    commonDriver->deviceClassVersion = ZIGBEE_DEVICE_MODEL_VERSION + deviceClassVersion;

    // Zigbee receiver mode
    commonDriver->rxMode = rxMode;

    // ZigbeeSubsystem callbacks
    commonDriver->deviceCallbacks =
        (ZigbeeSubsystemDeviceCallbacks *) calloc(1, sizeof(ZigbeeSubsystemDeviceCallbacks));
    commonDriver->deviceCallbacks->callbackContext = commonDriver;
    commonDriver->deviceCallbacks->attributeReportReceived = attributeReportReceived;
    commonDriver->deviceCallbacks->clusterCommandReceived = clusterCommandReceived;
    commonDriver->deviceCallbacks->otaUpgradeMessageSent = otaUpgradeMessageSent;
    commonDriver->deviceCallbacks->otaUpgradeMessageReceived = otaUpgradeMessageReceived;
    commonDriver->deviceCallbacks->deviceRejoined = deviceRejoined;
    commonDriver->deviceCallbacks->deviceLeft = deviceLeft;
    commonDriver->deviceCallbacks->deviceAnnounced = deviceAnnounced;

    commonDriver->batteryBackedUp = false;              // can be overwritten by higher level driver
    commonDriver->readInitialBatteryThresholds = false; // can be overwritten by higher level driver

    // Add clusters that are common across all/most devices.

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 pollControlClusterCreate(&pollControlClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 alarmsClusterCreate(&alarmsClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 diagnosticsClusterCreate(&diagnosticsClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster(
        (DeviceDriver *) commonDriver,
        temperatureMeasurementClusterCreate(&temperatureMeasurementClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 powerConfigurationClusterCreate(&powerConfigurationClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver, otaUpgradeClusterCreate());

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 remoteCliClusterCreate(&remoteCliClusterCallbacks, commonDriver));

    return (DeviceDriver *) commonDriver;
}

void zigbeeDriverCommonAddCluster(DeviceDriver *driver, ZigbeeCluster *cluster)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    uint16_t *clusterId = (uint16_t *) malloc(sizeof(uint16_t));
    *clusterId = cluster->clusterId;
    hashMapPut(commonDriver->clusters, clusterId, sizeof(cluster->clusterId), cluster);
}

void zigbeeDriverCommonSetDiagnosticsCollectionEnabled(DeviceDriver *driver, bool enabled)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    commonDriver->diganosticsCollectionEnabled = enabled;
}

void zigbeeDriverCommonRegisterBatteryThresholdResource(DeviceDriver *driver, bool enabled)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    commonDriver->readInitialBatteryThresholds = enabled;
}

void zigbeeDriverCommonSetEndpointNumber(icDeviceEndpoint *endpoint, uint8_t endpointNumber)
{
    if (endpoint != NULL)
    {
        char epid[4]; // max uint8_t in decimal plus null
        sprintf(epid, "%" PRIu8, endpointNumber);
        createEndpointMetadata(endpoint, ZIGBEE_ENDPOINT_ID_METADATA_NAME, epid);
    }
}

uint8_t zigbeeDriverCommonGetEndpointNumber(ZigbeeDriverCommon *ctx, icDeviceEndpoint *endpoint)
{
    uint8_t endpointId = 0;
    if (endpoint != NULL)
    {
        AUTO_CLEAN(free_generic__auto)
        const char *zigbeeEpId = getMetadata(endpoint->deviceUuid, endpoint->id, ZIGBEE_ENDPOINT_ID_METADATA_NAME);

        if (zigbeeEpId != NULL)
        {
            // TODO: This may be a useful stringutils function
            errno = 0;
            char *bad = NULL;
            unsigned long tmp = strtoul(zigbeeEpId, &bad, 10);

            if (!errno)
            {
                if (*bad)
                {
                    errno = EINVAL;
                }
                else if (tmp > UINT8_MAX)
                {
                    errno = ERANGE;
                }
            }

            if (!errno)
            {
                endpointId = (uint8_t) tmp;
            }
            else
            {
                AUTO_CLEAN(free_generic__auto) const char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "Unable to convert %s to a Zigbee endpoint id: %s", endpoint->id, errStr);
            }
        }
        else
        {
            icLogError(LOG_TAG,
                       "Unable to read endpoint metadata for %s on %s",
                       ZIGBEE_ENDPOINT_ID_METADATA_NAME,
                       endpoint->uri);
        }
    }

    return endpointId;
}

DeviceDescriptor *zigbeeDriverCommonGetDeviceDescriptor(const char *manufacturer,
                                                        const char *model,
                                                        uint64_t hardwareVersion,
                                                        uint64_t firmwareVersion)
{
    char hw[21]; // max uint64_t plus null
    // Convert to decimal string, as that's what we expect everywhere
    sprintf(hw, "%" PRIu64, hardwareVersion);

    AUTO_CLEAN(free_generic__auto) char *fw = getZigbeeVersionString((uint32_t) (firmwareVersion));

    return deviceDescriptorsGet(manufacturer, model, hw, fw);
}

const char *zigbeeDriverCommonGetDeviceClass(ZigbeeDriverCommon *ctx)
{
    return ctx->deviceClass;
}

uint32_t zigbeeDriverCommonGetDeviceCommFailTimeout(DeviceDriver *driver)
{
    if (driver == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid device driver", __FUNCTION__);
        return deviceServiceCommFailGetTimeoutSecs();
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;

    if (commonDriver->commFailTimeoutSeconds == 0)
    {
        icLogWarn(LOG_TAG, "%s: unable to get commFailTimeoutSeconds for driver %s", __FUNCTION__, driver->driverName);
        return deviceServiceCommFailGetTimeoutSecs();
    }

    return commonDriver->commFailTimeoutSeconds;
}

static void setDriverCommFailTimeout(DeviceDriver *driver, const icDevice *device, uint32_t commFailSeconds)
{
    if (driver == NULL)
    {
        return;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    commonDriver->commFailTimeoutSeconds = commFailSeconds;
}

void zigbeeDriverCommonSkipConfiguration(DeviceDriver *driver)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    commonDriver->skipConfiguration = true;
}

static void migrateZigbeeCommonVersion(ZigbeeDriverCommon *commonDriver, icDevice *device)
{
    AUTO_CLEAN(free_generic__auto)
    char *commonVersionStr = getMetadata(device->uuid, NULL, ZIGBEE_COMMON_VERSION_METADATA);

    // Version 1 had no metadata, so default to that
    uint8_t commonVersion = 1;
    if (commonVersionStr != NULL)
    {
        if (stringToUint8(commonVersionStr, &commonVersion) == false)
        {
            icLogWarn(LOG_TAG, "Invalid common version detected %s", commonVersionStr);
        }
    }

    if (commonVersion < ZIGBEE_COMMON_VERSION)
    {
        icLogInfo(LOG_TAG,
                  "Migrating zigbeeCommon version %d -> %d on %s",
                  commonVersion,
                  ZIGBEE_COMMON_VERSION,
                  device->uuid);

        if (commonVersion < 2)
        {
            // Create linkQuality resource
            icDeviceResource *res = createDeviceResource(device,
                                                         COMMON_DEVICE_RESOURCE_LINK_QUALITY,
                                                         LINK_QUALITY_UNKNOWN,
                                                         RESOURCE_TYPE_LINK_QUALITY,
                                                         RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                             RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                                         CACHING_POLICY_ALWAYS);

            // Add to device
            addNewResource(device->uri, res);
        }

        // update driver common version metadata
        AUTO_CLEAN(free_generic__auto) char *versionString = stringBuilder("%d", ZIGBEE_COMMON_VERSION);
        setMetadata(device->uuid, NULL, ZIGBEE_COMMON_VERSION_METADATA, versionString);
    }
}

static void startup(void *ctx)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *driverName = commonDriver->baseDriver.driverName;

    icLogDebug(LOG_TAG, "startup %s", driverName);

    bool startDiagCollector = false;
    commonDriver->pendingFirmwareUpgrades = hashMapCreate();
    pthread_mutex_init(&commonDriver->pendingFirmwareUpgradesMtx, NULL);

    if (commonDriver->commonCallbacks->preStartup != NULL)
    {
        commonDriver->commonCallbacks->preStartup(ctx, &commonDriver->commFailTimeoutSeconds);
    }

    icLinkedList *devices = deviceServiceGetDevicesByDeviceDriver(driverName);
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);

    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);

        migrateZigbeeCommonVersion(commonDriver, device);

        zigbeeSubsystemRegisterDeviceListener(zigbeeSubsystemIdToEui64(device->uuid), commonDriver->deviceCallbacks);

        // if this driver uses the diagnostics collection task and has at least 1 device, we need to start the collector
        if (commonDriver->diganosticsCollectionEnabled == true)
        {
            startDiagCollector = true;
        }
    }
    linkedListIteratorDestroy(iterator);

    if (commonDriver->commonCallbacks->devicesLoaded != NULL)
    {
        commonDriver->commonCallbacks->devicesLoaded(ctx, devices);
    }

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    zigbeeSubsystemRegisterDiscoveryHandler(commonDriver->baseDriver.driverName, &commonDriver->discoveredHandler);

    if (commonDriver->commonCallbacks->postStartup != NULL)
    {
        commonDriver->commonCallbacks->postStartup(ctx);
    }

    if (startDiagCollector == true)
    {
        diagnosticsCollectionTaskStart(); // safe to call multiple times -- it will only start once
    }
}

static void driverShutdown(void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *driverName = commonDriver->baseDriver.driverName;

    icLogDebug(LOG_TAG, "shutdown %s", driverName);

    zigbeeSubsystemUnregisterDiscoveryHandler(&commonDriver->discoveredHandler);

    if (commonDriver->commonCallbacks->preShutdown != NULL)
    {
        commonDriver->commonCallbacks->preShutdown(ctx);
    }

    icLinkedList *devices = deviceServiceGetDevicesByDeviceDriver(driverName);
    if (devices != NULL)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
        while (linkedListIteratorHasNext(iterator))
        {
            icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
            zigbeeSubsystemUnregisterDeviceListener(zigbeeSubsystemIdToEui64(device->uuid));

            // stop monitoring this device
            deviceCommunicationWatchdogStopMonitoringDevice(device->uuid);
        }
        linkedListIteratorDestroy(iterator);
        linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
    }

    // Cancel all pending upgrade tasks and wait for any that are running to complete
    zigbeeDriverCommonCancelPendingUpgrades(commonDriver, NULL);
    waitForUpgradesToComplete();

    hashMapDestroy(commonDriver->pendingFirmwareUpgrades, pendingFirmwareUpgradeFreeFunc);
    commonDriver->pendingFirmwareUpgrades = NULL;

    if (commonDriver->commonCallbacks->postShutdown != NULL)
    {
        commonDriver->commonCallbacks->postShutdown(ctx);
    }

    pthread_mutex_destroy(&commonDriver->pendingFirmwareUpgradesMtx);

    diagnosticsCollectionTaskStop();
}

static void driverDestroy(void *ctx)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, commonDriver->baseDriver.driverName);

    if (commonDriver->commonCallbacks->preDeleteDriver != NULL)
    {
        commonDriver->commonCallbacks->preDeleteDriver(ctx);
    }

    free(commonDriver->discoveredHandler.driverName);
    free(commonDriver->deviceCallbacks);
    free(commonDriver->deviceClass);
    free(commonDriver->deviceIds);

    hashMapDestroy(commonDriver->discoveredDeviceDetails, discoveredDeviceDetailsFreeFunc);
    commonDriver->discoveredDeviceDetails = NULL;
    pthread_mutex_destroy(&commonDriver->discoveredDeviceDetailsMtx);

    hashMapDestroy(commonDriver->clusters, destroyMapCluster);
    commonDriver->commonCallbacks = NULL;
    hashMapDestroy(blockingUpgrades, NULL);
    blockingUpgrades = NULL;

    // Must free baseDriver resources since we are implementing a destroy callback
    free(commonDriver->baseDriver.driverName);
    free(commonDriver->baseDriver.subsystemName);
    linkedListDestroy(commonDriver->baseDriver.supportedDeviceClasses, NULL);
    hashMapDestroy(commonDriver->baseDriver.endpointProfileVersions, NULL);

    free(commonDriver);
}

static uint8_t getEndpointNumber(void *ctx, const char *deviceUuid, const char *endpointId)
{
    uint8_t result = 0; // an invalid endpoint

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL)
    {
        char *epid = getMetadata(deviceUuid, endpointId, ZIGBEE_ENDPOINT_ID_METADATA_NAME);
        if (epid != NULL)
        {
            result = (uint8_t) atoi(epid);
            free(epid);
        }
    }

    return result;
}

static void deviceRemoved(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL && device != NULL && device->uuid != NULL)
    {
        // stop monitoring this device
        deviceCommunicationWatchdogStopMonitoringDevice(device->uuid);

        if (commonDriver->commonCallbacks->preDeviceRemoved != NULL)
        {
            commonDriver->commonCallbacks->preDeviceRemoved(ctx, device);
        }

        uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

        zigbeeSubsystemUnregisterDeviceListener(eui64);
        zigbeeSubsystemRemoveDeviceAddress(eui64);

        // just tell the first endpoint to reset to factory and leave... that should get the whole device
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListGetElementAt(device->endpoints, 0);
        bool willReset = false;
        if (endpoint != NULL)
        {
            sbIcLinkedListIterator *it = linkedListIteratorCreate(endpoint->metadata);
            while (linkedListIteratorHasNext(it) == true)
            {
                icDeviceMetadata *metadata = linkedListIteratorGetNext(it);
                if (strcmp(metadata->id, ZIGBEE_ENDPOINT_ID_METADATA_NAME) == 0)
                {
                    const char *epName = metadata->value;
                    if (epName != NULL)
                    {
                        errno = 0;
                        char *bad = NULL;
                        long epid = strtoul(epName, &bad, 10);

                        if (errno == 0 && *bad == 0 && epid <= UINT8_MAX)
                        {
                            // Kick this off in the background so we don't block for devices which are sleepy
                            triggerBackgroundResetToFactory((uint8_t) epid, eui64);
                            willReset = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!willReset)
        {
            icLogWarn(LOG_TAG, "Removed device was not told to reset to factory");
        }

        mutexLock(&linkQualityMutex);
        hashMapDelete(devicesLinkQualityMap, &eui64, sizeof(uint64_t), NULL);
        mutexUnlock(&linkQualityMutex);

        if (commonDriver->commonCallbacks->postDeviceRemoved != NULL)
        {
            commonDriver->commonCallbacks->postDeviceRemoved(ctx, device);
        }

        zigbeeDriverCommonCancelPendingUpgrades(commonDriver, device->uuid);
    }

    // Go through and remove and unused firmware files now that something has been removed
    // This will do an overall scan, which is more work than is needed, but this event
    // is rare so this overhead is minimal
    zigbeeSubsystemCleanupFirmwareFiles();
}

static void communicationFailed(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL && device != NULL && device->uuid != NULL)
    {
        if (commonDriver->commonCallbacks->communicationFailed != NULL)
        {
            commonDriver->commonCallbacks->communicationFailed(ctx, device);
        }

        updateResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_COMM_FAIL, "true", NULL);
    }

    /*
     * This must be done *after* any resource updates;
     * zigbeeSubsystem needs to know if all devices are in comm fail
     */
    zigbeeSubsystemNotifyDeviceCommFail(device);
}

static void communicationRestored(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL && device != NULL && device->uuid != NULL)
    {
        if (commonDriver->commonCallbacks->communicationRestored != NULL)
        {
            commonDriver->commonCallbacks->communicationRestored(ctx, device);
        }

        updateResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_COMM_FAIL, "false", NULL);
    }

    zigbeeSubsystemNotifyDeviceCommRestored(device);
}

static void destroyFirmwareUpgradeContext(FirmwareUpgradeContext *ctx)
{
    deviceDescriptorFree(ctx->dd);
    free(ctx->deviceUuid);
    free(ctx->endpointId);
    free(ctx->taskHandle);
    free(ctx);
}

static bool processDeviceDescriptor(void *ctx, icDevice *device, DeviceDescriptor *dd)
{
    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (dd == NULL)
    {
        icLogWarn(LOG_TAG, "processDeviceDescriptor: NULL dd argument; ignoring");
        return true;
    }

    if (device == NULL)
    {
        icLogWarn(LOG_TAG, "processDeviceDescriptor: NULL device argument; ignoring");
        return false;
    }

    if (dd->latestFirmware == NULL || dd->latestFirmware->version == NULL)
    {
        icLogWarn(LOG_TAG, "processDeviceDescriptor: No latest firmware for dd uuid: %s; ignoring", dd->uuid);
        return true;
    }

    icLogDebug(LOG_TAG, "processDeviceDescriptor: %s", device->uuid);

    // Get the device's current firmware version
    icDeviceResource *firmwareVersionResource = (icDeviceResource *) linkedListFind(
        device->resources, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, findDeviceResource);

    if (firmwareVersionResource != NULL && firmwareVersionResource->value != NULL)
    {
        // Get a handle to the provided device's firmware update status resource so we can
        //  update it as well as attempting to update in the database since during pairing
        //  it isnt persisted yet.
        icDeviceResource *firmwareUpdateStatusResource = (icDeviceResource *) linkedListFind(
            device->resources, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, findDeviceResource);

        bool firmwareUpgradeRequired = false;
        if (commonDriver->commonCallbacks->firmwareUpgradeRequired != NULL)
        {
            firmwareUpgradeRequired = commonDriver->commonCallbacks->firmwareUpgradeRequired(
                ctx, device->uuid, dd->latestFirmware->version, firmwareVersionResource->value);
        }
        else
        {
            int versionComparison = compareVersionStrings(dd->latestFirmware->version, firmwareVersionResource->value);
            firmwareUpgradeRequired = versionComparison == -1;
        }

        if (firmwareUpgradeRequired)
        {
            icLogDebug(LOG_TAG,
                       "processDeviceDescriptor: New firmware for device %s, at version %s, latest version %s",
                       device->uuid,
                       firmwareVersionResource->value,
                       dd->latestFirmware->version);

            // We want to avoid clobbering any state other than what is covered below, lest we overwrite important
            // information.
            if (firmwareUpdateStatusResource == NULL || firmwareUpdateStatusResource->value == NULL ||
                stringCompare(firmwareUpdateStatusResource->value, FIRMWARE_UPDATE_STATUS_FAILED, false) == 0 ||
                stringCompare(firmwareUpdateStatusResource->value, FIRMWARE_UPDATE_STATUS_COMPLETED, false) == 0 ||
                stringCompare(firmwareUpdateStatusResource->value, FIRMWARE_UPDATE_STATUS_UP_TO_DATE, false) == 0)
            {
                updateResource(device->uuid,
                               NULL,
                               COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                               FIRMWARE_UPDATE_STATUS_PENDING,
                               NULL);
                if (firmwareUpdateStatusResource != NULL)
                {
                    free(firmwareUpdateStatusResource->value);
                    firmwareUpdateStatusResource->value = strdup(FIRMWARE_UPDATE_STATUS_PENDING);
                }
            }

            if (dd->latestFirmware->fileInfos != NULL)
            {
                DeviceDescriptor *ddCopy = deviceDescriptorClone(dd);
                if (ddCopy == NULL)
                {
                    icLogError(LOG_TAG, "Failed to clone device descriptor for firmware download");
                    result = false;
                }
                else
                {
                    FirmwareUpgradeContext *firmwareUpgradeContext =
                        (FirmwareUpgradeContext *) calloc(1, sizeof(FirmwareUpgradeContext));
                    firmwareUpgradeContext->dd = ddCopy;
                    firmwareUpgradeContext->deviceUuid = strdup(device->uuid);

                    icDeviceEndpoint *firstEndpoint = linkedListGetElementAt(device->endpoints, 0);
                    if (firstEndpoint != NULL)
                    {
                        firmwareUpgradeContext->endpointId = strdup(firstEndpoint->id);
                    }

                    firmwareUpgradeContext->taskHandle = malloc(sizeof(uint32_t));
                    firmwareUpgradeContext->commonDriver = commonDriver;

                    uint32_t delaySeconds = 1;
                    g_autoptr(BCorePropertyProvider) propertyProvider =
                        deviceServiceConfigurationGetPropertyProvider();
                    bool noDelay = b_core_property_provider_get_property_as_bool(
                        propertyProvider, ZIGBEE_FW_UPGRADE_NO_DELAY_BOOL_PROPERTY, false);

                    if (!noDelay)
                    {
                        delaySeconds = b_core_property_provider_get_property_as_uint32(
                            propertyProvider, FIRMWARE_UPGRADE_DELAYSECS, FIRMWARE_UPGRADE_DELAYSECS_DEFAULT);
                    }

                    // Cancel if there is any pending upgrade task for this driver already scheduled
                    // before scheduling a new one
                    zigbeeDriverCommonCancelPendingUpgrades(commonDriver, device->uuid);

                    pthread_mutex_lock(&commonDriver->pendingFirmwareUpgradesMtx);

                    icLogInfo(LOG_TAG,
                              "%s: scheduling firmware upgrade to start in %" PRIu32 " seconds.",
                              __FUNCTION__,
                              delaySeconds);

                    *firmwareUpgradeContext->taskHandle =
                        scheduleDelayTask(delaySeconds, DELAY_SECS, doFirmwareUpgrade, firmwareUpgradeContext);

                    if (*firmwareUpgradeContext->taskHandle == 0)
                    {
                        icLogError(LOG_TAG, "Failed to add task for firmware upgrade");
                        destroyFirmwareUpgradeContext(firmwareUpgradeContext);
                        result = false;
                    }
                    else
                    {
                        hashMapPut(commonDriver->pendingFirmwareUpgrades,
                                   firmwareUpgradeContext->taskHandle,
                                   sizeof(uint32_t),
                                   firmwareUpgradeContext);
                    }

                    pthread_mutex_unlock(&commonDriver->pendingFirmwareUpgradesMtx);
                }
            }
            else
            {
                icLogWarn(LOG_TAG, "No filenames in DD for uuid: %s", dd->uuid);
            }
        }
        else
        {
            icLogDebug(LOG_TAG, "Device %s does not need a firmware upgrade, skipping download", device->uuid);

            updateResource(device->uuid,
                           NULL,
                           COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                           FIRMWARE_UPDATE_STATUS_UP_TO_DATE,
                           NULL);

            if (firmwareUpdateStatusResource != NULL)
            {
                free(firmwareUpdateStatusResource->value);
                firmwareUpdateStatusResource->value = strdup(FIRMWARE_UPDATE_STATUS_UP_TO_DATE);
            }
        }

        if (dd->metadata != NULL)
        {
            processDeviceDescriptorMetadata(commonDriver, device, dd->metadata);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Unable to find firmware resource for device %s", device->uuid);
    }

    return result;
}

static bool discoverStart(void *ctx, const char *deviceClass)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: %s deviceClass=%s", __FUNCTION__, commonDriver->baseDriver.driverName, deviceClass);

    commonDriver->discoveryActive = true;

    zigbeeSubsystemStartDiscoveringDevices();

    return true;
}

static void discoverStop(void *ctx, const char *deviceClass)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(
        LOG_TAG, "%s: %s stopping discovery of %s", __FUNCTION__, commonDriver->baseDriver.driverName, deviceClass);

    commonDriver->discoveryActive = false;

    zigbeeSubsystemStopDiscoveringDevices();

    if (commonDriver->commonCallbacks->postDiscoverStop != NULL)
    {
        commonDriver->commonCallbacks->postDiscoverStop(ctx);
    }
}

/*
 * For the provided clusterIds, get the attribute infos and store them in the pre-allocated clusterDetails list
 */
static bool getAttributeInfos(uint64_t eui64,
                              uint8_t endpointId,
                              IcDiscoveredClusterDetails *clusterDetails,
                              uint8_t numClusterDetails)
{
    bool result = true;

    for (uint8_t i = 0; i < numClusterDetails; i++)
    {
        zhalAttributeInfo *attributeInfos = NULL;
        uint16_t numAttributeInfos = 0;

        if (zhalGetAttributeInfos(eui64,
                                  endpointId,
                                  clusterDetails[i].clusterId,
                                  clusterDetails[i].isServer,
                                  &attributeInfos,
                                  &numAttributeInfos) == 0)
        {
            clusterDetails[i].numAttributeIds = numAttributeInfos;
            // Cleanup the old stuff before allocating something new
            free(clusterDetails[i].attributeIds);
            clusterDetails[i].attributeIds = (uint16_t *) calloc(numAttributeInfos, sizeof(uint16_t));
            for (uint16_t j = 0; j < numAttributeInfos; j++)
            {
                clusterDetails[i].attributeIds[j] = attributeInfos[j].id;
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: failed to get attribute infos", __FUNCTION__);
            result = false;
        }

        free(attributeInfos);
    }

    return result;
}

static bool getDeviceAttributeInfos(uint64_t eui64, IcDiscoveredDeviceDetails *deviceDetails)
{
    bool result = true;

    for (int i = 0; i < deviceDetails->numEndpoints; i++)
    {
        // get server cluster attributes
        result = getAttributeInfos(deviceDetails->eui64,
                                   deviceDetails->endpointDetails[i].endpointId,
                                   deviceDetails->endpointDetails[i].serverClusterDetails,
                                   deviceDetails->endpointDetails[i].numServerClusterDetails);

        if (!result)
        {
            icLogError(LOG_TAG, "%s: failed to discover server cluster attributes", __FUNCTION__);
            break;
        }

        // get client cluster attributes
        result = getAttributeInfos(deviceDetails->eui64,
                                   deviceDetails->endpointDetails[i].endpointId,
                                   deviceDetails->endpointDetails[i].clientClusterDetails,
                                   deviceDetails->endpointDetails[i].numClientClusterDetails);

        if (!result)
        {
            icLogError(LOG_TAG, "%s: failed to discover client cluster attributes", __FUNCTION__);
            break;
        }
    }

    return result;
}

/**
 * Compute the cluster configuration order. Clusters with the same priority will be in no particular order within the
 * priority band.
 */
static icLinkedList *createClusterOrder(ZigbeeDriverCommon *commonDriver)
{
    icLinkedList *orderedClusters = linkedListCreate();

    icHashMapIterator *it = hashMapIteratorCreate(commonDriver->clusters);
    while (hashMapIteratorHasNext(it))
    {
        uint16_t *clusterId;
        uint16_t keyLen;
        ZigbeeCluster *cluster;

        hashMapIteratorGetNext(it, (void **) &clusterId, &keyLen, (void **) &cluster);

        // TODO: support insertAfter(void *) in icLinkedList to build a nicely ordered list
        switch (cluster->priority)
        {
            case CLUSTER_PRIORITY_DEFAULT:
                linkedListAppend(orderedClusters, cluster);
                break;
            case CLUSTER_PRIORITY_HIGHEST:
                linkedListPrepend(orderedClusters, cluster);
                break;
            default:
                icLogWarn(
                    LOG_TAG, "Cluster priority [%d] not supported, assigning default priority", cluster->priority);
                break;
        }
    }
    hashMapIteratorDestroy(it);

    return orderedClusters;
}

bool zigbeeDriverCommonConfigureEndpointClusters(uint64_t eui64,
                                                 uint8_t endpointId,
                                                 ZigbeeDriverCommon *commonDriver,
                                                 IcDiscoveredDeviceDetails *deviceDetails,
                                                 DeviceDescriptor *descriptor)
{
    bool result = true;

    DeviceConfigurationContext deviceConfigContext;
    memset(&deviceConfigContext, 0, sizeof(DeviceConfigurationContext));
    deviceConfigContext.eui64 = eui64;
    deviceConfigContext.endpointId = endpointId;
    deviceConfigContext.deviceDescriptor = descriptor;
    deviceConfigContext.configurationMetadata = stringHashMapCreate();
    deviceConfigContext.discoveredDeviceDetails = deviceDetails;

    // allow each cluster to perform its configuration
    icLinkedList *orderedClusters = createClusterOrder(commonDriver);
    icLinkedListIterator *it = linkedListIteratorCreate(orderedClusters);
    while (linkedListIteratorHasNext(it))
    {
        ZigbeeCluster *cluster = linkedListIteratorGetNext(it);
        if (cluster->configureCluster != NULL)
        {
            // if this endpoint has this cluster, let the cluster configure it
            bool endpointHasCluster =
                icDiscoveredDeviceDetailsEndpointHasCluster(deviceDetails, endpointId, cluster->clusterId, true);

            // if this cluster wasnt in the server clusters, maybe its in the client
            if (endpointHasCluster == false)
            {
                endpointHasCluster =
                    icDiscoveredDeviceDetailsEndpointHasCluster(deviceDetails, endpointId, cluster->clusterId, false);
            }

            if (endpointHasCluster)
            {
                bool doConfigure = true;

                if (commonDriver->commonCallbacks->preConfigureCluster != NULL)
                {
                    // Let the driver do any preconfiguration of the cluster and tell us whether it wants a cluster
                    // configured
                    doConfigure =
                        commonDriver->commonCallbacks->preConfigureCluster(commonDriver, cluster, &deviceConfigContext);
                }
                if (doConfigure)
                {
                    if (cluster->configureCluster(cluster, &deviceConfigContext) == false)
                    {
                        icLogError(LOG_TAG, "%s: cluster 0x%04x failed to configure", __FUNCTION__, cluster->clusterId);
                        result = false;
                        break;
                    }
                }
            }
        }
    }
    linkedListIteratorDestroy(it);
    linkedListDestroy(orderedClusters, standardDoNotFreeFunc);

    stringHashMapDestroy(deviceConfigContext.configurationMetadata, NULL);

    return result;
}

static bool configureClusters(uint64_t eui64,
                              ZigbeeDriverCommon *commonDriver,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              DeviceDescriptor *descriptor)
{
    bool result = true;

    for (int i = 0; result && i < discoveredDeviceDetails->numEndpoints; i++)
    {
        IcDiscoveredEndpointDetails *endpointDetails = &discoveredDeviceDetails->endpointDetails[i];
        result = zigbeeDriverCommonConfigureEndpointClusters(
            eui64, endpointDetails->endpointId, commonDriver, discoveredDeviceDetails, descriptor);
    }

    return result;
}

static void registerNewDevice(icDevice *device, ZigbeeSubsystemDeviceCallbacks *deviceCallbacks)
{
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    zigbeeSubsystemRegisterDeviceListener(eui64, deviceCallbacks);
}

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor)
{
    bool result = true;

    if (ctx == NULL || device == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid args", __FUNCTION__);
        return false;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;
    icLogDebug(LOG_TAG, "%s: %s configuring %s", __FUNCTION__, commonDriver->baseDriver.driverName, device->uuid);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    IcDiscoveredDeviceDetails *discoveredDeviceDetails = getDiscoveredDeviceDetails(eui64, commonDriver);

    if (discoveredDeviceDetails == NULL)
    {
        icLogError(LOG_TAG, "%s: discovered device details not found", __FUNCTION__);
        return false;
    }

    // This check was added to zigbeeDoorLockDeviceDriver.c in 52cf397e73fa5cda42fcb88e58e27039f8cf9ae9 for
    // XHFW-1288. In order to achieve the same goal, moved the check to the common driver during configuration.
    // The idea is we may not have the power source at this time due to a migration bug (and thus don't want to
    // reconfigure), but after the device gets announced it will be populated correctly and another go at
    // reconfiguration should work.
    if (discoveredDeviceDetails->powerSource == powerSourceUnknown)
    {
        icLogWarn(LOG_TAG,
                  "%s: device %s has powerSource of unknown, skipping configuration at this time",
                  __func__,
                  device->uuid);
        return false;
    }

    bool isReconfigurationPending = false;
    if (commonDriver->skipConfiguration == false)
    {
        // While re-configuring sleepy devices we need to wait for poll control checkin to arrive,
        // and the device to switch to fast poll mode before we can communicate with it to do a
        // reconfiguration. We don't need to wait for non-sleepy devices and pseduo sleepy devices.
        // Assuming legacy sleepy devices will never have a reconfiguration task scheduled.
        // Devices that are not being reconfigured are going through initial configuration, which runs now.
        isReconfigurationPending = deviceServiceIsReconfigurationPending(device->uuid);
        if (commonDriver->rxMode == RX_MODE_SLEEPY && isReconfigurationPending)
        {
            if (deviceServiceWaitForReconfigure(device->uuid) == false)
            {
                icError("Waiting for poll control checkin to arrive failed");
                return false;
            }
        }

        // Before we configure the device, perform the detailed discovery of its attributes so we know what type of
        //  capabilities we should configure
        result = getDeviceAttributeInfos(eui64, discoveredDeviceDetails);

        if (!result)
        {
            goto exit;
        }

        // allow each cluster to perform its configuration
        result = configureClusters(eui64, commonDriver, discoveredDeviceDetails, descriptor);

        if (!result)
        {
            goto exit;
        }
    }

    if (commonDriver->commonCallbacks->configureDevice != NULL)
    {
        if (commonDriver->commonCallbacks->configureDevice(ctx, device, descriptor, discoveredDeviceDetails) == false)
        {
            icLogError(LOG_TAG, "%s: higher level driver failed to configure device", __FUNCTION__);
            result = false;
            goto exit;
        }
    }

    if (commonDriver->rxMode == RX_MODE_SLEEPY && isReconfigurationPending)
    {
        // take the device out of fast poll mode in case of reconfiguration
        pollControlClusterStopFastPoll(eui64, discoveredDeviceDetails->endpointDetails[0].endpointId);
    }

exit:
    return result;
}

static char *getDefaultLabel(icInitialResourceValues *initialResourceValues, const char *uuid)
{
    char *label = NULL;

    const char *manufacturer =
        initialResourceValuesGetDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_MANUFACTURER);

    if (manufacturer != NULL && strlen(uuid) >= 4)
    {
        label = stringBuilder("%s%s", manufacturer, uuid + strlen(uuid) - 4);
    }

    return label;
}

// register resources that all zigbee devices would have.  It could determine which resources to add based
//  on information in the device (if it has a battery, etc)
static bool registerCommonZigbeeResources(ZigbeeDriverCommon *commonDriver,
                                          icDevice *device,
                                          IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                          icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    if (device == NULL || discoveredDeviceDetails == NULL || initialResourceValues == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLinkedListIterator *it = NULL;
    char *driverCommonVersion = NULL;

    // first add the discoveredDetails metadata
    cJSON *deviceDetailsJson = icDiscoveredDeviceDetailsToJson(discoveredDeviceDetails);
    char *detailsStr = cJSON_PrintUnformatted(deviceDetailsJson);
    cJSON_Delete(deviceDetailsJson);
    if (createDeviceMetadata(device, DISCOVERED_DEVICE_DETAILS, detailsStr) == NULL)
    {
        icLogError(LOG_TAG, "%s: failed to create discovered details metadata", __FUNCTION__);
        result = false;
        free(detailsStr);
        goto exit;
    }
    free(detailsStr);


    // create resources common for all endpoints created by the concrete driver
    it = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(it))
    {
        icDeviceEndpoint *endpoint = linkedListIteratorGetNext(it);

        scoped_icDeviceResource *labelResource =
            deviceServiceGetResourceById(endpoint->deviceUuid, endpoint->id, COMMON_ENDPOINT_RESOURCE_LABEL);
        const char *label = NULL;
        if (deviceServiceIsInRecoveryMode() == true && labelResource != NULL)
        {
            // if device joined as a part of recovery then assign label value from devicedb
            label = labelResource->value;
        }
        else
        {
            // Create this regardless if there is a value there or not
            label = initialResourceValuesGetEndpointValue(
                initialResourceValues, endpoint->id, COMMON_ENDPOINT_RESOURCE_LABEL);
        }

        AUTO_CLEAN(free_generic__auto) char *defaultLabel = NULL;

#ifdef BARTON_CONFIG_GENERATE_DEFAULT_LABELS
        // if there was no label in the initialResourceValues, create a default one
        if (label == NULL)
        {
            defaultLabel = getDefaultLabel(initialResourceValues, device->uuid);
        }
#endif

        result &=
            createEndpointResource(endpoint,
                                   COMMON_ENDPOINT_RESOURCE_LABEL,
                                   label == NULL ? defaultLabel : label,
                                   RESOURCE_TYPE_LABEL,
                                   RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                   CACHING_POLICY_ALWAYS) != NULL;
    }
    linkedListIteratorDestroy(it);

    // fe rssi
    result &=
        createDeviceResourceIfAvailable(device,
                                        COMMON_DEVICE_RESOURCE_FERSSI,
                                        initialResourceValues,
                                        RESOURCE_TYPE_RSSI,
                                        RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                        CACHING_POLICY_ALWAYS) != NULL;

    // fe lqi
    result &=
        createDeviceResourceIfAvailable(device,
                                        COMMON_DEVICE_RESOURCE_FELQI,
                                        initialResourceValues,
                                        RESOURCE_TYPE_LQI,
                                        RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                        CACHING_POLICY_ALWAYS) != NULL;

    // ne rssi
    result &=
        createDeviceResourceIfAvailable(device,
                                        COMMON_DEVICE_RESOURCE_NERSSI,
                                        initialResourceValues,
                                        RESOURCE_TYPE_RSSI,
                                        RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                        CACHING_POLICY_ALWAYS) != NULL;

    // ne lqi
    result &=
        createDeviceResourceIfAvailable(device,
                                        COMMON_DEVICE_RESOURCE_NELQI,
                                        initialResourceValues,
                                        RESOURCE_TYPE_LQI,
                                        RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                        CACHING_POLICY_ALWAYS) != NULL;

    // linkQuality
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_LINK_QUALITY,
                                              initialResourceValues,
                                              RESOURCE_TYPE_LINK_QUALITY,
                                              RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                  RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                              CACHING_POLICY_ALWAYS) != NULL;

    // temperature: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_TEMPERATURE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_TEMPERATURE,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    // highTemperature: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_HIGH_TEMPERATURE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS |
                                        RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    // battery low: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_LOW,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    // battery voltage: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BATTERY_VOLTAGE,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS |
                                        RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    // ac mains connected: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    // battery bad: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_BAD,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    // battery missing: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_MISSING,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    // battery high temperature: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_HIGH_TEMPERATURE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    // battery percentage remaining: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING,
                                    initialResourceValues,
                                    RESOURCE_TYPE_PERCENTAGE,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    // last user interaction date: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_DATETIME,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS |
                                        RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_THRESHOLDS,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BATTERY_THRESHOLDS,
                                    RESOURCE_MODE_READABLE,
                                    CACHING_POLICY_ALWAYS);

    driverCommonVersion = stringBuilder("%" PRIu32, ZIGBEE_COMMON_VERSION);
    if (createDeviceMetadata(device, ZIGBEE_COMMON_VERSION_METADATA, driverCommonVersion) == NULL)
    {
        icLogError(LOG_TAG, "Failed creating driver common version metadata");
        result = false;
    }
    free(driverCommonVersion);

exit:

    return result;
}

// read all battery voltage and battery percentage thresholds
static bool readBatteryThresholds(IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                  icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    // we will assume that the attributes we are after are on the first endpoint
    PowerConfigurationClusterBatteryThresholds *thresholdValues = powerConfigurationClusterGetBatteryThresholds(
        discoveredDeviceDetails->eui64, discoveredDeviceDetails->endpointDetails[0].endpointId);

    if (thresholdValues != NULL)
    {
        // it requires a single resource with values stored in JSON object
        //
        scoped_cJSON *root = cJSON_CreateObject();

        if (thresholdValues->minThresholdDecivolts != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryMinThresholdDecivolts", *thresholdValues->minThresholdDecivolts);
        }

        if (thresholdValues->threshold1Decivolts != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryThreshold1Decivolts", *thresholdValues->threshold1Decivolts);
        }

        if (thresholdValues->threshold2Decivolts != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryThreshold2Decivolts", *thresholdValues->threshold2Decivolts);
        }

        if (thresholdValues->threshold3Decivolts != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryThreshold3Decivolts", *thresholdValues->threshold3Decivolts);
        }

        if (thresholdValues->minThresholdPercent != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryMinThresholdPercentage", *thresholdValues->minThresholdPercent);
        }

        if (thresholdValues->threshold1Percent != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryThreshold1Percentage", *thresholdValues->threshold1Percent);
        }

        if (thresholdValues->threshold2Percent != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryThreshold2Percentage", *thresholdValues->threshold2Percent);
        }

        if (thresholdValues->threshold3Percent != NULL)
        {
            cJSON_AddNumberToObject(root, "BatteryThreshold3Percentage", *thresholdValues->threshold3Percent);
        }

        scoped_generic char *jsonStr = cJSON_PrintUnformatted(root);
        result = initialResourceValuesPutDeviceValue(
            initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_THRESHOLDS, jsonStr);

        // cleanup
        //
        powerConfigurationClusterBatteryThresholdsDestroy(thresholdValues);
    }
    else
    {
        result = false;
    }

    return result;
}

// fetch resources values that all zigbee devices would have.  It could determine which resources to populate based
//  on information in the device (if it has a battery, etc)
static bool fetchCommonZigbeeResourceValues(ZigbeeDriverCommon *commonDriver,
                                            icDevice *device,
                                            IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                            icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    if (device == NULL || discoveredDeviceDetails == NULL || initialResourceValues == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    const char *linkQuality = LINK_QUALITY_UNKNOWN;
    uint8_t epid;
    if (icDiscoveredDeviceDetailsGetAttributeEndpoint(discoveredDeviceDetails,
                                                      DIAGNOSTICS_CLUSTER_ID,
                                                      DIAGNOSTICS_LAST_MESSAGE_RSSI_ATTRIBUTE_ID, // if it has either...
                                                      &epid))
    {
        char tmp[5]; //-127 + \0 worst case

        // fe rssi
        int8_t rssi;
        if (diagnosticsClusterGetLastMessageRssi(discoveredDeviceDetails->eui64, epid, &rssi))
        {
            snprintf(tmp, 5, "%" PRId8, rssi);
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FERSSI, tmp);
        }
        else
        {
            result = false;
            goto exit;
        }

        // fe lqi
        uint8_t lqi;
        if (diagnosticsClusterGetLastMessageLqi(discoveredDeviceDetails->eui64, epid, &lqi))
        {
            snprintf(tmp, 4, "%" PRIu8, lqi);
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FELQI, tmp);
        }
        else
        {
            result = false;
            goto exit;
        }

        linkQuality = zigbeeSubsystemDetermineLinkQuality(INT8_MIN, rssi, 0, lqi, LINK_QUALITY_LEVEL_UNKNOWN);
        // set the newly discovered device linkQuality in the deviceLinkQualityMap
        setCurrentLinkQuality(discoveredDeviceDetails->eui64, linkQuality);
    }
    else
    {
        // Just provide NULL defaults if the driver didn't provide anything, since these are resources we always create
        initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_FERSSI, NULL);
        initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_FELQI, NULL);
    }

    // Just default these with NULL, they don't get populated until later
    initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_NERSSI, NULL);
    initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_NELQI, NULL);
    // Default based on fe lqi/rssi if available.
    initialResourceValuesPutDeviceValueIfNotExists(
        initialResourceValues, COMMON_DEVICE_RESOURCE_LINK_QUALITY, linkQuality);

    if (icDiscoveredDeviceDetailsGetAttributeEndpoint(
            discoveredDeviceDetails, TEMPERATURE_MEASUREMENT_CLUSTER_ID, TEMP_MEASURED_VALUE_ATTRIBUTE_ID, &epid))
    {
        // Provide NULL default with the expectation that it eventually gets replaced upon a valid temperature reading.
        // Sometimes devices report an invalid value to indicate that they don't yet have a value to report.
        initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_TEMPERATURE, NULL);

        // temperature
        int16_t value;

        if (temperatureMeasurementClusterGetMeasuredValue(discoveredDeviceDetails->eui64, epid, &value))
        {
            if (temperatureMeasurementClusterIsTemperatureValid(value) == true)
            {
                scoped_generic char *temp = stringBuilder("%" PRId16, value);
                initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_TEMPERATURE, temp);
            }
        }
        else
        {
            result = false;
            goto exit;
        }
    }

    icLogDebug(LOG_TAG,
               "Is battery powered=%s, is battery backed up=%s",
               discoveredDeviceDetails->powerSource == powerSourceBattery ? "true" : "false",
               commonDriver->batteryBackedUp ? "true" : "false");

    if (discoveredDeviceDetails->powerSource == powerSourceBattery || commonDriver->batteryBackedUp)
    {
        // battery low
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_LOW))
        {
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_LOW, "false");
        }

        if (icDiscoveredDeviceDetailsGetAttributeEndpoint(
                discoveredDeviceDetails, POWER_CONFIGURATION_CLUSTER_ID, BATTERY_VOLTAGE_ATTRIBUTE_ID, &epid))
        {
            uint8_t value;

            // battery voltage
            if (powerConfigurationClusterGetBatteryVoltage(discoveredDeviceDetails->eui64, epid, &value))
            {
                char tmp[6]; // 25500 + \0 worst case
                snprintf(tmp, 6, "%u", (unsigned int) value * 100);
                initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE, tmp);
            }
        }

        // battery percentage remaining
        if (icDiscoveredDeviceDetailsGetAttributeEndpoint(discoveredDeviceDetails,
                                                          POWER_CONFIGURATION_CLUSTER_ID,
                                                          BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID,
                                                          &epid))
        {
            uint8_t value;

            if (powerConfigurationClusterGetBatteryPercentageRemaining(discoveredDeviceDetails->eui64, epid, &value))
            {
                char tmp[4]; // 100 + \0 worst case
                snprintf(tmp, 4, "%u", (unsigned int) value / 2);
                initialResourceValuesPutDeviceValue(
                    initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING, tmp);
            }
        }
    }

    if (commonDriver->batteryBackedUp)
    {
        // ac mains connected
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED))
        {
            initialResourceValuesPutDeviceValue(
                initialResourceValues, COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED, "false");
        }

        // battery bad
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_BAD))
        {
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_BAD, "false");
        }

        // battery missing
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_MISSING))
        {
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_MISSING, "false");
        }
    }

    if (commonDriver->readInitialBatteryThresholds == true)
    {
        result = readBatteryThresholds(discoveredDeviceDetails, initialResourceValues);
    }

exit:
    return result;
}

static bool fetchInitialResourceValues(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
    IcDiscoveredDeviceDetails *discoveredDeviceDetails =
        hashMapGet(commonDriver->discoveredDeviceDetails, &eui64, sizeof(uint64_t));
    pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

    if (commonDriver->commonCallbacks->fetchInitialResourceValues != NULL)
    {
        result = commonDriver->commonCallbacks->fetchInitialResourceValues(
            ctx, device, discoveredDeviceDetails, initialResourceValues);
    }

    if (result)
    {
        result = fetchCommonZigbeeResourceValues(commonDriver, device, discoveredDeviceDetails, initialResourceValues);
    }
    else
    {
        icLogError(LOG_TAG, "%s: %s driver failed to fetch initial resource values", __FUNCTION__, device->uuid);
    }

    return result;
}

static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
    IcDiscoveredDeviceDetails *discoveredDeviceDetails =
        hashMapGet(commonDriver->discoveredDeviceDetails, &eui64, sizeof(uint64_t));
    pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

    if (commonDriver->commonCallbacks->registerResources != NULL)
    {
        result = commonDriver->commonCallbacks->registerResources(
            ctx, device, discoveredDeviceDetails, initialResourceValues);
    }

    if (result)
    {
        result = registerCommonZigbeeResources(commonDriver, device, discoveredDeviceDetails, initialResourceValues);
    }

    // It is a static resource for this device
    result = createDeviceResource(device,
                                  COMMON_DEVICE_RESOURCE_NETWORK_TYPE,
                                  NETWORK_TYPE_ZIGBEE,
                                  RESOURCE_TYPE_NETWORK_TYPE,
                                  RESOURCE_MODE_READABLE,
                                  CACHING_POLICY_ALWAYS);

    return result;
}

static bool devicePersisted(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // Finish registering the new device
    registerNewDevice(device, commonDriver->deviceCallbacks);

    if (commonDriver->commonCallbacks->devicePersisted != NULL)
    {
        result = commonDriver->commonCallbacks->devicePersisted(ctx, device);
    }

    // update the addresses and flags
    zigbeeSubsystemSetAddresses();

    // start diagnostics monitor task if required
    if (commonDriver->diganosticsCollectionEnabled == true)
    {
        diagnosticsCollectionTaskStart();
    }

    return result;
}

static bool readResource(void *ctx, icDeviceResource *resource, char **value)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = false;

    if (ctx == NULL || resource == NULL || value == NULL)
    {
        return false;
    }

    icLogDebug(LOG_TAG, "readResource %s", resource->id);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (resource->endpointId == NULL)
    {
        if (commonDriver->commonCallbacks->readDeviceResource != NULL)
        {
            return commonDriver->commonCallbacks->readDeviceResource(ctx, resource, value);
        }
    }
    else
    {
        if (commonDriver->commonCallbacks->readEndpointResource != NULL)
        {
            uint8_t epid = getEndpointNumber(ctx, resource->deviceUuid, resource->endpointId);
            return commonDriver->commonCallbacks->readEndpointResource(ctx, epid, resource, value);
        }
    }

    return result;
}

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;
    bool shouldUpdateResource = true;

    if (ctx == NULL || resource == NULL)
    {
        icLogDebug(LOG_TAG, "writeResource: invalid arguments");
        return false;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (resource->endpointId == NULL)
    {
        if (commonDriver->commonCallbacks->writeDeviceResource != NULL)
        {
            result = commonDriver->commonCallbacks->writeDeviceResource(ctx, resource, previousValue, newValue);
        }
    }
    else
    {
        // dont pass resource writes that we manage to the owning driver
        if (strcmp(resource->id, COMMON_ENDPOINT_RESOURCE_LABEL) != 0 &&
            commonDriver->commonCallbacks->writeEndpointResource != NULL)
        {
            uint8_t epid = getEndpointNumber(ctx, resource->deviceUuid, resource->endpointId);
            result = commonDriver->commonCallbacks->writeEndpointResource(
                ctx, epid, resource, previousValue, newValue, &shouldUpdateResource);
        }
    }

    if (result && shouldUpdateResource)
    {
        updateResource(resource->deviceUuid, resource->endpointId, resource->id, newValue, NULL);
    }

    return result;
}

static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response)
{
    icLogDebug(LOG_TAG, "%s", __func__);

    if (ctx == NULL || resource == NULL)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __func__);
        return false;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    bool result = executeCommonResource(ctx, resource, arg, response);
    if (result == false)
    {
        if (resource->endpointId == NULL)
        {
            if (commonDriver->commonCallbacks->executeDeviceResource != NULL)
            {
                result = commonDriver->commonCallbacks->executeDeviceResource(ctx, resource, arg, response);
            }
        }
        else
        {
            if (commonDriver->commonCallbacks->executeEndpointResource != NULL)
            {
                uint8_t epid = getEndpointNumber(ctx, resource->deviceUuid, resource->endpointId);
                result = commonDriver->commonCallbacks->executeEndpointResource(ctx, epid, resource, arg, response);
            }
        }
    }

    return result;
}

/* The function returns true if execute resource request was successfully handled here
   else it returns false */
static bool executeCommonResource(void *ctx, icDeviceResource *resource, const char *arg, char **response)
{
    bool result = false;

    if (ctx == NULL || resource == NULL)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __func__);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: %s", __func__, resource->id);

    // handle common device resources
    if (resource->endpointId == NULL)
    {
        if (stringCompare(COMMON_DEVICE_FUNCTION_RESET_TO_FACTORY, resource->id, false) == 0)
        {
            uint8_t numEndpointIds = 0;
            scoped_generic uint8_t *endpointIds = NULL;
            uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);
            if (zigbeeSubsystemGetEndpointIds(eui64, &endpointIds, &numEndpointIds) == 0)
            {
                if (numEndpointIds > 0)
                {
                    icLogDebug(
                        LOG_TAG, "%s: Triggering Reset to Factory on device Uuid: %s", __func__, resource->deviceUuid);
                    // use the first endpoint ID to trigger factory reset
                    triggerBackgroundResetToFactory(endpointIds[0], eui64);
                    result = true;
                }
                else
                {
                    icLogDebug(LOG_TAG,
                               "%s: The device %s does not have an endpoint. Failed to trigger a factory reset.",
                               __func__,
                               resource->deviceUuid);
                }
            }
            else
            {
                icLogDebug(LOG_TAG, "%s: zigbeeSubsystemGetEndpointIds failed", __func__);
            }
        }
    }
    else
    {
        // handle common endpoint resources
    }

    return result;
}

static void attributeReportReceived(void *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // update ne rssi and lqi
    updateNeRssiAndLqi(ctx, report->eui64, report->rssi, report->lqi);

    // forward to the owning cluster
    ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &report->clusterId, sizeof(report->clusterId));
    if (cluster != NULL)
    {
        if (cluster->handleAttributeReport != NULL)
        {
            cluster->handleAttributeReport(cluster, report);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: no cluster registered to handle the command", __FUNCTION__);
    }

    // always let the actual driver have a crack at it too
    if (commonDriver->commonCallbacks->handleAttributeReport != NULL)
    {
        commonDriver->commonCallbacks->handleAttributeReport(ctx, report);
    }
}

static void clusterCommandReceived(void *ctx, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // update ne rssi and lqi
    updateNeRssiAndLqi(ctx, command->eui64, command->rssi, command->lqi);

    // forward to the owning cluster
    ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &command->clusterId, sizeof(command->clusterId));
    if (cluster != NULL)
    {
        if (cluster->handleClusterCommand != NULL)
        {
            cluster->handleClusterCommand(cluster, command);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: no cluster registered to handle the command", __FUNCTION__);
    }

    // always let the actual driver have a crack at it too
    if (commonDriver->commonCallbacks->handleClusterCommand != NULL)
    {
        commonDriver->commonCallbacks->handleClusterCommand(ctx, command);
    }
}

static bool validateOtaLegacyBootloadUpgradeStartedMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // Legacy Ota Event payload will be empty
    if (bufferLen != 0)
    {
        icLogWarn(LOG_TAG, "%s: payload is not empty!", __func__);
    }
    return true;
}

static bool validateOtaLegacyBootloadUpgradeFailedMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // Legacy Ota Event payload will be empty
    if (bufferLen != 0)
    {
        icLogWarn(LOG_TAG, "%s: payload is not empty!", __func__);
    }
    return true;
}

static bool validateOtaLegacyBootloadUpgradeCompletedMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // Legacy Ota Event payload will be empty
    if (bufferLen != 0)
    {
        icLogWarn(LOG_TAG, "%s: payload is not empty!", __func__);
    }
    return true;
}

static bool validateOtaImageNotifyMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // imageNotify command should consist of atleast 2 bytes
    // payload type (1 byte) + query jitter (1 byte)

    uint16_t minBytes = 2;
    if (bufferLen < minBytes)
    {
        icLogError(
            LOG_TAG, "%s: need atleast %" PRIu16 "  bytes, but got only %" PRIu16, __func__, minBytes, bufferLen);
        return false;
    }

    sbZigbeeIOContext *zio = zigbeeIOInit(buffer, 2, ZIO_READ); // We validate only those 2 bytes

    // Extract the payloadType and queryJitter
    uint8_t payloadType = zigbeeIOGetUint8(zio);
    uint8_t queryJitter = zigbeeIOGetUint8(zio);

    // payload type should be from 0x00 - 0x03
    switch (payloadType)
    {
        case 0x00:
        {
            minBytes += 0; // no extra bytes needed
            break;
        }

        case 0x01:
        {
            minBytes += 2; // 2 bytes extra for mfg code
            break;
        }

        case 0x02:
        {
            minBytes += 4; // 4 bytes extra for mfg code (2) + img type (2)
            break;
        }

        case 0x03:
        {
            minBytes += 8; // 8 bytes extra for mfg code (2) + img type (2) + file version (4)
            break;
        }

        default:
        {
            icLogError(LOG_TAG, "%s: invalid payload type %" PRIu8, __func__, payloadType);
            return false;
        }
    }

    if (bufferLen < minBytes)
    {
        icLogError(
            LOG_TAG, "%s: need atleast %" PRIu16 "  bytes, but got only %" PRIu16, __func__, minBytes, bufferLen);
        return false;
    }

    // query jitter must be an integer from 1-100
    if (queryJitter < 1 || queryJitter > 100)
    {
        icLogError(LOG_TAG, "%s: query jitter must lie between 1-100, but got %" PRIu8, __func__, queryJitter);
        return false;
    }

    return true;
}

static bool validateOtaQueryNextImageRequestMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // query next image request needs atleast 9 bytes
    // Field Control (1) | Mfg Code (2) | Img Type (2) | Firm Ver (4) | Hardware Version (0/2)

    uint16_t minBytes = 9;
    if (bufferLen < minBytes)
    {
        icLogError(
            LOG_TAG, "%s: need atleast %" PRIu16 "  bytes, but got only %" PRIu16, __func__, minBytes, bufferLen);
        return false;
    }
    return true;
}

static bool validateOtaQueryNextImageResponseMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // query next image response needs atleast 1 byte
    // Status (1) | Mfg Code (0/2) | Img Type (0/2) | File Ver (0/4) | Img Size (0/4)

    uint8_t minBytes = 1;
    if (bufferLen < minBytes)
    {
        icLogError(
            LOG_TAG, "%s: need atleast %" PRIu16 "  bytes, but got only %" PRIu16, __func__, minBytes, bufferLen);
        return false;
    }
    return true;
}

static bool validateOtaUpgradeStartedMessage(uint8_t *buffer, uint16_t bufferLen)
{
    if (bufferLen != 0)
    {
        icLogWarn(LOG_TAG, "%s: payload is not empty!", __func__);
    }
    return true;
}

static bool validateOtaUpgradeEndRequestMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // upgrade end request should have 9 bytes
    // Status (1) | Mfg Code (2) | Img Type (2) | File Ver (4)

    uint16_t minBytes = 9;
    if (bufferLen < minBytes)
    {
        icLogError(
            LOG_TAG, "%s: need atleast %" PRIu16 "  bytes, but got only %" PRIu16, __func__, minBytes, bufferLen);
        return false;
    }
    return true;
}

static bool validateOtaUpgradeEndResponseMessage(uint8_t *buffer, uint16_t bufferLen)
{
    // upgrade end response has 16 bytes
    // Mfg Code (2) | Img Type (2) | File Ver (4) | Curr Time (4) | Upgrade Time (4)

    uint16_t minBytes = 16;
    if (bufferLen < minBytes)
    {
        icLogError(
            LOG_TAG, "%s: need atleast %" PRIu16 "  bytes, but got only %" PRIu16, __func__, minBytes, bufferLen);
        return false;
    }
    return true;
}

bool validateOtaUpgradeMessage(OtaUpgradeEvent *otaEvent)
{
    if ((otaEvent == NULL) || (otaEvent->buffer == NULL && otaEvent->bufferLen != 0))
    {
        return false;
    }

    bool isValid = false;

    switch (otaEvent->eventType)
    {
        case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_STARTED_EVENT:
        {
            isValid = validateOtaLegacyBootloadUpgradeStartedMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_FAILED_EVENT:
        {
            isValid = validateOtaLegacyBootloadUpgradeFailedMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_COMPLETED_EVENT:
        {
            isValid = validateOtaLegacyBootloadUpgradeCompletedMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_IMAGE_NOTIFY_EVENT:
        {
            isValid = validateOtaImageNotifyMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT:
        {
            isValid = validateOtaQueryNextImageRequestMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT:
        {
            isValid = validateOtaQueryNextImageResponseMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_UPGRADE_STARTED_EVENT:
        {
            isValid = validateOtaUpgradeStartedMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_UPGRADE_END_REQUEST_EVENT:
        {
            isValid = validateOtaUpgradeEndRequestMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        case ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT:
        {
            isValid = validateOtaUpgradeEndResponseMessage(otaEvent->buffer, otaEvent->bufferLen);
            break;
        }

        default:
        {
            icLogWarn(LOG_TAG, "Unknown eventType: %d", otaEvent->eventType);
            break;
        }
    }

    return isValid;
}

static void otaUpgradeMessageSent(void *ctx, OtaUpgradeEvent *otaEvent)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (validateOtaUpgradeMessage(otaEvent) == false)
    {
        return;
    }

    switch (otaEvent->eventType)
    {
        case ZHAL_OTA_IMAGE_NOTIFY_EVENT:
        {
            otaImageNotifySent(ctx, otaEvent->eui64, otaEvent->timestamp, *otaEvent->sentStatus);
            break;
        }

        case ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT:
        {
            sbZigbeeIOContext *zio =
                zigbeeIOInit(otaEvent->buffer, 1, ZIO_READ); // We only need the first byte for image status

            uint8_t statusCode = zigbeeIOGetUint8(zio);

            otaQueryNextImageResponseSent(ctx, otaEvent->eui64, otaEvent->timestamp, statusCode, *otaEvent->sentStatus);
            break;
        }

        case ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT:
        {
            sbZigbeeIOContext *zio = zigbeeIOInit(otaEvent->buffer, otaEvent->bufferLen, ZIO_READ);

            uint32_t mfgCode = zigbeeIOGetUint32(zio);
            uint32_t newVersion = zigbeeIOGetUint32(zio);
            uint32_t currentTime = zigbeeIOGetUint32(zio);
            uint32_t upgradeTime = zigbeeIOGetUint32(zio);

            otaUpgradeEndResponseSent(ctx,
                                      otaEvent->eui64,
                                      otaEvent->timestamp,
                                      mfgCode,
                                      newVersion,
                                      currentTime,
                                      upgradeTime,
                                      *otaEvent->sentStatus);
            break;
        }

        default:
        {
            // Unexpected event types logged (shouldn't come here!)
            icLogWarn(LOG_TAG, "Unexpected eventType: %d", otaEvent->eventType);
            break;
        }
    }
}

static void otaUpgradeMessageReceived(void *ctx, OtaUpgradeEvent *otaEvent)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (validateOtaUpgradeMessage(otaEvent) == false)
    {
        return;
    }

    switch (otaEvent->eventType)
    {
        case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_STARTED_EVENT:
        {
            otaLegacyBootloadUpgradeStarted(ctx, otaEvent->eui64, otaEvent->timestamp);
            break;
        }

        case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_FAILED_EVENT:
        {
            otaLegacyBootloadUpgradeFailed(ctx, otaEvent->eui64, otaEvent->timestamp);
            break;
        }

        case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_COMPLETED_EVENT:
        {
            otaLegacyBootloadUpgradeCompleted(ctx, otaEvent->eui64, otaEvent->timestamp);
            break;
        }

        case ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT:
        {
            sbZigbeeIOContext *zio = zigbeeIOInit(otaEvent->buffer, otaEvent->bufferLen, ZIO_READ);

            uint8_t fieldControl = zigbeeIOGetUint8(zio);
            uint16_t mfgCode = zigbeeIOGetUint16(zio);
            uint16_t imageType = zigbeeIOGetUint16(zio);
            uint32_t currentVersion = zigbeeIOGetUint32(zio);
            uint16_t hardwareVersion = 0xFFFF; // safe default to 0xFFFF

            if (fieldControl == 0x00 && zigbeeIOGetRemainingSize(zio) >= 2)
            {
                // Parse the remaining 2 bytes for hardware version
                hardwareVersion = zigbeeIOGetUint16(zio);
            }

            otaQueryNextImageRequest(
                ctx, otaEvent->eui64, otaEvent->timestamp, mfgCode, imageType, currentVersion, hardwareVersion);
            break;
        }

        case ZHAL_OTA_UPGRADE_STARTED_EVENT:
        {
            otaUpgradeStarted(ctx, otaEvent->eui64, otaEvent->timestamp);
            break;
        }

        case ZHAL_OTA_UPGRADE_END_REQUEST_EVENT:
        {
            sbZigbeeIOContext *zio = zigbeeIOInit(otaEvent->buffer, otaEvent->bufferLen, ZIO_READ);

            uint8_t upgradeStatus = zigbeeIOGetUint8(zio);
            uint16_t mfgCode = zigbeeIOGetUint16(zio);
            uint16_t imageType = zigbeeIOGetUint16(zio);
            uint32_t nextVersion = zigbeeIOGetUint32(zio);

            otaUpgradeEndRequest(
                ctx, otaEvent->eui64, otaEvent->timestamp, upgradeStatus, mfgCode, imageType, nextVersion);
            break;
        }

        default:
        {
            // Unexpected event types logged (shouldn't come here!)
            icLogWarn(LOG_TAG, "Unexpected eventType: %d", otaEvent->eventType);
            break;
        }
    }
}

static void otaLegacyBootloadUpgradeStarted(void *ctx, uint64_t eui64, uint64_t timestamp)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);

    firmwareUpdateStarted(ctx, eui64);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "LBUStartedDate", dateStr);
}

static void otaLegacyBootloadUpgradeFailed(void *ctx, uint64_t eui64, uint64_t timestamp)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "LBUFailedDate", dateStr);

    firmwareUpdateFailed(ctx, eui64);
}

static void otaLegacyBootloadUpgradeCompleted(void *ctx, uint64_t eui64, uint64_t timestamp)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "LBUCompletedDate", dateStr);

    firmwareUpdateCompleted(ctx, eui64);
}

static void otaImageNotifySent(void *ctx, uint64_t eui64, uint64_t timestamp, ZHAL_STATUS sentStatus)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);
    scoped_generic const char *sentStatusStr = stringBuilder("%" PRIzHAL, sentStatus);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "INSentDate", dateStr);
    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "INSentStatus", sentStatusStr);
}

static void otaQueryNextImageRequest(void *ctx,
                                     uint64_t eui64,
                                     uint64_t timestamp,
                                     uint16_t mfgCode,
                                     uint16_t imageType,
                                     uint32_t currentVersion,
                                     uint16_t hardwareVersion)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    scoped_generic char *fw = getZigbeeVersionString(currentVersion);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "QNIRequestDate", dateStr);

    // Read the currently known firmware version and compare with what we just got sent to determine if a firmware
    //  update just completed.
    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *fwRes = deviceServiceGetResourceById(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if (fwRes != NULL)
    {
        if (strcmp(fw, fwRes->value) != 0)
        {
            firmwareUpdateCompleted(ctx, eui64);
        }

        updateResource(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, fw, NULL);
    }

    // Go through and remove and unused firmware files now that something has upgraded
    // This will do an overall scan, which is more work than is needed, but this event
    // is rare so this overhead is minimal
    zigbeeSubsystemCleanupFirmwareFiles();
}

static void
otaQueryNextImageResponseSent(void *ctx, uint64_t eui64, uint64_t timestamp, uint8_t statusCode, ZHAL_STATUS sentStatus)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);
    scoped_generic const char *statusCodeStr = stringBuilder("%" PRIu8, statusCode);
    scoped_generic const char *sentStatusStr = stringBuilder("%" PRIzHAL, sentStatus);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "QNIResponseSentDate", dateStr);
    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "QNIResponseImageStatus", statusCodeStr);
    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "QNIResponseSentStatus", sentStatusStr);
}

static void otaUpgradeStarted(void *ctx, uint64_t eui64, uint64_t timestamp)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "UpgradeStartedDate", dateStr);

    firmwareUpdateStarted(ctx, eui64);
}

static void otaUpgradeEndRequest(void *ctx,
                                 uint64_t eui64,
                                 uint64_t timestamp,
                                 uint8_t upgradeStatus,
                                 uint16_t mfgCode,
                                 uint16_t imageType,
                                 uint32_t nextVersion)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);
    scoped_generic const char *upgradeStatusString = stringBuilder("%" PRIu8, upgradeStatus);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "UERequestDate", dateStr);
    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "UERequestStatus", upgradeStatusString);

    if (upgradeStatus == ZCL_STATUS_SUCCESS)
    {
        // Upgrade Completed
        firmwareUpdateCompleted(ctx, eui64);
    }
    else
    {
        // Upgrade Failed
        firmwareUpdateFailed(ctx, eui64);
    }
}

static void otaUpgradeEndResponseSent(void *ctx,
                                      uint64_t eui64,
                                      uint64_t timestamp,
                                      uint32_t mfgCode,
                                      uint32_t newVersion,
                                      uint32_t currentTime,
                                      uint32_t upgradeTime,
                                      ZHAL_STATUS sentStatus)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    scoped_generic const char *dateStr = stringBuilder("%" PRIu64, timestamp);
    scoped_generic const char *sentStatusStr = stringBuilder("%" PRIzHAL, sentStatus);

    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "UEResponseSentDate", dateStr);
    updateJsonMetadata(deviceUuid, NULL, OTA_UPGRADE_INFO_METADATA_NAME, "UEResponseSentStatus", sentStatusStr);
}

static void firmwareUpdateStarted(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    TELEMETRY_COUNTER(TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_START);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    updateResource(
        deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, FIRMWARE_UPDATE_STATUS_STARTED, NULL);

    // Cleanup
    free(deviceUuid);
}

static void firmwareUpdateCompleted(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    TELEMETRY_COUNTER(TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_SUCCESS);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    updateResource(
        deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, FIRMWARE_UPDATE_STATUS_COMPLETED, NULL);

    if (deviceServiceGetPostUpgradeAction(deviceUuid) == POST_UPGRADE_ACTION_RECONFIGURE)
    {
        deviceServiceReconfigureDevice(deviceUuid, RECONFIGURATION_DELAY_SECS, NULL, false);
    }

    // Cleanup
    free(deviceUuid);
}

static void firmwareUpdateFailed(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    TELEMETRY_COUNTER(TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_FAILED);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // Forward to subscribing drivers
    if (commonDriver->commonCallbacks->firmwareUpgradeFailed != NULL)
    {
        commonDriver->commonCallbacks->firmwareUpgradeFailed(commonDriver, eui64);
    }

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    updateResource(
        deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, FIRMWARE_UPDATE_STATUS_FAILED, NULL);

    // Cleanup
    free(deviceUuid);
}

static void deviceRejoined(void *ctx, uint64_t eui64, bool isSecure)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: driver %s", __FUNCTION__, commonDriver->baseDriver.driverName);

    if (commonDriver->commonCallbacks->deviceRejoined != NULL)
    {
        IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
        commonDriver->commonCallbacks->deviceRejoined(ctx, eui64, isSecure, details);
    }

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    if (deviceServiceIsReconfigurationPending(deviceUuid) && deviceServiceIsReconfigurationAllowedAsap(deviceUuid))
    {
        IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
        uint8_t endpointId = 0;

        if (icDiscoveredDeviceDetailsGetClusterEndpoint(details, POLL_CONTROL_CLUSTER_ID, &endpointId))
        {
            if (pollControlClusterSendCheckInResponse(eui64, endpointId, true))
            {
                deviceServiceSendReconfigurationSignal(deviceUuid, false);
            }
            else
            {
                icError("driver %s failed to enter fast poll on rejoin. reconfiguration is still pending !",
                        commonDriver->baseDriver.driverName);
            }
        }
        else
        {
            deviceServiceSendReconfigurationSignal(deviceUuid, true);
            icError("driver %s failed to get poll control cluster endpoint. cancelling reconfiguration !",
                    commonDriver->baseDriver.driverName);
        }
    }
}

static void deviceLeft(void *ctx, uint64_t eui64)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: driver %s", __FUNCTION__, commonDriver->baseDriver.driverName);

    if (commonDriver->commonCallbacks->deviceLeft != NULL)
    {
        IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
        commonDriver->commonCallbacks->deviceLeft(ctx, eui64, details);
    }
}

static void deviceAnnounced(void *ctx, uint64_t eui64, zhalDeviceType deviceType, zhalPowerSource powerSource)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // For initial migration there was a bug where we didn't get the deviceType or powerSource populated, this ensures
    // we update with correct values since these are used particular during device reconfiguration to determine whether
    // to create some resources
    IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
    if (details != NULL && (details->deviceType != deviceType || details->powerSource != powerSource))
    {
        details->deviceType = deviceType;
        details->powerSource = powerSource;

        cJSON *detailsJson = icDiscoveredDeviceDetailsToJson(details);
        char *detailsStr = cJSON_PrintUnformatted(detailsJson);
        cJSON_Delete(detailsJson);

        char *uuid = zigbeeSubsystemEui64ToId(eui64);
        icLogDebug(LOG_TAG, "%s: Updating deviceType/powerSource for device details on device %s", __FUNCTION__, uuid);
        setMetadata(uuid, NULL, DISCOVERED_DEVICE_DETAILS, detailsStr);
        free(uuid);
        free(detailsStr);
    }
}

// Called by zigbeeSubsystem.
// return true if we own this device.  The provided details are fully filled out with the exception
// of information about attributes since that discovery takes a while to perform and we are trying
// to get device discovery events out as quickly as possible.
static bool deviceDiscoveredCallback(void *ctx, IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator)
{
    bool result = false;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // silently ignore if this driver instance is not discovering and we aren't migrating
    if (commonDriver->discoveryActive == false && deviceMigrator == NULL)
    {
        return false;
    }

    icLogDebug(LOG_TAG, "%s: driver %s", __FUNCTION__, commonDriver->baseDriver.driverName);

    if (commonDriver->commonCallbacks->claimDevice != NULL)
    {
        result = commonDriver->commonCallbacks->claimDevice(ctx, details);
    }

    // we may have had a customClaimCallback registered, but want to let regular deviceid matching work
    if (!result)
    {
        if (details->numEndpoints == 0)
        {
            return false;
        }

        // we expect the first endpoint to match one of our device ids
        for (int i = 0; i < commonDriver->numDeviceIds; i++)
        {
            if (details->endpointDetails[0].appDeviceId == commonDriver->deviceIds[i])
            {
                result = true;
                break;
            }
        }

        if (!result)
        {
            icLogDebug(LOG_TAG,
                       "%s: deviceId %04x does not match any of our (%s) device ids",
                       __FUNCTION__,
                       details->endpointDetails[0].appDeviceId,
                       commonDriver->baseDriver.driverName);
        }
    }

    if (result)
    {
        // save off a copy of the discovered device details for use later if the device is kept.  Delete any previous
        //  entry
        pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
        IcDiscoveredDeviceDetails *detailsCopy = cloneIcDiscoveredDeviceDetails(details);
        hashMapDelete(
            commonDriver->discoveredDeviceDetails, &details->eui64, sizeof(uint64_t), discoveredDeviceDetailsFreeFunc);
        hashMapPut(commonDriver->discoveredDeviceDetails, &detailsCopy->eui64, sizeof(uint64_t), detailsCopy);
        pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

        // if we got here either the higher level device driver claimed it or it matched one of the device ids
        char *uuid = zigbeeSubsystemEui64ToId(details->eui64);

        char hw[21]; // max uint64_t plus null
        // Convert to decimal string, as that's what we expect everywhere
        sprintf(hw, "%" PRIu64, details->hardwareVersion);

        char *fw = getZigbeeVersionString((uint32_t) (details->firmwareVersion));

        icStringHashMap *metadata = NULL;

        if (commonDriver->commonCallbacks->getDiscoveredDeviceMetadata != NULL)
        {
            metadata = stringHashMapCreate();
            commonDriver->commonCallbacks->getDiscoveredDeviceMetadata(ctx, details, metadata);
        }

        // Provide some more information in the form of a mapping of endpoint id to its profile
        icStringHashMap *endpointProfileMap = stringHashMapCreate();
        if (commonDriver->commonCallbacks->mapDeviceIdToProfile != NULL)
        {
            for (int i = 0; i < details->numEndpoints; ++i)
            {
                const char *profile =
                    commonDriver->commonCallbacks->mapDeviceIdToProfile(ctx, details->endpointDetails[i].appDeviceId);
                if (profile != NULL)
                {
                    char *key = stringBuilder("%d", details->endpointDetails[i].endpointId);
                    stringHashMapPut(endpointProfileMap, key, strdup(profile));
                }
            }
        }

        DeviceFoundDetails deviceFoundDetails = {.deviceDriver = (DeviceDriver *) commonDriver,
                                                 .deviceMigrator = deviceMigrator,
                                                 .subsystem = ZIGBEE_SUBSYSTEM_NAME,
                                                 .deviceClass = commonDriver->deviceClass,
                                                 .deviceClassVersion = commonDriver->deviceClassVersion,
                                                 .deviceUuid = uuid,
                                                 .manufacturer = details->manufacturer,
                                                 .model = details->model,
                                                 .hardwareVersion = hw,
                                                 .firmwareVersion = fw,
                                                 .metadata = metadata,
                                                 .endpointProfileMap = endpointProfileMap};

        if (deviceServiceDeviceFound(&deviceFoundDetails, commonDriver->baseDriver.neverReject) == false)
        {
            // device service did not like something about this device and it was not successfully
            //  added.  We cannot keep anything about it around, so clean up, reset it, and tell it
            //  to leave
            //
            //  Note: This log line is used for telemetry, please DO NOT modify or remove it
            //
            icLogWarn(LOG_TAG,
                      "%s: device service rejected the device of type %s and id %s",
                      __FUNCTION__,
                      commonDriver->deviceClass,
                      uuid);

            // If the device driver needs to handle the device getting rejected after claiming
            if (commonDriver->commonCallbacks->deviceRejected != NULL)
            {
                commonDriver->commonCallbacks->deviceRejected(ctx, details);
            }

            pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
            hashMapDelete(commonDriver->discoveredDeviceDetails,
                          &details->eui64,
                          sizeof(uint64_t),
                          discoveredDeviceDetailsFreeFunc);
            pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

            // Don't reset if we failed migration
            if (deviceMigrator == NULL)
            {
                // reset and kick it out in the background so we dont block.  We just send to the
                // first endpoint since its a global operation on the device
                triggerBackgroundResetToFactory(details->endpointDetails[0].endpointId, details->eui64);
            }

            result = false;
        }

        // Cleanup endpointProfileMap
        stringHashMapDestroy(endpointProfileMap, NULL);

        // Cleanup metadata
        stringHashMapDestroy(metadata, NULL);
        free(uuid);
        free(fw);
    }

    return result;
}

static void synchronizeDevice(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->synchronizeDevice != NULL)
    {
        uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
        IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
        commonDriver->commonCallbacks->synchronizeDevice(ctx, device, details);
    }
}

static void endpointDisabled(void *ctx, icDeviceEndpoint *endpoint)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s, endpointId=%s", __FUNCTION__, endpoint->deviceUuid, endpoint->id);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->endpointDisabled != NULL)
    {
        commonDriver->commonCallbacks->endpointDisabled(ctx, endpoint);
    }
}

static void handleAlarmCommand(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // forward to the owning cluster
    ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, (void *) &entry->clusterId, sizeof(entry->clusterId));
    if (cluster != NULL)
    {
        if (cluster->handleAlarm != NULL)
        {
            cluster->handleAlarm(cluster, eui64, endpointId, entry);
        }
    }
    else
    {
        icLogError(LOG_TAG,
                   "%s: no cluster registered to handle the command: cluster 0x%.2" PRIx16 " ep %" PRIu8
                   " alarmCode 0x%.2" PRIx8,
                   __FUNCTION__,
                   entry->clusterId,
                   endpointId,
                   entry->alarmCode);
    }

    if (commonDriver->commonCallbacks->handleAlarm != NULL)
    {
        commonDriver->commonCallbacks->handleAlarm(commonDriver, eui64, endpointId, entry);
    }
}

static void
handleAlarmClearedCommand(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // forward to the owning cluster
    ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, (void *) &entry->clusterId, sizeof(entry->clusterId));
    if (cluster != NULL)
    {
        if (cluster->handleAlarmCleared != NULL)
        {
            cluster->handleAlarmCleared(cluster, eui64, endpointId, entry);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: no cluster registered to handle the command", __FUNCTION__);
    }

    if (commonDriver->commonCallbacks->handleAlarmCleared != NULL)
    {
        commonDriver->commonCallbacks->handleAlarmCleared(commonDriver, eui64, endpointId, entry);
    }
}

/*
 * Updates a JSON metadata entry with the key-value pair.
 * A new JSON object will be created if the entry doesn't exist.
 */
static void
updateJsonMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *key, const char *value)
{
    // Place zigbee driver JSON metadata updates under a mutex
    // to avoid interleaving gets and sets across threads
    LOCK_SCOPE(deviceOtaUpgradeEventMtx);

    cJSON *valueJson = cJSON_CreateString(value); // will get freed from jsonMetadata, so don't use scoped_cJSON!

    scoped_cJSON *jsonMetadata = getJsonMetadata(deviceUuid, endpointId, name);
    if (jsonMetadata == NULL)
    {
        // Create a new JSON object and add directly
        jsonMetadata = cJSON_CreateObject();
        cJSON_AddItemToObject(jsonMetadata, key, valueJson);
    }
    else
    {
        // try to update greedily and if that fails, insert
        if (cJSON_ReplaceItemInObject(jsonMetadata, key, valueJson) == false)
        {
            cJSON_AddItemToObject(jsonMetadata, key, valueJson);
        }
    }

    setJsonMetadata(deviceUuid, endpointId, name, jsonMetadata);
}

static IcDiscoveredDeviceDetails *getDiscoveredDeviceDetails(uint64_t eui64, ZigbeeDriverCommon *commonDriver)
{
    IcDiscoveredDeviceDetails *result = NULL;

    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    // try to get the details from our cached map.  If it isnt in there, load from metadata JSON and cache for next time
    pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
    result = hashMapGet(commonDriver->discoveredDeviceDetails, &eui64, sizeof(uint64_t));

    if (result == NULL)
    {
        char *detailsStr = getMetadata(uuid, NULL, DISCOVERED_DEVICE_DETAILS);

        if (detailsStr == NULL)
        {
            icLogError(LOG_TAG, "%s: missing %s metadata!", __FUNCTION__, DISCOVERED_DEVICE_DETAILS);
            pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);
            free(uuid);
            return NULL;
        }

        cJSON *detailsJson = cJSON_Parse(detailsStr);
        result = icDiscoveredDeviceDetailsFromJson(detailsJson);
        cJSON_Delete(detailsJson);
        free(detailsStr);

        if (result == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to parse %s metadata!", __FUNCTION__, DISCOVERED_DEVICE_DETAILS);
            pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);
            free(uuid);
            return NULL;
        }

        // cache it
        if (hashMapPut(commonDriver->discoveredDeviceDetails, &result->eui64, sizeof(uint64_t), result) == false)
        {
            icLogWarn(LOG_TAG, "%s: Failed to cache discovered device details for %s", __func__, uuid);
            freeIcDiscoveredDeviceDetails(result);
            result = NULL;
        }
    }
    pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

    free(uuid);

    return result;
}

static bool resourceNeedsRefreshing(const char *deviceUuid,
                                    const char *resourceId,
                                    const char *metadataPropName,
                                    uint32_t defaultRefreshIntervalSecs)
{
    bool result = true; // default is to go ahead and refresh

    uint64_t resourceAgeMillis;
    if (deviceServiceGetResourceAgeMillis(deviceUuid, NULL, resourceId, &resourceAgeMillis) == true)
    {
        // if there is a metadata entry for this interval, use that.  Otherwise fall back to default.
        uint32_t refreshIntervalSecs = defaultRefreshIntervalSecs;

        AUTO_CLEAN(free_generic__auto) char *uri = metadataUriCreate(deviceUuid, NULL, metadataPropName);

        char *value = NULL;
        if (deviceServiceGetMetadata(uri, &value) == true && value != NULL)
        {
            uint32_t temp = 0;
            if (stringToUint32(value, &temp) == true)
            {
                refreshIntervalSecs = temp;
            }
        }
        free(value);

        // if the resource is not old enough, lets not do anything now
        if (resourceAgeMillis < ((uint64_t) refreshIntervalSecs * 1000))
        {
            icLogDebug(LOG_TAG, "%s: resource %s does not need refreshing yet", __func__, resourceId);
            result = false;
        }
    }

    return result;
}

/*
 * Caller must NOT destroy the contents of the returned list
 */
static icLinkedList *
getClustersNeedingPollControlRefresh(uint64_t eui64, uint8_t endpointId, const ZigbeeDriverCommon *commonDriver)
{
    icLinkedList *result = linkedListCreate();

    /*
     * Currently only Temperature Measurement, Diagnostics, Power Configuration, and OTA clusters do anything
     * during poll control checkin.  Given that these are all common clusters controlled by this common driver,
     * we handle the checkin here by only including these clusters in the checkin processing if our logic
     * determines there is something to be done.  This allows us to let the device go right back to sleep
     * when it does a checkin if nothing needs to be done.
     *
     * In the future we may want lower level drivers and/or other clusters to be able to do their processing
     * during checkin.  If/when that time comes, we will likely need to extend this and check some new callback
     * on the lower level device driver or cluster.
     */

    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    if (resourceNeedsRefreshing(deviceUuid,
                                COMMON_DEVICE_RESOURCE_TEMPERATURE,
                                TEMP_REFRESH_MIN_SECS_PROP,
                                DEFAULT_TEMP_REFRESH_MIN_SECONDS) == true)
    {
        // add the temperature measurement cluster to our result set
        uint16_t clusterId = TEMPERATURE_MEASUREMENT_CLUSTER_ID;
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &clusterId, sizeof(clusterId));
        if (cluster != NULL)
        {
            icLogDebug(LOG_TAG, "%s: going to refresh temperature measurement", __func__);
            linkedListAppend(result, cluster);
        }
    }

    if (resourceNeedsRefreshing(deviceUuid,
                                COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE,
                                BATTERY_VOLTAGE_REFRESH_MIN_SECS_PROP,
                                DEFAULT_BATTERY_VOLTAGE_REFRESH_MIN_SECONDS) == true)
    {
        // add the power configuration cluster to our result set
        uint16_t clusterId = POWER_CONFIGURATION_CLUSTER_ID;
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &clusterId, sizeof(clusterId));
        if (cluster != NULL)
        {
            icLogDebug(LOG_TAG, "%s: going to refresh power configuration", __func__);
            linkedListAppend(result, cluster);
        }
    }

    // if either far end rssi or lqi need updating, add the diagnostics cluster
    if (resourceNeedsRefreshing(
            deviceUuid, COMMON_DEVICE_RESOURCE_FERSSI, RSSI_REFRESH_MIN_SECS_PROP, DEFAULT_RSSI_REFRESH_MIN_SECONDS) ==
            true ||
        resourceNeedsRefreshing(
            deviceUuid, COMMON_DEVICE_RESOURCE_FELQI, LQI_REFRESH_MIN_SECS_PROP, DEFAULT_LQI_REFRESH_MIN_SECONDS) ==
            true)
    {
        uint16_t clusterId = DIAGNOSTICS_CLUSTER_ID;
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &clusterId, sizeof(clusterId));
        if (cluster != NULL)
        {
            icLogDebug(LOG_TAG, "%s: going to refresh diagnostics", __func__);
            linkedListAppend(result, cluster);
        }
    }

    return result;
}

static void handlePollControlCheckin(uint64_t eui64,
                                     uint8_t endpointId,
                                     const ComcastBatterySavingData *batterySavingData,
                                     const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;
    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    // check if this device needs reconfiguration
    if (deviceServiceIsReconfigurationPending(deviceUuid))
    {
        // put the device in fast poll mode for reconfiguration
        if (pollControlClusterSendCheckInResponse(eui64, endpointId, true))
        {
            // we can signal the waiting reconfigure task to proceed and
            // skip the usual checkin stuff as it can be done in the
            // next checkin once reconfiguration is complete
            deviceServiceSendReconfigurationSignal(deviceUuid, false);
        }
        else
        {
            icError("failed to enter fast poll. reconfiguration is still pending !");
        }
    }
    else
    {
        // TODO: allow each cluster to perform some action on poll control check-in.
        if (batterySavingData != NULL)
        {
            zigbeeDriverCommonComcastBatterySavingUpdateResources(eui64, batterySavingData, commonDriver);

            if (!pollControlClusterSendCustomCheckInResponse(eui64, endpointId))
            {
                icError("failed to send custom poll control checkin response!");
            }
        }
        else
        {
            // first check to see if any cluster had any work to do during this checkin.  If not, no need to start fast
            //  polling
            icLinkedList *clustersNeedingPollControlRefresh =
                getClustersNeedingPollControlRefresh(eui64, endpointId, commonDriver);

            if (clustersNeedingPollControlRefresh != NULL && linkedListCount(clustersNeedingPollControlRefresh) > 0)
            {
                // request fast poll while we do the refresh
                if (pollControlClusterSendCheckInResponse(eui64, endpointId, true))
                {
                    // Allow each cluster that needs refresh to run
                    sbIcLinkedListIterator *it = linkedListIteratorCreate(clustersNeedingPollControlRefresh);
                    while (linkedListIteratorHasNext(it) == true)
                    {
                        ZigbeeCluster *cluster = linkedListIteratorGetNext(it);

                        if (cluster->handlePollControlCheckin != NULL)
                        {
                            icLogDebug(LOG_TAG,
                                       "%s: notifying cluster 0x%04x that it can do poll control checkin work",
                                       __FUNCTION__,
                                       cluster->clusterId);

                            cluster->handlePollControlCheckin(cluster, eui64, endpointId);
                        }
                    }
                }
                else
                {
                    icError("failed to enter fast poll!");
                }

                // Stop the fast polling
                pollControlClusterStopFastPoll(eui64, endpointId);
            }
            else
            {
                // no work to do. send checkin response indicating no fast polling
                icLogDebug(LOG_TAG, "%s: no work to do, not fast polling", __func__);
                pollControlClusterSendCheckInResponse(eui64, endpointId, false);
            }

            // dont destroy the clusters in the list
            linkedListDestroy(clustersNeedingPollControlRefresh, standardDoNotFreeFunc);
        }
    }
}


void zigbeeDriverCommonComcastBatterySavingUpdateResources(uint64_t eui64,
                                                           const ComcastBatterySavingData *data,
                                                           const void *ctx)
{
    AUTO_CLEAN(free_generic__auto) const char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    char valStr[11];

    // Battery Voltage
    snprintf(valStr, 6, "%u", data->battVoltage); // 65535 + \0 worst case
    updateResource(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE, valStr, NULL);

    // FE RSSI
    snprintf(valStr, 5, "%" PRIi8, data->rssi); //-127 + \0 worst case
    updateResource(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FERSSI, valStr, NULL);

    // FE LQI
    snprintf(valStr, 4, "%u", data->lqi); // 255 + \0 worst case
    updateResource(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_FELQI, valStr, NULL);

    // Sensor Temp
    if (temperatureMeasurementClusterIsTemperatureValid(data->temp) == true)
    {
        scoped_generic char *temp = stringBuilder("%" PRId16, data->temp);
        updateResource(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_TEMPERATURE, temp, NULL);
    }

    // Optional data is defined through metadata of a device and only common definition(s) across all/multiple device
    // classes are handled below. Optional data related to particular device class is processed by its device driver.
    // Following battCapUsed can be optional data for security devices(sensor, keypad, etc), so it is handled below.
    scoped_generic char *optionalDataType = getMetadata(deviceUuid, NULL, OPTIONAL_SENSOR_DATA_TYPE_METADATA);

    if (optionalDataType == NULL ||
        stringCompare(optionalDataType, OPTIONAL_SENSOR_DATA_TYPE_BATTERY_CAP_USED, false) == 0)
    {
        // Battery used mAH
        scoped_generic char *battUsedMilliAmpHr = NULL;
        battUsedMilliAmpHr = stringBuilder("%u", data->optionalData.battUsedMilliAmpHr); // 65535 + \0 worst case
        setMetadata(deviceUuid, NULL, COMMON_DEVICE_METADATA_BATTERY_USED_MAH, battUsedMilliAmpHr);
    }

    // Rejoins
    snprintf(valStr, 11, "%u", data->rejoins); // 4294967295 + \0 worst case
    setMetadata(deviceUuid, NULL, COMMON_DEVICE_METADATA_REJOINS, valStr);

    // Retries
    snprintf(valStr, 11, "%u", data->retries); // 4294967295 + \0 worst case
    setMetadata(deviceUuid, NULL, COMMON_DEVICE_METADATA_RETRIES, valStr);
}

char *getZigbeeVersionString(uint32_t version)
{
    char *result = (char *) malloc(11); // max 32 bit hex string plus 0x and null
    snprintf(result, 11, FIRMWARE_FORMAT_STRING, version);
    return result;
}

uint32_t getZigbeeVersionFromString(const char *version)
{
    uint32_t result = 0;
    stringToUint32(version, &result);
    return result;
}

void *zigbeeDriverCommonGetDriverPrivateData(ZigbeeDriverCommon *ctx)
{
    return ctx->driverPrivate;
}

void zigbeeDriverCommonSetDriverPrivateData(ZigbeeDriverCommon *ctx, void *privateData)
{
    ctx->driverPrivate = privateData;
}

void zigbeeDriverCommonSetBatteryBackedUp(DeviceDriver *driver)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    commonDriver->batteryBackedUp = true;
}

static void discoveredDeviceDetailsFreeFunc(void *key, void *value)
{
    (void) key;
    freeIcDiscoveredDeviceDetails((IcDiscoveredDeviceDetails *) value);
}

static void *resetToFactoryTask(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    resetToFactoryData *resetData = (resetToFactoryData *) arg;

    // coverity[check_return] device may be sleepy and it is okay if this is not delivered.
    zigbeeSubsystemSendCommand(
        resetData->eui64, resetData->epid, BASIC_CLUSTER_ID, true, BASIC_RESET_TO_FACTORY_COMMAND_ID, NULL, 0);

    zhalRequestLeave(resetData->eui64, false, false);

    // Cleanup
    free(resetData);

    return NULL;
}

static bool findDeviceResource(void *searchVal, void *item)
{
    icDeviceResource *resourceItem = (icDeviceResource *) item;
    return strcmp(searchVal, resourceItem->id) == 0;
}

static void updateLinkQuality(ZigbeeDriverCommon *commonDriver, const char *deviceUuid)
{
    const char *linkQuality = LINK_QUALITY_UNKNOWN;
    icDevice *device = deviceServiceGetDevice(deviceUuid);
    int8_t neRssi = INT8_MIN;
    int8_t feRssi = INT8_MIN;
    uint8_t neLqi = 0;
    uint8_t feLqi = 0;
    bool inCommFail = false;

    icLinkedListIterator *iter = linkedListIteratorCreate(device->resources);
    while (linkedListIteratorHasNext(iter) == true)
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iter);
        if (stringCompare(resource->id, COMMON_DEVICE_RESOURCE_NERSSI, false) == 0)
        {
            stringToInt8(resource->value, &neRssi);
        }
        else if (stringCompare(resource->id, COMMON_DEVICE_RESOURCE_FERSSI, false) == 0)
        {
            stringToInt8(resource->value, &feRssi);
        }
        else if (stringCompare(resource->id, COMMON_DEVICE_RESOURCE_NELQI, false) == 0)
        {
            stringToUint8(resource->value, &neLqi);
        }
        else if (stringCompare(resource->id, COMMON_DEVICE_RESOURCE_FELQI, false) == 0)
        {
            stringToUint8(resource->value, &feLqi);
        }
        else if (stringCompare(resource->id, COMMON_DEVICE_RESOURCE_COMM_FAIL, false) == 0)
        {
            inCommFail = stringToBool(resource->value);
        }
    }
    linkedListIteratorDestroy(iter);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(deviceUuid);

    ZigbeeSubsystemLinkQualityLevel prevLinkQuality = getPrevLinkQuality(eui64);
    if ((neRssi > INT8_MIN || feRssi > INT8_MIN) && (neLqi > 0 || feLqi > 0))
    {
        linkQuality = zigbeeSubsystemDetermineLinkQuality(neRssi, feRssi, neLqi, feLqi, prevLinkQuality);
    }
    setCurrentLinkQuality(eui64, linkQuality);

    cJSON *linkQualityDetails = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(linkQualityDetails, COMMON_DEVICE_RESOURCE_NERSSI, cJSON_CreateNumber(neRssi));
    cJSON_AddItemToObjectCS(linkQualityDetails, COMMON_DEVICE_RESOURCE_FERSSI, cJSON_CreateNumber(feRssi));
    cJSON_AddItemToObjectCS(linkQualityDetails, COMMON_DEVICE_RESOURCE_NELQI, cJSON_CreateNumber(neLqi));
    cJSON_AddItemToObjectCS(linkQualityDetails, COMMON_DEVICE_RESOURCE_FELQI, cJSON_CreateNumber(feLqi));
    cJSON_AddItemToObjectCS(linkQualityDetails, COMMON_DEVICE_RESOURCE_COMM_FAIL, cJSON_CreateBool(inCommFail));

    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_LINK_QUALITY, linkQuality, linkQualityDetails);

    cJSON_Delete(linkQualityDetails);
    deviceDestroy(device);
}

/*
 * update the Last linkQuality value with the current linkQuality value
 * @Param eui64 device id
 * @Param linkQuality device current linkQuality
 */
static void setCurrentLinkQuality(uint64_t eui64, const char *linkQuality)
{

    // converts the string formatted linkQuality to enum
    ZigbeeSubsystemLinkQualityLevel qualLevel = zigbeeSubsystemLinkQualityStringToEnum(linkQuality);

    mutexLock(&linkQualityMutex);
    if (devicesLinkQualityMap == NULL)
    {
        devicesLinkQualityMap = hashMapCreate();
    }

    ZigbeeSubsystemLinkQualityLevel *quality = hashMapGet(devicesLinkQualityMap, &eui64, sizeof(uint64_t));
    if (quality == NULL)
    {
        uint64_t *key = (uint64_t *) malloc(sizeof(uint64_t));
        *key = eui64;
        quality = (ZigbeeSubsystemLinkQualityLevel *) malloc(sizeof(ZigbeeSubsystemLinkQualityLevel));
        *quality = qualLevel;
        if (hashMapPut(devicesLinkQualityMap, key, sizeof(uint64_t), quality) == false)
        {
            free(key);
            key = NULL;
            free(quality);
            quality = NULL;
        }
    }
    else
    {
        *quality = qualLevel;
    }
    mutexUnlock(&linkQualityMutex);
}

/*
 * get the previous linkQuality value of the given eui64
 * @Param eui64 device uid
 * return linkQuality enum value
 */
static ZigbeeSubsystemLinkQualityLevel getPrevLinkQuality(uint64_t eui64)
{
    ZigbeeSubsystemLinkQualityLevel result = LINK_QUALITY_LEVEL_UNKNOWN;

    LOCK_SCOPE(linkQualityMutex);

    if (devicesLinkQualityMap != NULL)
    {
        ZigbeeSubsystemLinkQualityLevel *linkQuality = hashMapGet(devicesLinkQualityMap, &eui64, sizeof(uint64_t));
        if (linkQuality != NULL)
        {
            result = *linkQuality;
        }
    }

    return result;
}

static void updateNeRssiAndLqi(void *ctx, uint64_t eui64, int8_t rssi, uint8_t lqi)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    char rssiStr[5]; //-127 + \0
    snprintf(rssiStr, 5, "%d", rssi);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_NERSSI, rssiStr, NULL);

    char lqiStr[4]; // 255 + \0
    snprintf(lqiStr, 4, "%u", lqi);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_NELQI, lqiStr, NULL);

    updateLinkQuality(commonDriver, deviceUuid);

    free(deviceUuid);
}

static void handleRssiLqiUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int8_t rssi, uint8_t lqi)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    char rssiStr[5]; //-127 + \0
    snprintf(rssiStr, 5, "%d", rssi);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_FERSSI, rssiStr, NULL);

    char lqiStr[4]; // 255 + \0
    snprintf(lqiStr, 4, "%u", lqi);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_FELQI, lqiStr, NULL);

    updateLinkQuality(commonDriver, deviceUuid);

    free(deviceUuid);
}

static void
handleTemperatureMeasurementMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int16_t value)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    char tmp[7]; //-32767 + \0 worst case
    snprintf(tmp, 7, "%" PRId16, value);

    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_TEMPERATURE, tmp, NULL);
    free(deviceUuid);
}

static void handleBatteryVoltageUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t decivolts)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    char tmp[6]; // 25500 + \0 worst case
    snprintf(tmp, 6, "%u", (unsigned int) decivolts * 100);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE, tmp, NULL);
    free(deviceUuid);
}

static void
handleBatteryPercentageRemainingUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t halfIntPercent)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    char tmp[4]; // 100 + \0 worst case
    snprintf(tmp, 4, "%u", (unsigned int) halfIntPercent / 2);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING, tmp, NULL);
    free(deviceUuid);
}

void zigbeeDriverCommonUpdateBatteryChargeStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryLow)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_BATTERY_LOW, isBatteryLow ? "true" : "false", NULL);
    free(deviceUuid);
}

static void handleBatteryChargeStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryLow)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateBatteryChargeStatus(commonDriver, eui64, isBatteryLow);
}

void zigbeeDriverCommonUpdateBatteryBadStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryBad)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_BATTERY_BAD, isBatteryBad ? "true" : "false", NULL);
    free(deviceUuid);
}

static void handleBatteryBadStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryBad)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateBatteryBadStatus(commonDriver, eui64, isBatteryBad);
}

void zigbeeDriverCommonUpdateBatteryMissingStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryMissing)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_BATTERY_MISSING, isBatteryMissing ? "true" : "false", NULL);
    free(deviceUuid);
}

static void handleBatteryMissingStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryMissing)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateBatteryMissingStatus(commonDriver, eui64, isBatteryMissing);
}

void zigbeeDriverCommonUpdateBatteryTemperatureStatus(DeviceDriver *driver, uint64_t eui64, bool isHigh)
{
    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    updateResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_BATTERY_HIGH_TEMPERATURE, stringValueOfBool(isHigh), NULL);
}

static void handleBatteryTemperatureStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isHigh)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;
    zigbeeDriverCommonUpdateBatteryTemperatureStatus(commonDriver, eui64, isHigh);
}

void zigbeeDriverCommonUpdateAcMainsStatus(DeviceDriver *driver, uint64_t eui64, bool isAcMainsConnected)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    updateResource(
        deviceUuid, 0, COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED, isAcMainsConnected ? "false" : "true", NULL);
    free(deviceUuid);
}

static void handleAcMainsStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isAcMainsConnected)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateAcMainsStatus(commonDriver, eui64, isAcMainsConnected);
}

static void handleCliCommandResponse(uint64_t eui64, uint8_t endpointId, const char *response, const void *ctx)
{
    scoped_generic char *localEui64 = zigbeeSubsystemEui64ToId(eui64);
    sendZigbeeRemoteCliCommandResponseReceivedEvent(localEui64, response);
}

/**
 * Reprocess any commands received before the device was persisted, e.g. pass them to their appropriate
 * cluster/driver to be handled.   Typically this would be called from a devicePersisted callback.
 * @param driver the calling driver
 * @param eui64 the eui64 of the device
 * @param filter a function that will return true for the commands to be processed, or NULL can be passed to process
 * them all.
 */
void zigbeeDriverCommonProcessPrematureClusterCommands(ZigbeeDriverCommon *driver,
                                                       uint64_t eui64,
                                                       receivedClusterCommandFilter filter)
{
    icLinkedList *commands = zigbeeSubsystemGetPrematureClusterCommands(eui64);
    icLinkedListIterator *iter = linkedListIteratorCreate((icLinkedList *) commands);
    while (linkedListIteratorHasNext(iter))
    {
        ReceivedClusterCommand *item = (ReceivedClusterCommand *) linkedListIteratorGetNext(iter);
        if (filter == NULL || filter(item))
        {
            clusterCommandReceived(driver, item);
        }
    }
    linkedListIteratorDestroy(iter);

    linkedListDestroy(commands, (linkedListItemFreeFunc) freeReceivedClusterCommand);
}

void zigbeeDriverCommonRegisterNewDevice(ZigbeeDriverCommon *driver, icDevice *device)
{
    // Pass through to our common registration function
    registerNewDevice(device, driver->deviceCallbacks);
}

void zigbeeDriverCommonSetBlockingUpgrade(ZigbeeDriverCommon *driver, uint64_t eui64, bool inProgress)
{
    icLogDebug(LOG_TAG, "%s: %016" PRIx64 " upgrade %s", __FUNCTION__, eui64, inProgress ? "in progress" : "complete");

    pthread_mutex_lock(&blockingUpgradesMtx);
    if (inProgress)
    {
        if (blockingUpgrades == NULL)
        {
            blockingUpgrades = hashMapCreate();
        }

        uint64_t *key = malloc(sizeof(uint64_t));
        *key = eui64;
        hashMapPut(blockingUpgrades, key, sizeof(uint64_t), NULL);
    }
    else
    {
        if (blockingUpgrades != NULL)
        {
            if (hashMapDelete(blockingUpgrades, &eui64, sizeof(uint64_t), NULL) == false)
            {
                icLogError(LOG_TAG, "%s: device not found in blockingUpgrades set", __FUNCTION__);
            }
            else
            {
                // notify anyone that might be waiting that our map of blocking upgrades has shrunk
                pthread_cond_broadcast(&blockingUpgradesCond);
            }
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: no blockingUpgrades set", __FUNCTION__);
        }
    }
    pthread_mutex_unlock(&blockingUpgradesMtx);
}

bool zigbeeDriverCommonIsDiscoveryActive(ZigbeeDriverCommon *driver)
{
    return driver->discoveryActive;
}

static void triggerBackgroundResetToFactory(uint8_t epid, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    resetToFactoryData *resetData = (resetToFactoryData *) calloc(1, sizeof(resetToFactoryData));
    resetData->epid = epid;
    resetData->eui64 = eui64;

    createDetachedThread(resetToFactoryTask, resetData, "zbDrvDefaultDev");
}

/*
 * validate file's MD5 checksum against the one present in DDL
 */
static bool validateMD5Checksum(const char *originalMD5Checksum, const char *filePath)
{
    bool retVal = false;

    scoped_generic char *fileMd5Checksum = digestFileHex(filePath, CRYPTO_DIGEST_MD5);
    if (stringCompare(originalMD5Checksum, fileMd5Checksum, true) == 0)
    {
        retVal = true;
    }

    return retVal;
}

/*
 * curl callback to store URL contents into a file
 */
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

/*
 * Do the firmware file download
 */
static bool
downloadFirmwareFile(const char *firmwareBaseUrl, const char *firmwareDirectory, DeviceFirmwareFileInfo *fileInfo)
{
    bool result = false;
    char *url = stringBuilder("%s/%s", firmwareBaseUrl, fileInfo->fileName);

    icLogDebug(LOG_TAG, "downloadFirmwareFile: attempting to download firmware from %s", url);

    char *outFileName = stringBuilder("%s/%s", firmwareDirectory, fileInfo->fileName);
    // download the new list
    CURL *curl;

    CURLcode res;
    curl = curl_easy_init();
    if (curl)
    {
        // Write to a temp file in case the transfer dies in the middle so we don't leave a partial firmware
        // file sitting around
        FILE *fp = tmpfile();
        if (fp != NULL)
        {
            // set standard curl options
            const char *propKey = sslVerifyPropKeyForCategoryBarton(SSL_VERIFY_HTTP_FOR_SERVER);
            g_autoptr(BCorePropertyProvider) propertyProvider =
                deviceServiceConfigurationGetPropertyProvider();
            g_autofree char *propValue =
                b_core_property_provider_get_property_as_string(propertyProvider, propKey, NULL);

            sslVerify verifyFlag = convertVerifyPropValToModeBarton(propValue);
            applyStandardCurlOptions(curl, url, 60, NULL, verifyFlag, false);
            if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
            {
                icLogError(LOG_TAG, "curl_easy_setopt(curl, CURLOPT_URL, url) failed at %s(%d)", __FILE__, __LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data) != CURLE_OK)
            {
                icLogError(LOG_TAG,
                           "curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data) failed at %s(%d)",
                           __FILE__,
                           __LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp) != CURLE_OK)
            {
                icLogError(
                    LOG_TAG, "curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp) failed at %s(%d)", __FILE__, __LINE__);
            }
            res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s\n", __func__, curl_easy_strerror(res));
                fclose(fp);
            }
            else
            {
                icLogDebug(LOG_TAG,
                           "downloadFirmwareFile: firmware file download finished, moving into place at %s",
                           outFileName);
                // Copy from the temp file to out output file
                rewind(fp);
                FILE *outFp = fopen(outFileName, "wb");
                // This will *always* close both files, which will delete 'fp' per tmpfile() API
                if (!copyFile(fp, outFp))
                {
                    icLogError(LOG_TAG, "Failed to copy firmware temp file to firmware directory: %d", errno);
                    // Cleanup the file in case it got partially there
                    if (deleteFile(outFileName) == false)
                    {
                        icLogError(LOG_TAG, "Failed remove %s", outFileName);
                    }
                }
                else
                {
                    /* fp already closed by copyFile */
                    fp = NULL;
                    icLogInfo(LOG_TAG, "downloadFirmwareFile: firmware file %s successfully downloaded!", outFileName);
                    // Set permissions
                    if (chmod(outFileName, 0777) != 0)
                    {
                        icLogError(LOG_TAG, "Failed set permissions on %s", outFileName);
                    }
                    // Download was successful
                    result = true;
                }
            }
        }
        else
        {
            icLogError(LOG_TAG, "failed to open output file for writing: %s", outFileName);
        }
        // Cleanup curl
        curl_easy_cleanup(curl);
    }

    free(outFileName);
    free(url);

    return result;
}

/*
 * Download the firmware files related to this device descriptor.
 *
 * Return true on success, false on failure.
 */
bool zigbeeDriverCommonDownloadFirmwareFiles(const DeviceDescriptor *dd)
{
    bool result = false;
    int filesRequired = 0;
    int filesAvailable = 0;
    scoped_generic const char *firmwareDirectory =
        zigbeeSubsystemGetAndCreateFirmwareFileDirectory(dd->latestFirmware->type);

    if (firmwareDirectory != NULL)
    {
        // This property gets straight mapped to a CPE property
        g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        scoped_generic char *firmwareBaseUrl =
            b_core_property_provider_get_property_as_string(propertyProvider, DEVICE_FIRMWARE_URL_NODE, NULL);

        if (firmwareBaseUrl == NULL)
        {
            icLogError(LOG_TAG, "Device Firmware Base URL was empty, cannot download firmware");
        }
        else
        {
            icLinkedListIterator *iterator = linkedListIteratorCreate(dd->latestFirmware->fileInfos);
            while (linkedListIteratorHasNext(iterator) == true)
            {
                filesRequired++;

                DeviceFirmwareFileInfo *fileInfo = (DeviceFirmwareFileInfo *) linkedListIteratorGetNext(iterator);
                scoped_generic const char *path = stringBuilder("%s/%s", firmwareDirectory, fileInfo->fileName);

                // Existing OTA file is valid if
                // - checksum field is not available in DDL for this device
                // - checksum field is available in DDL and it matches with that of existing file
                // If checksum validation fails, delete existing one and download
                //
                bool shouldSkipDownload = false;
                if (doesNonEmptyFileExist(path) == true)
                {
                    if (fileInfo->checksum == NULL)
                    {
                        shouldSkipDownload = true;
                    }
                    else if (validateMD5Checksum(fileInfo->checksum, path) == true)
                    {
                        shouldSkipDownload = true;
                    }
                    else
                    {
                        // File exists and its MD5 sum is obsolete, remove the file
                        //
                        if (deleteFile(path) == false)
                        {
                            icLogError(LOG_TAG, "Failed to remove %s", path);
                        }
                    }
                }

                if (shouldSkipDownload == false)
                {
                    icLogDebug(LOG_TAG,
                               "%s: Either file %s is missing in %s or its MD5 checksum validation failed; downloading",
                               __FUNCTION__,
                               fileInfo->fileName,
                               firmwareDirectory);

                    if (downloadFirmwareFile(firmwareBaseUrl, firmwareDirectory, fileInfo))
                    {
                        // Downloaded file is valid if
                        // - DDL does not have checksum field for this file
                        // - DDL's checksum value matches with that of downloaded file
                        //
                        bool isFileValid =
                            (fileInfo->checksum == NULL) ? true : validateMD5Checksum(fileInfo->checksum, path);

                        if (isFileValid == true)
                        {
                            filesAvailable++;
                        }
                        else
                        {
                            // Checksum validation failed , remove the file and break
                            //
                            icLogWarn(LOG_TAG, "MD5 checksum validation failed for %s", fileInfo->fileName);

                            if (deleteFile(path) == false)
                            {
                                icLogError(LOG_TAG, "Failed to remove %s", path);
                            }
                            break;
                        }
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Firmware file %s failed to download", fileInfo->fileName);
                    }
                }
                else
                {
                    icLogDebug(LOG_TAG,
                               "Firmware file %s already exists in directory %s",
                               fileInfo->fileName,
                               firmwareDirectory);

                    filesAvailable++;
                }
            }
            linkedListIteratorDestroy(iterator);

            if (filesRequired == filesAvailable)
            {
                result = true;
            }
        }

        // We can do this part regardless of whether or not we got all the files
        if (filesAvailable > 0)
        {
            // Inform zigbee that there are new OTA files for devices
            zhalRefreshOtaFiles();
        }
    }
    else
    {
        icLogError(LOG_TAG, "Could not get/create firmware directory for dd uuid: %s", dd->uuid);
    }

    return result;
}

/*
 * For a device, download all firmware files and apply the upgrade.
 */
static void doFirmwareUpgrade(void *arg)
{
    FirmwareUpgradeContext *ctx = (FirmwareUpgradeContext *) arg;

    // The delayed task handle is no longer valid.  Remove it from our map.
    pthread_mutex_lock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);
    bool found = hashMapDelete(
        ctx->commonDriver->pendingFirmwareUpgrades, ctx->taskHandle, sizeof(uint32_t), standardDoNotFreeHashMapFunc);
    pthread_mutex_unlock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);

    if (!found)
    {
        icLogInfo(LOG_TAG, "%s: exiting since this upgrade was not found in pendingFirmwareUpgrades", __FUNCTION__);
        destroyFirmwareUpgradeContext(ctx);
        return;
    }

    DeviceDescriptor *dd = ctx->dd;

    bool willRetry = false;
    if (zigbeeDriverCommonDownloadFirmwareFiles(dd) == false)
    {
        icLogError(LOG_TAG, "%s: failed to download firmware files", __FUNCTION__);

        g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        uint32_t retrySeconds = b_core_property_provider_get_property_as_uint32(
            propertyProvider, FIRMWARE_UPGRADE_RETRYDELAYSECS, FIRMWARE_UPGRADE_RETRYDELAYSECS_DEFAULT);

        icLogInfo(LOG_TAG, "%s: rescheduling for %" PRIu32 " seconds", __FUNCTION__, retrySeconds);

        pthread_mutex_lock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);

        // Reschedule for a retry
        *ctx->taskHandle = scheduleDelayTask(retrySeconds, DELAY_SECS, doFirmwareUpgrade, ctx);
        if (*ctx->taskHandle > 0)
        {
            hashMapPut(ctx->commonDriver->pendingFirmwareUpgrades, ctx->taskHandle, sizeof(uint32_t), ctx);

            // Make sure we don't cleanup the context since we still need it
            willRetry = true;
        }

        pthread_mutex_unlock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);
    }

    if (!willRetry)
    {
        if (ctx->commonDriver->commonCallbacks->initiateFirmwareUpgrade != NULL)
        {
            ctx->commonDriver->commonCallbacks->initiateFirmwareUpgrade(ctx->commonDriver, ctx->deviceUuid, dd);
        }
        else
        {
            // we completed the download and we dont have a custom initiate firmware upgrade callback.
            //  Attempt a standard OTA Upgrade cluster image notify command.  That will
            //  harmlessly fail on very sleepy devices and/or legacy iControl security devices.
            //  If that doesnt work, the notify will be sent if the device supports the poll control checkin
            //  cluster and checks in with a pending firmware upgrade set.
            uint64_t eui64 = zigbeeSubsystemIdToEui64(ctx->deviceUuid);

            // if for whatever reason we didnt get an endpoint number, fall back to the most common value of 1
            uint8_t epid =
                ctx->endpointId == NULL ? 1 : getEndpointNumber(ctx->commonDriver, ctx->deviceUuid, ctx->endpointId);
            otaUpgradeClusterImageNotify(eui64, epid);
        }

        destroyFirmwareUpgradeContext(ctx);
    }
}

// set the provided metadata on this device
static void
processDeviceDescriptorMetadata(ZigbeeDriverCommon *commonDriver, icDevice *device, icStringHashMap *metadata)
{
    // If this device is not yet in our database, then it is a newly pairing device which already processed the metadata
    if (device->uri == NULL)
    {
        icLogDebug(LOG_TAG, "%s: skipping metadata processing for newly paired device", __FUNCTION__);
        return;
    }

    icStringHashMapIterator *it = stringHashMapIteratorCreate(metadata);
    while (stringHashMapIteratorHasNext(it))
    {
        char *key;
        char *value;
        if (stringHashMapIteratorGetNext(it, &key, &value))
        {
            icLogInfo(LOG_TAG, "%s: setting metadata (%s=%s) on device %s", __FUNCTION__, key, value, device->uuid);

            setMetadata(device->uuid, NULL, key, value);
        }
    }
    stringHashMapIteratorDestroy(it);

    // now let the higher level device driver at it, if it cares
    if (commonDriver->commonCallbacks->processDeviceDescriptorMetadata != NULL)
    {
        commonDriver->commonCallbacks->processDeviceDescriptorMetadata(commonDriver, device, metadata);
    }
}

void zigbeeDriverCommonCancelPendingUpgrades(ZigbeeDriverCommon *commonDriver, const char *uuid)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&commonDriver->pendingFirmwareUpgradesMtx);
    icHashMapIterator *pendingIt = hashMapIteratorCreate(commonDriver->pendingFirmwareUpgrades);
    while (hashMapIteratorHasNext(pendingIt) == true)
    {
        void *taskHandle;
        uint16_t keyLen;
        FirmwareUpgradeContext *fwupgctx;
        hashMapIteratorGetNext(pendingIt, &taskHandle, &keyLen, (void **) &fwupgctx);

        // check if this operation is for a specific device which ideally happens while it's
        // being removed. If yes, handle for that and break
        //
        if (uuid != NULL)
        {
            if (stringCompare(fwupgctx->deviceUuid, uuid, true) == 0)
            {
                icLogDebug(LOG_TAG, "%s: canceling pending upgrade for %s", __FUNCTION__, uuid);
                cancelDelayTask(*((uint32_t *) taskHandle));
                hashMapIteratorDeleteCurrent(pendingIt, pendingFirmwareUpgradeFreeFunc);
                break;
            }
            else
            {
                continue;
            }
        }

        cancelDelayTask(*((uint32_t *) taskHandle));
        hashMapIteratorDeleteCurrent(pendingIt, pendingFirmwareUpgradeFreeFunc);
    }
    hashMapIteratorDestroy(pendingIt);
    pthread_mutex_unlock(&commonDriver->pendingFirmwareUpgradesMtx);
}

bool zigbeeDriverCommonIsBatteryBackedUp(const ZigbeeDriverCommon *commonDriver)
{
    return commonDriver->batteryBackedUp;
}

ZigbeeCluster *zigbeeDriverCommonGetCluster(const ZigbeeDriverCommon *commonDriver, uint16_t clusterId)
{
    return hashMapGet(commonDriver->clusters, &clusterId, sizeof(clusterId));
}

/*
 * This can wait forever in a stuck scenario. Device service on shutdown will allow up to some max time
 * (historically 31 minutes) before existing the process.
 */
static void waitForUpgradesToComplete()
{
    while (true)
    {
        pthread_mutex_lock(&blockingUpgradesMtx);
        uint16_t count = 0;
        if (blockingUpgrades != NULL)
        {
            count = hashMapCount(blockingUpgrades);
        }

        if (count == 0)
        {
            pthread_mutex_unlock(&blockingUpgradesMtx);
            break;
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: %" PRIu16 " upgrades are blocking", __FUNCTION__, count);
            pthread_cond_wait(&blockingUpgradesCond, &blockingUpgradesMtx);
        }

        pthread_mutex_unlock(&blockingUpgradesMtx);
    }
}

static void destroyMapCluster(void *key, void *value)
{
    ZigbeeCluster *cluster = (ZigbeeCluster *) value;
    if (cluster && cluster->destroy)
    {
        cluster->destroy(cluster);
    }
    free(cluster);
    free(key);
}

static void pendingFirmwareUpgradeFreeFunc(void *key, void *value)
{
    (void) key;
    destroyFirmwareUpgradeContext(value);
}

static void systemPowerEvent(void *ctx, DeviceServiceSystemPowerEventType powerEvent)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->systemPowerEvent != NULL)
    {
        commonDriver->commonCallbacks->systemPowerEvent(powerEvent);
    }
}

static void propertyChanged(void *ctx, const char *propKey, const char *propValue)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->handlePropertyChanged != NULL)
    {
        commonDriver->commonCallbacks->handlePropertyChanged(commonDriver, propKey, propValue);
    }
}

static void fetchRuntimeStats(void *ctx, icStringHashMap *output)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->fetchRuntimeStats != NULL)
    {
        commonDriver->commonCallbacks->fetchRuntimeStats(commonDriver, output);
    }
}

static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version)
{
    bool result = false;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;
    if (version != NULL && stringCompare(commonDriver->deviceClass, deviceClass, false) == 0)
    {
        *version = commonDriver->deviceClassVersion;
        result = true;
    }

    return result;
}

static void handleSubsystemInitialized(void *ctx)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->subsystemInitialized != NULL)
    {
        commonDriver->commonCallbacks->subsystemInitialized(commonDriver);
    }
}

static void serviceStatusChanged(void *ctx, DeviceServiceStatus *status)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->serviceStatusChanged != NULL)
    {
        commonDriver->commonCallbacks->serviceStatusChanged(commonDriver, status);
    }
}

static void handleBatteryRechargeCyclesChanged(void *ctx, uint64_t eui64, uint16_t rechargeCycles)
{
    icLogTrace(LOG_TAG, "%s ", __FUNCTION__);
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->updateBatteryRechargeCycles != NULL)
    {
        commonDriver->commonCallbacks->updateBatteryRechargeCycles(commonDriver, eui64, rechargeCycles);
    }
}

// start the collection task if it isnt already running
static void diagnosticsCollectionTaskStart(void)
{
    mutexLock(&diagCollectorMtx);
    if (diagCollectorTask == 0)
    {
        diagCollectorTask = createFixedRateRepeatingTask(
            DIAGNOSTICS_COLLECTION_INTERVAL_MINS, DELAY_MINS, diagnosticsCollectionTaskFunc, NULL);
    }
    mutexUnlock(&diagCollectorMtx);
}

// stop the collection task if its running
static void diagnosticsCollectionTaskStop(void)
{
    mutexLock(&diagCollectorMtx);
    if (diagCollectorTask != 0)
    {
        cancelRepeatingTask(diagCollectorTask);
        diagCollectorTask = 0;
    }
    mutexUnlock(&diagCollectorMtx);
}

// iterate over all devices that need their diagnostics collected
// NOTE: do not acquire diagCollectorMtx here or shutdown deadlock could occur
static void diagnosticsCollectionTaskFunc(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __func__);

    static bool firstTime = true;

    // Skip the first invocation to give the same effect as a repeating task with a startup
    // delay, deferring the first set of resource update events that this will generate.
    if (firstTime == true)
    {
        firstTime = false;
        return;
    }

    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDriversBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    sbIcLinkedListIterator *driversIt = linkedListIteratorCreate(deviceDrivers);
    while (linkedListIteratorHasNext(driversIt) == true)
    {
        // see if this driver has this feature enabled
        ZigbeeDriverCommon *driver = linkedListIteratorGetNext(driversIt);
        if (driver->diganosticsCollectionEnabled == true)
        {
            // iterate over this driver's devices and do the needful
            icLinkedList *devices = deviceServiceGetDevicesByDeviceDriver(driver->baseDriver.driverName);
            sbIcLinkedListIterator *devicesIt = linkedListIteratorCreate(devices);
            while (linkedListIteratorHasNext(devicesIt) == true)
            {
                icDevice *device = linkedListIteratorGetNext(devicesIt);
                uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
                uint8_t endpointNumber = 0;

                // dont bother querying devices that we know are in comm fail
                if (deviceServiceIsDeviceInCommFail(device->uuid) == true)
                {
                    continue;
                }

                // find the endpoint with the diagnostics cluster
                IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, driver);

                if (icDiscoveredDeviceDetailsGetClusterEndpoint(details, DIAGNOSTICS_CLUSTER_ID, &endpointNumber) ==
                    true)
                {
                    char temp[5]; // -127 + \0 worst case
                    int8_t feRssi;
                    uint8_t feLqi;

                    if (diagnosticsClusterGetLastMessageRssi(eui64, endpointNumber, &feRssi) == true)
                    {
                        snprintf(temp, 5, "%" PRId8, feRssi);
                        updateResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_FERSSI, temp, NULL);
                    }

                    if (diagnosticsClusterGetLastMessageLqi(eui64, endpointNumber, &feLqi) == true)
                    {
                        snprintf(temp, 5, "%" PRIu8, feLqi);
                        updateResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_FELQI, temp, NULL);
                    }

                    // we have read (or attempted to read) from this device.  Delay here a bit so as not to overwhelm
                    // the network when this task fires.
                    sleep(DIAGNOSTICS_COLLECTION_DELAY_SECS);
                }
            }
            linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
        }
    }
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);
}

// FIXME: Copy Paste Tech Debt (duplicated in deviceDescriptorHandler)

#define DEFAULT_SSL_VERIFY_MODE               SSL_VERIFY_BOTH
#define SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER "cpe.sslCert.validateForHttpsToServer"
#define SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE "cpe.sslCert.validateForHttpsToDevice"
#define SSL_VERIFICATION_TYPE_NONE            "none"
#define SSL_VERIFICATION_TYPE_HOST            "host"
#define SSL_VERIFICATION_TYPE_PEER            "peer"
#define SSL_VERIFICATION_TYPE_BOTH            "both"

static const char *sslVerifyCategoryToProp[] = {
    SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER,
    SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE,
};

static sslVerify convertVerifyPropValToModeBarton(const char *strVal)
{
    sslVerify retVal = DEFAULT_SSL_VERIFY_MODE;
    if (strVal == NULL || strlen(strVal) == 0 || strcasecmp(strVal, SSL_VERIFICATION_TYPE_NONE) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_NONE option");
        retVal = SSL_VERIFY_NONE;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_HOST) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_HOST option");
        retVal = SSL_VERIFY_HOST;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_PEER) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_PEER option");
        retVal = SSL_VERIFY_PEER;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_BOTH) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_BOTH option");
        retVal = SSL_VERIFY_BOTH;
    }
    else
    {
        icLogDebug(LOG_TAG, "using default option [%d]", retVal);
    }

    return retVal;
}

static const char *sslVerifyPropKeyForCategoryBarton(sslVerifyCategory cat)
{
    const char *propKey = NULL;

    if (cat >= SSL_VERIFY_CATEGORY_FIRST && cat <= SSL_VERIFY_CATEGORY_LAST)
    {
        propKey = sslVerifyCategoryToProp[cat];
    }

    return propKey;
}

#endif // BARTON_CONFIG_ZIGBEE
