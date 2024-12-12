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

#ifndef FLEXCORE_ZIGBEESUBSYSTEM_H
#define FLEXCORE_ZIGBEESUBSYSTEM_H

#include "device-driver/device-driver.h"
#include "zigbeeAttributeTypes.h"
#include <cjson/cJSON.h>
#include <deviceDescriptor.h>
#include <icTypes/icLinkedList.h>
#include <stdbool.h>
#include <zhal/zhal.h>

#define ZIGBEE_SUBSYSTEM_NAME             "zigbee"
#define NETWORK_BLOB_PROPERTY_NAME        "ZIGBEE_NETWORK_CONFIG_DATA"

/* 27 min */
#define ZIGBEE_DEFAULT_CHECKIN_INTERVAL_S 27 * 60

#define FAST_COMM_FAIL_PROP               "zigbee.testing.fastCommFail.flag"

typedef enum
{
    channelChangeUnknown,
    channelChangeSuccess,
    channelChangeFailed,
    channelChangeNotAllowed,
    channelChangeInvalidChannel,
    channelChangeInProgress,
    channelChangeUnableToCalculate
} ChannelChangeResponseCode;

typedef struct
{
    uint8_t channelNumber;
    ChannelChangeResponseCode responseCode;
} ChannelChangeResponse;

typedef struct
{
    // this value will be provided back to the caller in each callback
    void *callbackContext;

    // callback frees report with freeReceivedAttributeReport()
    void (*attributeReportReceived)(void *ctx, ReceivedAttributeReport *report);

    // callback frees command with freeReceivedClusterCommand()
    void (*clusterCommandReceived)(void *ctx, ReceivedClusterCommand *command);

    void (*otaUpgradeMessageSent)(void *ctx, OtaUpgradeEvent *otaEvent);

    void (*otaUpgradeMessageReceived)(void *ctx, OtaUpgradeEvent *otaEvent);

    void (*deviceRejoined)(void *ctx, uint64_t eui64, bool isSecure);

    void (*deviceLeft)(void *ctx, uint64_t eui64);

    void (*deviceAnnounced)(void *ctx, uint64_t eui64, zhalDeviceType deviceType, zhalPowerSource powerSource);

} ZigbeeSubsystemDeviceCallbacks;

typedef struct
{
    uint8_t attributeType;
    uint16_t dataLen;
    uint8_t *data;
} IcDiscoveredAttributeValue;

typedef struct
{
    uint16_t clusterId;
    bool isServer;
    uint16_t *attributeIds;
    // Optional array of values
    IcDiscoveredAttributeValue *attributeValues;
    uint16_t numAttributeIds;
} IcDiscoveredClusterDetails;

typedef struct
{
    uint8_t endpointId;
    uint16_t appProfileId;
    uint16_t appDeviceId;
    uint8_t appDeviceVersion;
    IcDiscoveredClusterDetails *serverClusterDetails;
    uint8_t numServerClusterDetails;
    IcDiscoveredClusterDetails *clientClusterDetails;
    uint8_t numClientClusterDetails;
} IcDiscoveredEndpointDetails;

typedef struct
{
    uint64_t eui64;
    char *manufacturer; // presumed to be the same across all endpoints!
    char *model;        // presumed to be the same across all endpoints!
    uint64_t hardwareVersion;
    uint64_t firmwareVersion;
    uint64_t appVersion;
    uint8_t numEndpoints;
    IcDiscoveredEndpointDetails *endpointDetails;
    zhalDeviceType deviceType;
    zhalPowerSource powerSource;

} IcDiscoveredDeviceDetails;

typedef struct
{
    char *driverName;
    void *callbackContext;

    bool (*callback)(void *context, IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator);
} ZigbeeSubsystemDeviceDiscoveredHandler;

typedef struct
{
    uint64_t address;       // EUI64 of the zigbee device for this entry
    uint64_t nextCloserHop; // EUI64 of the next hop
    int32_t lqi;            // LQI of this hop
    uint16_t nodeId;        // Network node Id of device
} ZigbeeSubsystemNetworkMapEntry;

typedef enum
{
    LINK_QUALITY_LEVEL_UNKNOWN = 0,
    LINK_QUALITY_LEVEL_POOR = 1,
    LINK_QUALITY_LEVEL_FAIR = 2,
    LINK_QUALITY_LEVEL_GOOD = 3
} ZigbeeSubsystemLinkQualityLevel;

// This must be called in the order that the handlers will be invoked when a device is discovered.
//  the first handler to return true 'claims' the device and subsequent handlers will not be notified
int zigbeeSubsystemRegisterDiscoveryHandler(const char *name, ZigbeeSubsystemDeviceDiscoveredHandler *handler);

int zigbeeSubsystemUnregisterDiscoveryHandler(ZigbeeSubsystemDeviceDiscoveredHandler *handler);

// register callbacks for the provided eui64
int zigbeeSubsystemRegisterDeviceListener(uint64_t eui64, ZigbeeSubsystemDeviceCallbacks *callbacks);

int zigbeeSubsystemUnregisterDeviceListener(uint64_t eui64);

// increment the discovering device count.  Discovery runs until the count is decremented to zero
int zigbeeSubsystemStartDiscoveringDevices(void);

int zigbeeSubsystemStopDiscoveringDevices(void);

int zigbeeSubsystemSendCommand(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint8_t commandId,
                               uint8_t *message,
                               uint16_t messageLen);

int zigbeeSubsystemSendCommandDefaultResponse(uint64_t eui64,
                                              uint8_t endpointId,
                                              uint16_t clusterId,
                                              bool toServer,
                                              uint8_t commandId,
                                              uint8_t *message,
                                              uint16_t messageLen);

int zigbeeSubsystemSendCommandWithEncryption(uint64_t eui64,
                                             uint8_t endpointId,
                                             uint16_t clusterId,
                                             bool toServer,
                                             uint8_t commandId,
                                             uint8_t *message,
                                             uint16_t messageLen);

int zigbeeSubsystemSendMfgCommand(uint64_t eui64,
                                  uint8_t endpointId,
                                  uint16_t clusterId,
                                  bool toServer,
                                  uint8_t commandId,
                                  uint16_t mfgId,
                                  uint8_t *message,
                                  uint16_t messageLen);

int zigbeeSubsystemSendMfgCommandDefaultResponse(uint64_t eui64,
                                                 uint8_t endpointId,
                                                 uint16_t clusterId,
                                                 bool toServer,
                                                 uint8_t commandId,
                                                 uint16_t mfgId,
                                                 uint8_t *message,
                                                 uint16_t messageLen);

int zigbeeSubsystemSendMfgCommandWithEncryption(uint64_t eui64,
                                                uint8_t endpointId,
                                                uint16_t clusterId,
                                                bool toServer,
                                                uint8_t commandId,
                                                uint16_t mfgId,
                                                uint8_t *message,
                                                uint16_t messageLen);

int zigbeeSubsystemSendViaApsAck(uint64_t eui64,
                                 uint8_t endpointId,
                                 uint16_t clusterId,
                                 uint8_t sequenceNum,
                                 uint8_t *message,
                                 uint16_t messageLen);

/*
 * Caller frees non-null output
 */
int zigbeeSubsystemReadString(uint64_t eui64,
                              uint8_t endpointId,
                              uint16_t clusterId,
                              bool toServer,
                              uint16_t attributeId,
                              char **value);

/*
 * Can read 8 bit to 64 bit values.  Caller provides uint64_t output value and casts result as needed.
 */
int zigbeeSubsystemReadNumber(uint64_t eui64,
                              uint8_t endpointId,
                              uint16_t clusterId,
                              bool toServer,
                              uint16_t attributeId,
                              uint64_t *value);

/*
 * Can read 8 bit to 64 bit values.  Caller provides uint64_t output values array allocated to handle requested number
 * of attributes.  Each value can be cast as needed.
 */
int zigbeeSubsystemReadNumbers(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint8_t numAttributes,
                               const uint16_t *attributeIds,
                               uint64_t *values,
                               bool *readSuccesses);

/*
 * Caller frees non-null output
 */
int zigbeeSubsystemReadStringMfgSpecific(uint64_t eui64,
                                         uint8_t endpointId,
                                         uint16_t clusterId,
                                         uint16_t mfgId,
                                         bool toServer,
                                         uint16_t attributeId,
                                         char **value);

/*
 * Can read 8 bit to 64 bit values.  Caller provides uint64_t output value and casts result as needed.
 */
int zigbeeSubsystemReadNumberMfgSpecific(uint64_t eui64,
                                         uint8_t endpointId,
                                         uint16_t clusterId,
                                         uint16_t mfgId,
                                         bool toServer,
                                         uint16_t attributeId,
                                         uint64_t *value);

/*
 * Can read 8 bit to 64 bit values.  Caller provides uint64_t output values array allocated to handle requested number
 * of attributes.  Each value can be cast as needed.
 */
int zigbeeSubsystemReadNumbersMfgSpecific(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          uint16_t mfgId,
                                          bool toServer,
                                          uint8_t numAttributes,
                                          const uint16_t *attributeIds,
                                          uint64_t *values,
                                          bool *readSuccesses);

/*
 * Can write 8 to 64 bit values only.
 */
int zigbeeSubsystemWriteNumber(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint16_t attributeId,
                               ZigbeeAttributeType attributeType,
                               uint64_t value,
                               uint8_t numBytes);

/*
 * Can write 8 to 64 bit values only.
 */
int zigbeeSubsystemWriteNumberMfgSpecific(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          uint16_t mfgId,
                                          bool toServer,
                                          uint16_t attributeId,
                                          ZigbeeAttributeType attributeType,
                                          uint64_t value,
                                          uint8_t numBytes);

int zigbeeSubsystemBindingSet(uint64_t eui64, uint8_t endpointId, uint16_t clusterId);

// returns linked list of zhalBindingTableEntry on success or NULL on failure
icLinkedList *zigbeeSubsystemBindingGet(uint64_t eui64);

int zigbeeSubsystemBindingClear(uint64_t eui64, uint8_t endpointId, uint16_t clusterId);

// clear a remote binding to the provided target (not necessarily us).
int zigbeeSubsystemBindingClearTarget(uint64_t eui64,
                                      uint8_t endpointId,
                                      uint16_t clusterId,
                                      uint64_t targetEui64,
                                      uint8_t targetEndpointId);


int zigbeeSubsystemAttributesSetReporting(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          zhalAttributeReportingConfig *configs,
                                          uint8_t numConfigs);

int zigbeeSubsystemAttributesSetReportingMfgSpecific(uint64_t eui64,
                                                     uint8_t endpointId,
                                                     uint16_t clusterId,
                                                     uint16_t mfgId,
                                                     zhalAttributeReportingConfig *configs,
                                                     uint8_t numConfigs);

/*
 * Retrieve the available endpoint IDs from the target device.
 *
 * Caller frees non-null value returned in endpointIds.
 *
 * @return 0 on success
 */
int zigbeeSubsystemGetEndpointIds(uint64_t eui64, uint8_t **endpointIds, uint8_t *numEndpointIds);

/*
 * Discover the attributes available on a client or server cluster.
 *
 * Caller frees non-null value returned in infos.
 *
 * @return 0 on success
 */
int zigbeeSubsystemDiscoverAttributes(uint64_t eui64,
                                      uint8_t endpointId,
                                      uint16_t clusterId,
                                      bool toServer,
                                      zhalAttributeInfo **infos,
                                      uint16_t *numInfos);

/*
 * Retrieve our own EUI64
 *
 * @return our EUI64 or zero if not set or not known
 */
uint64_t getLocalEui64(void);

/*
 * Configure the complete list of Zigbee device addresses that are paired and allowed in our network.
 *
 * @return 0 on success
 */
int zigbeeSubsystemSetAddresses(void);

/*
 * Remove a single zigbee device address from those allowed on our network.
 */
int zigbeeSubsystemRemoveDeviceAddress(uint64_t eui64);

/*
 * Convert an EUI64 to a string (caller frees)
 */
char *zigbeeSubsystemEui64ToId(uint64_t eui64);

/**
 * Convert an endpoint id to a numeric string
 * @return a heap allocated numeric string (remember to free it)
 */
char *zigbeeSubsystemEndpointIdAsString(uint8_t epId);

/*
 * Convert a string to an EUI64
 */
uint64_t zigbeeSubsystemIdToEui64(const char *uuid);

/*
 * Retrieve the list of cluster commands received during discovery for the specified device which was
 * unknown at the time.  Used for supporting legacy sensors.
 *
 * destroy the returned list with icLinkedListDestroy and freeReceivedClusterCommand
 */
icLinkedList *zigbeeSubsystemGetPrematureClusterCommands(uint64_t eui64);

/**
 * Destroy premature cluster commands pending for a device
 * @param eui64
 */
void zigbeeSubsystemDestroyPrematureClusterCommands(uint64_t eui64);

/*
 * Wait for a specific premature cluster command.  This does not remove it from the collection.
 */
ReceivedClusterCommand *
zigbeeSubsystemGetPrematureClusterCommand(uint64_t eui64, uint8_t commandId, uint32_t timeoutSeconds);

/*
 * Remove any premature cluster commands for the provided device that match the command id
 */
void zigbeeSubsystemRemovePrematureClusterCommand(uint64_t eui64, uint8_t commandId);

/*
 * Cleanup any unused firmware files
 */
void zigbeeSubsystemCleanupFirmwareFiles(void);

/*
 * Create (if necessary) the directory where firmware files are stored, and return the path.
 * Caller is responsible for freeing the result
 */
char *zigbeeSubsystemGetAndCreateFirmwareFileDirectory(DeviceFirmwareType firmwareType);

/*
 * Create a IcDiscoveredDeviceDetails object
 */
IcDiscoveredDeviceDetails *createIcDiscoveredDeviceDetails(void);

/*
 * Create a clone of the provided IcDiscoveredDeviceDetails.  Caller frees the result.
 */
IcDiscoveredDeviceDetails *cloneIcDiscoveredDeviceDetails(const IcDiscoveredDeviceDetails *original);

/*
 * Free a IcDiscoveredDeviceDetails instance
 */
void freeIcDiscoveredDeviceDetails(IcDiscoveredDeviceDetails *details);

/*
 * Return a cJSON representation of the provided IcDiscoveredDeviceDetails.
 * Caller must free with cJSON_Delete.
 */
cJSON *icDiscoveredDeviceDetailsToJson(const IcDiscoveredDeviceDetails *details);

/*
 * Return a IcDiscoveredDeviceDetails parsed from the provided cJSON.
 * Caller must free with freeIcDiscoveredDeviceDetails.
 */
IcDiscoveredDeviceDetails *icDiscoveredDeviceDetailsFromJson(const cJSON *detailsJson);

/*
 * Return true if the provided device details has the server cluster and attribute and set the
 * endpoint id in the provided out value.  If endpointId is NULL, this can simply check to see
 * if the device has the cluster and attribute at all.
 */
bool icDiscoveredDeviceDetailsGetAttributeEndpoint(const IcDiscoveredDeviceDetails *details,
                                                   uint16_t clusterId,
                                                   uint16_t attributeId,
                                                   uint8_t *endpointId);

/*
 * Return true if the provided device details has the server cluster and set the
 * endpoint id in the provided out value.  If endpointId is NULL, this can simply check to see
 * if the device has the cluster at all.
 */
bool icDiscoveredDeviceDetailsGetClusterEndpoint(const IcDiscoveredDeviceDetails *details,
                                                 uint16_t clusterId,
                                                 uint8_t *endpointId);

/*
 * Return true if the provided device details has the specified cluster on the specified endpoint.
 */
bool icDiscoveredDeviceDetailsEndpointHasCluster(const IcDiscoveredDeviceDetails *details,
                                                 uint8_t endpointId,
                                                 uint16_t clusterId,
                                                 bool wantServer);

/*
 * Return true if the provided device details has the specified attribute on the specified cluster on the specified
 * endpoint.
 */
bool icDiscoveredDeviceDetailsClusterHasAttribute(const IcDiscoveredDeviceDetails *details,
                                                  uint8_t endpointId,
                                                  uint16_t clusterId,
                                                  bool wantServer,
                                                  uint16_t attributeId);

/**
 * Fetch an attribute value if it exists
 * @param details the details
 * @param endpointId the endpoint id
 * @param clusterId the cluster id
 * @param wantServer true if server cluster, false otherwise
 * @param attributeId the attributeid
 * @param attributeValue the value to be returned
 * @return true if found, false otherwise
 */
bool icDiscoveredDeviceDetailsClusterGetAttributeValue(const IcDiscoveredDeviceDetails *details,
                                                       uint8_t endpointId,
                                                       uint16_t clusterId,
                                                       bool wantServer,
                                                       uint16_t attributeId,
                                                       IcDiscoveredAttributeValue **attributeValue);

/**
 * Fetch the Device ID for the specified endpoint
 * @param details the details
 * @param endpointId the endpoint id
 * @param attributeValue the value to be returned
 * @return true if found, false otherwise
 */
bool icDiscoveredDeviceDetailsEndpointGetDeviceId(const IcDiscoveredDeviceDetails *details,
                                                  uint8_t endpointId,
                                                  uint16_t *deviceId);


/*
 * Debug print the provided details
 */
void zigbeeSubsystemDumpDeviceDiscovered(IcDiscoveredDeviceDetails *details);

/*
 * Get the zigbee system status.
 *
 * Return 0 on success
 */
int zigbeeSubsystemGetSystemStatus(zhalSystemStatus *status);

/*
 * Retrieve the currently supported zigbee stack counters as a JSON object.  This also clears/resets
 * the counters.
 *
 * @return non-null cJSON pointer on success (caller frees with cJSON_Delete)
 */
cJSON *zigbeeSubsystemGetAndClearCounters(void);

/*
 * Attempt to asynchronously change the zigbee channel
 *
 * @param channel the channel to change to
 * @param dryRun if true, dont actually change the channel
 *
 * @return a ChannelChnageResponse value
 */
ChannelChangeResponse zigbeeSubsystemChangeChannel(uint8_t channel, bool dryRun);

/*
 * Populate the zigbee network map
 * @return linked list of ZigbeeSubsystemNetworkMapEntry
 */
icLinkedList *zigbeeSubsystemGetNetworkMap(void);

/*
 * Initiate firmware upgrade of a remote device that uses the 'legacy' Ember
 * bootload mechanism.
 *
 * @param eui64
 *            the EUI64 of the remote device
 * @param routerEui64
 *            the proxy node for a multi-hop upgrade. It is the god-parent
 *            if the god-parent is not the coordinator.
 * @param appFilename
 *            the filename of the firmware image
 * @param bootloaderFilename
 *            the filename of the bootloader image to use (or NULL for none)
 *
 * @return true on success
 */
bool zigbeeSubsystemUpgradeDeviceFirmwareLegacy(uint64_t eui64,
                                                uint64_t routerEui64,
                                                const char *appFilename,
                                                const char *bootloaderFilename);

IcDiscoveredDeviceDetails *zigbeeSubsystemDiscoverDeviceDetails(uint64_t eui64);

/*
 * Get the zigbee module's firmware version.
 * @return the firmware version, or NULL on failure.  Caller must free.
 */
char *zigbeeSubsystemGetFirmwareVersion(void);

/*
 * Set whether unknown devices are reject and told to go away if they send something to us
 */
void zigbeeSubsystemSetRejectUnknownDevices(bool doReject);

/*
 * Tell the Zigbee Subsystem that a related property has changed
 */
void zigbeeSubsystemHandlePropertyChange(const char *prop, const char *value);

/*
 * Perform an energy scan
 *
 * @param channelsToScan
 *              an array of channel numbers to scan
 * @param numChannelsToScan
 *              the number of channels to scan
 * @param scanDurationMillis
 *              the duration in milliseconds per scan
 * @param numScans
 *              the number of scans per channel
 *
 * @return a linked list of zhalEnergyScanResults
 */

icLinkedList *zigbeeSubsystemPerformEnergyScan(const uint8_t *channelsToScan,
                                               uint8_t numChannelsToScan,
                                               uint32_t scanDurationMillis,
                                               uint32_t numScans);

/*
 * Notify zigbeeSubsystem that a zigbee device went into comm fail
 *
 * @param device
 */
void zigbeeSubsystemNotifyDeviceCommFail(icDevice *device);


/*
 * Notify zigbeeSubsystem that a zigbee device communication restored.
 *
 */
void zigbeeSubsystemNotifyDeviceCommRestored(icDevice *device);
/*
 * Determine a link quality value given rssi and lqi values.
 * prevLinkQuality is a hint to the algorithm for what we had before this latest check or LINK_QUALITY_LEVEL_UNKNOWN
 * if we dont have a previous value.
 */
const char *zigbeeSubsystemDetermineLinkQuality(int8_t neRssi,
                                                int8_t feRssi,
                                                uint8_t neLqi,
                                                uint8_t feLqi,
                                                ZigbeeSubsystemLinkQualityLevel qualityHint);

/*
 * Request that a device leave the network.
 */
bool zigbeeSubsystemRequestDeviceLeave(uint64_t eui64, bool withRejoin, bool isEndDevice);

/*
 * Notify zigbeeSubsystem that a device has announced
 * @param eui64
 * @param deviceType
 * @param powerSource
 */
void zigbeeSubsystemDeviceAnnounced(uint64_t eui64, zhalDeviceType deviceType, zhalPowerSource powerSource);

/*
 * Convert string formatted linkQuality to enum
 * @param linkQuality
 * return linkQuality enum value
 */
ZigbeeSubsystemLinkQualityLevel zigbeeSubsystemLinkQualityStringToEnum(const char *linkQuality);

#endif // FLEXCORE_ZIGBEESUBSYSTEM_H
