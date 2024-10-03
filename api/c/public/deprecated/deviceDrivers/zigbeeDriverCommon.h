//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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
// Created by tlea on 2/13/19.
//

#ifndef ZILKER_ZIGBEEDRIVERCOMMON_H
#define ZILKER_ZIGBEEDRIVERCOMMON_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/alarmsCluster.h"
#include "zigbeeClusters/comcastBatterySaving.h"
#include "zigbeeClusters/zigbeeCluster.h"
#include <device-driver/device-driver.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

typedef struct ZigbeeDriverCommon ZigbeeDriverCommon;

// receiver modes for zigbee devices
// Sleepy        - Rx is on only for a short duration after poll control checkin arrives
// Pseudo-sleepy - Rx is on periodically such that APS retries will deliver a message to it
// Non-sleepy    - Rx is always on
typedef enum
{
    RX_MODE_SLEEPY,
    RX_MODE_PSEUDO_SLEEPY,
    RX_MODE_NON_SLEEPY
} zigbeeReceiverMode;

// These callbacks will be invoked at the proper time if they are non-null
typedef struct
{
    bool (*claimDevice)(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details);

    void (*deviceRejoined)(ZigbeeDriverCommon *ctx, uint64_t eui64, bool isSecure, IcDiscoveredDeviceDetails *details);

    void (*deviceLeft)(ZigbeeDriverCommon *ctx, uint64_t eui64, IcDiscoveredDeviceDetails *details);

    void (*deviceRejected)(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details);

    bool (*getDiscoveredDeviceMetadata)(ZigbeeDriverCommon *ctx,
                                        IcDiscoveredDeviceDetails *details,
                                        icStringHashMap *metadata);

    bool (*configureDevice)(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails);

    bool (*fetchInitialResourceValues)(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

    bool (*registerResources)(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

    bool (*devicePersisted)(ZigbeeDriverCommon *ctx, icDevice *device);

    bool (*readEndpointResource)(ZigbeeDriverCommon *ctx,
                                 uint32_t endpointNumber,
                                 icDeviceResource *resource,
                                 char **value);

    bool (*readDeviceResource)(ZigbeeDriverCommon *ctx, icDeviceResource *resource, char **value);

    // baseDriverUpdatesResource can be set to false by the higher level driver to prevent the base driver from updating
    //  the resource
    bool (*writeEndpointResource)(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);

    bool (*writeDeviceResource)(ZigbeeDriverCommon *ctx,
                                icDeviceResource *resource,
                                const char *previousValue,
                                const char *newValue);

    bool (*executeEndpointResource)(ZigbeeDriverCommon *ctx,
                                    uint32_t endpointNumber,
                                    icDeviceResource *resource,
                                    const char *arg,
                                    char **response);

    bool (*executeDeviceResource)(ZigbeeDriverCommon *ctx,
                                  icDeviceResource *resource,
                                  const char *arg,
                                  char **response);

    const char *(*mapDeviceIdToProfile)(ZigbeeDriverCommon *ctx, uint16_t deviceId);

    // Additional hooks available if needed

    /**
     * Configure the driver before startup
     * @param ctx
     * @param commFailTimeoutSeconds Maximum silence interval before marking the device in comm fail
     *                               Set to 0 to disable monitoring.
     */
    void (*preStartup)(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds);

    void (*postStartup)(ZigbeeDriverCommon *ctx);

    void (*preShutdown)(ZigbeeDriverCommon *ctx);

    void (*postShutdown)(ZigbeeDriverCommon *ctx);

    void (*preDeleteDriver)(ZigbeeDriverCommon *ctx);

    // This hook can be used to process metadata found in the device descriptor.  The base driver already stored
    //  this in the device's metadata.
    void (*processDeviceDescriptorMetadata)(ZigbeeDriverCommon *ctx, icDevice *device, icStringHashMap *metadata);

    // returning true means we are accepting this device without the normal processing
    bool (*preDeviceDiscovered)(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details);

    void (*preDiscoverStart)(ZigbeeDriverCommon *ctx, const char *deviceClass);

    void (*postDiscoverStart)(ZigbeeDriverCommon *ctx, const char *deviceClass);

    void (*preDiscoverStop)(ZigbeeDriverCommon *ctx);

    void (*postDiscoverStop)(ZigbeeDriverCommon *ctx);

    void (*preDeviceRemoved)(ZigbeeDriverCommon *ctx, icDevice *device);

    void (*postDeviceRemoved)(ZigbeeDriverCommon *ctx, icDevice *device);

    // list of icDevice.  List and items will be destroyed after callback invocation
    void (*devicesLoaded)(ZigbeeDriverCommon *ctx, icLinkedList *devices);

    void (*communicationFailed)(ZigbeeDriverCommon *ctx, icDevice *device);

    void (*communicationRestored)(ZigbeeDriverCommon *ctx, icDevice *device);

    void (*handleAlarm)(ZigbeeDriverCommon *ctx,
                        uint64_t eui64,
                        uint8_t endpointId,
                        const ZigbeeAlarmTableEntry *alarm);

    void (*handleAlarmCleared)(ZigbeeDriverCommon *ctx,
                               uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *alarm);

    void (*initiateFirmwareUpgrade)(ZigbeeDriverCommon *ctx, const char *deviceUuid, const DeviceDescriptor *dd);

    // callback frees report with freeReceivedAttributeReport()
    void (*handleAttributeReport)(ZigbeeDriverCommon *ctx, ReceivedAttributeReport *report);

    // callback frees command with freeReceivedClusterCommand()
    void (*handleClusterCommand)(ZigbeeDriverCommon *ctx, ReceivedClusterCommand *command);

    void (*setEndpointNumber)(ZigbeeDriverCommon *ctx, icDeviceEndpoint *endpoint, uint8_t endpointNumber);

    bool (*preConfigureCluster)(ZigbeeDriverCommon *ctx,
                                ZigbeeCluster *cluster,
                                DeviceConfigurationContext *deviceConfigContext);

    void (*synchronizeDevice)(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details);

    bool (*firmwareUpgradeRequired)(ZigbeeDriverCommon *ctx,
                                    const char *deviceUuid,
                                    const char *latestVersion,
                                    const char *currentVersion);

    void (*endpointDisabled)(ZigbeeDriverCommon *ctx, icDeviceEndpoint *endpoint);

    void (*systemPowerEvent)(DeviceServiceSystemPowerEventType powerEvent);

    void (*firmwareUpgradeFailed)(ZigbeeDriverCommon *ctx, uint64_t eui64);

    void (*handlePropertyChanged)(ZigbeeDriverCommon *ctx, const char *propKey, const char *propValue);

    void (*fetchRuntimeStats)(ZigbeeDriverCommon *ctx, icStringHashMap *output);

    void (*updateBatteryRechargeCycles)(void *ctx, uint64_t eui64, uint16_t rechargeCycles);

    void (*subsystemInitialized)(ZigbeeDriverCommon *ctx);

    void (*serviceStatusChanged)(ZigbeeDriverCommon *ctx, DeviceServiceStatus *status);

} ZigbeeDriverCommonCallbacks;

DeviceDriver *zigbeeDriverCommonCreateDeviceDriver(const char *driverName,
                                                   const char *deviceClass,
                                                   uint8_t deviceClassVersion,
                                                   const uint16_t *deviceIds,
                                                   uint8_t numDeviceIds,
                                                   zigbeeReceiverMode rxMode,
                                                   const ZigbeeDriverCommonCallbacks *commonCallbacks,
                                                   bool customCommFail);

void zigbeeDriverCommonAddCluster(DeviceDriver *driver, ZigbeeCluster *cluster);

void zigbeeDriverCommonSetDiagnosticsCollectionEnabled(DeviceDriver *driver, bool enabled);

void zigbeeDriverCommonRegisterBatteryThresholdResource(DeviceDriver *driver, bool enabled);

void zigbeeDriverCommonSetEndpointNumber(icDeviceEndpoint *endpoint, uint8_t endpointNumber);

uint8_t zigbeeDriverCommonGetEndpointNumber(ZigbeeDriverCommon *ctx, icDeviceEndpoint *endpoint);

const char *zigbeeDriverCommonGetDeviceClass(ZigbeeDriverCommon *ctx);

// FIXME: no lookup by device/deviceId. Note 'commFailOverrideSeconds' metadata is read
// by the standard deviceServiceCommFail module.
uint32_t zigbeeDriverCommonGetDeviceCommFailTimeout(DeviceDriver *driver);

// configure this instance to not perform any discovery or configuration of devices during pairing
void zigbeeDriverCommonSkipConfiguration(DeviceDriver *driver);

bool zigbeeDriverCommonConfigureEndpointClusters(uint64_t eui64,
                                                 uint8_t endpointId,
                                                 ZigbeeDriverCommon *commonDriver,
                                                 IcDiscoveredDeviceDetails *deviceDetails,
                                                 DeviceDescriptor *descriptor);

DeviceDescriptor *zigbeeDriverCommonGetDeviceDescriptor(const char *manufacturer,
                                                        const char *model,
                                                        uint64_t hardwareVersion,
                                                        uint64_t firmwareVersion);

char *getZigbeeVersionString(uint32_t version);

uint32_t getZigbeeVersionFromString(const char *version);

bool validateOtaUpgradeMessage(OtaUpgradeEvent *otaEvent);

void *zigbeeDriverCommonGetDriverPrivateData(ZigbeeDriverCommon *ctx);
void zigbeeDriverCommonSetDriverPrivateData(ZigbeeDriverCommon *ctx, void *privateData);

// configure this instance as being for devices that are battery backed up
void zigbeeDriverCommonSetBatteryBackedUp(DeviceDriver *driver);

// Common resource update functions
void zigbeeDriverCommonUpdateAcMainsStatus(DeviceDriver *driver, uint64_t eui64, bool isAcMainsConnected);

void zigbeeDriverCommonUpdateBatteryChargeStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryLow);

void zigbeeDriverCommonUpdateBatteryBadStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryBad);

void zigbeeDriverCommonUpdateBatteryMissingStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryMissing);

void zigbeeDriverCommonUpdateBatteryTemperatureStatus(DeviceDriver *driver, uint64_t eui64, bool isHigh);

typedef bool (*receivedClusterCommandFilter)(const ReceivedClusterCommand *receivedClusterCommand);

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
                                                       receivedClusterCommandFilter filter);

void zigbeeDriverCommonRegisterNewDevice(ZigbeeDriverCommon *driver, icDevice *device);

/**
 * Inform the common driver that a firmware upgrade is in progress that should block shutdown if possible.
 *
 * @param driver the calling driver
 * @param eui64 the eui64 of the device
 * @param inProgress true if a blocking upgrade is in progress or false if it has completed
 */
void zigbeeDriverCommonSetBlockingUpgrade(ZigbeeDriverCommon *driver, uint64_t eui64, bool inProgress);

/**
 * Find out if this driver is currently participating in device discovery
 * @param driver
 * @return true when discovery is active
 */
bool zigbeeDriverCommonIsDiscoveryActive(ZigbeeDriverCommon *driver);

/**
 * Download the files related to the provided device descriptor.
 * @param dd
 * @return true if all files are available
 */
bool zigbeeDriverCommonDownloadFirmwareFiles(const DeviceDescriptor *dd);

/**
 * Cancel any pending upgrades for this driver. If uuid is present in argument
 * then cancel pending upgrade only for that particular device
 * @param commonDriver
 * @param uuid the uuid of device (can be NULL)
 */
void zigbeeDriverCommonCancelPendingUpgrades(ZigbeeDriverCommon *commonDriver, const char *uuid);

/**
 * Check if this driver is for battery backed up devices
 * @param commonDriver
 * @return true if is battery backed up
 */
bool zigbeeDriverCommonIsBatteryBackedUp(const ZigbeeDriverCommon *commonDriver);

/**
 * Retrieve a cluster instance for this driver
 *
 * @param commonDriver
 * @param clusterId
 * @return the cluster handle or NULL if not found
 */
ZigbeeCluster *zigbeeDriverCommonGetCluster(const ZigbeeDriverCommon *commonDriver, uint16_t clusterId);

/**
 * Handle a ComcastBatterySavingData structure by updating related resources.
 *
 * @param eui64  the eui64 of the device that sent the data
 * @param data the data to handle
 * @param ctx a ZigbeeDriverCommon handle
 */
void zigbeeDriverCommonComcastBatterySavingUpdateResources(uint64_t eui64,
                                                           const ComcastBatterySavingData *data,
                                                           const void *ctx);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_ZIGBEEDRIVERCOMMON_H
