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
 * Created by Thomas Lea on 3/14/16.
 */

#ifndef FLEXCORE_ZHAL_H
#define FLEXCORE_ZHAL_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <cjson/cJSON.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>

#define MAX_CLUSTERS_PER_ENDPOINT            255

#define REPORTING_INTERVAL_MAX               0xFFFE
#define REPORTING_INTERVAL_TWENTY_SEVEN_MINS 0x654

typedef int ZHAL_STATUS;

#define PRIzHAL                              "d"

#define ZHAL_STATUS_OK                       0
#define ZHAL_STATUS_FAIL                     -1
#define ZHAL_STATUS_INVALID_ARG              -2
#define ZHAL_STATUS_NOT_IMPLEMENTED          -3
#define ZHAL_STATUS_TIMEOUT                  -4
#define ZHAL_STATUS_OUT_OF_MEMORY            -5
#define ZHAL_STATUS_MESSAGE_DELIVERY_FAILED  -6
#define ZHAL_STATUS_NETWORK_BUSY             -7
#define ZHAL_STATUS_NOT_READY                -8
#define ZHAL_STATUS_LPM                      -9

#define ZHAL_DESTINATION_ADDRESS_MODE_DEVICE 0x03
#define ZHAL_DESTINATION_ADDRESS_MODE_GROUP  0x01

#define ZHAL_RESPONSE_TYPE_ATTRIBUTES_READ   "attributesReadResponse"
#define ZHAL_RESPONSE_TYPE_SEND_COMMAND      "sendCommandResponse"

typedef enum
{
    deviceTypeUnknown,
    deviceTypeEndDevice,
    deviceTypeRouter
} zhalDeviceType;

typedef enum
{
    powerSourceUnknown,
    powerSourceMains,
    powerSourceBattery
} zhalPowerSource;

// Free with freeReceivedAttributeReport()
typedef struct
{
    uint64_t eui64;
    uint8_t sourceEndpoint;
    uint16_t clusterId;
    uint8_t *reportData;
    uint16_t reportDataLen;
    int8_t rssi;
    uint8_t lqi;
    uint16_t mfgId;
} ReceivedAttributeReport;

// Free with freeReceivedClusterCommand()
typedef struct
{
    uint64_t eui64;
    uint8_t sourceEndpoint;
    uint16_t profileId;
    uint16_t clusterId;
    bool fromServer;
    uint8_t commandId;
    bool mfgSpecific;
    uint16_t mfgCode;
    uint8_t seqNum;
    uint8_t apsSeqNum;
    int8_t rssi;
    uint8_t lqi;
    uint8_t *commandData;
    uint16_t commandDataLen;
} ReceivedClusterCommand;

typedef enum
{
    piezoToneNone,
    piezoToneWarble,
    piezoToneFire,
    piezoToneCo,
    piezoToneHighFreq,
    piezoToneLowFreq
} zhalPiezoTone;

typedef enum
{
    ZHAL_OTA_INVALID_EVENT,
    ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_STARTED_EVENT,
    ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_FAILED_EVENT,
    ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_COMPLETED_EVENT,
    ZHAL_OTA_IMAGE_NOTIFY_EVENT,
    ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT,
    ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT,
    ZHAL_OTA_UPGRADE_STARTED_EVENT,
    ZHAL_OTA_IMAGE_BLOCK_REQUEST_EVENT,
    ZHAL_OTA_IMAGE_BLOCK_RESPONSE_EVENT,
    ZHAL_OTA_UPGRADE_END_REQUEST_EVENT,
    ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT
} zhalOtaEventType;

typedef struct
{
    uint64_t eui64;
    uint64_t timestamp;
    zhalOtaEventType eventType;
    uint8_t *buffer;
    uint16_t bufferLen;
    ZHAL_STATUS *sentStatus;
} OtaUpgradeEvent;

typedef struct
{
    void (*startup)(void *ctx);

    void (*deviceAnnounced)(void *ctx, uint64_t eui64, zhalDeviceType deviceType, zhalPowerSource powerSource);

    void (*deviceJoined)(void *ctx, uint64_t eui64);

    void (*deviceLeft)(void *ctx, uint64_t eui64);

    void (*deviceRejoined)(void *ctx, uint64_t eui64, bool isSecure);

    void (*linkKeyUpdated)(void *ctx, uint64_t eui64, bool isUsingHashBasedKey);

    void (*apsAckFailure)(void *ctx, uint64_t eui64);

    // callback must free report with freeReceivedAttributeReport()
    void (*attributeReportReceived)(void *ctx, ReceivedAttributeReport *report);

    // callback must free report with freeReceivedClusterCommand()
    void (*clusterCommandReceived)(void *ctx, ReceivedClusterCommand *command);

    // callback must free ota message event with freeOtaUpgradeEvent()
    void (*deviceOtaUpgradeMessageSent)(void *ctx, OtaUpgradeEvent *otaEvent);

    // callback must free ota message event with freeOtaUpgradeEvent()
    void (*deviceOtaUpgradeMessageReceived)(void *ctx, OtaUpgradeEvent *otaEvent);

    void (*deviceCommunicationSucceeded)(void *ctx, uint64_t eui64);

    void (*deviceCommunicationFailed)(void *ctx, uint64_t eui64);

    void (*networkConfigChanged)(void *ctx, char *networkConfigData);

    void (*networkHealthProblem)(void *ctx);

    void (*networkHealthProblemRestored)(void *ctx);

    void (*panIdAttackDetected)(void *ctx);

    void (*panIdAttackCleared)(void *ctx);

    void (*beaconReceived)(void *ctx,
                           uint64_t eui64,
                           uint16_t panId,
                           bool isOpen,
                           bool hasEndDeviceCapacity,
                           bool hasRouterCapability,
                           uint8_t depth);

} zhalCallbacks;

typedef struct
{
    bool networkIsUp;
    bool networkIsOpenForJoin;
    uint64_t eui64;
    uint64_t originalEui64;
    uint8_t channel;
    uint16_t panId;
    uint8_t networkKey[16];
    char version[64];
} zhalSystemStatus;

typedef struct
{
    uint8_t endpointId;
    uint16_t appProfileId;
    uint16_t appDeviceId;
    uint8_t appDeviceVersion;
    uint16_t serverClusterIds[MAX_CLUSTERS_PER_ENDPOINT];
    uint8_t numServerClusterIds;
    uint16_t clientClusterIds[MAX_CLUSTERS_PER_ENDPOINT];
    uint8_t numClientClusterIds;
} zhalEndpointInfo;

typedef struct
{
    uint16_t id;
    uint8_t type;
} zhalAttributeInfo;

typedef struct
{
    zhalAttributeInfo attributeInfo;
    uint8_t *data; // null if the read for this attribute failed
    uint16_t dataLen;
} zhalAttributeData;

typedef struct
{
    zhalAttributeInfo attributeInfo;
    uint16_t minInterval;
    uint16_t maxInterval;
    uint64_t reportableChange;
} zhalAttributeReportingConfig;

typedef struct
{
    uint8_t channel;
    uint16_t panId;
    uint8_t networkKey[16];
} zhalNetworkChangeRequest;

typedef struct
{
    uint64_t eui64;
    uint8_t lqi;
} zhalLqiData;

typedef enum
{
    MESSAGE_HANDLING_NORMAL = 0,
    MESSAGE_HANDLING_IGNORE_ALL,
    MESSAGE_HANDLING_PASSTHRU_ALL
} zhalMessageHandlingType;

typedef struct
{
    uint64_t eui64;
    int32_t timeoutSeconds;
    zhalMessageHandlingType messageHandling;
} zhalLpmMonitoredDeviceInfo;

typedef struct
{
    uint8_t channel;
    int8_t maxRssi;
    int8_t minRssi;
    int8_t averageRssi;
    uint32_t score;
} zhalEnergyScanResult;

typedef struct
{
    uint64_t sourceAddress;
    uint8_t sourceEndpoint;
    uint16_t clusterId;
    uint8_t destinationAddressMode;

    union
    {
        struct
        {
            uint64_t eui64;
            uint8_t endpoint;
        } extendedAddress;

        uint16_t group;

    } destination;

} zhalBindingTableEntry;

typedef struct
{
    uint64_t eui64;
    union
    {
        // This bitfield must be kept in sync with xNCP
        struct
        {
            uint8_t ignoreMessages                :1;
            uint8_t passAllMessages               :1;
            uint8_t ignoreTimer                   :1;
            uint8_t isAutoApsAcked                :1;
            uint8_t useHashBasedLinkKey           :1;
            uint8_t doNotUpgradeToHashBasedLinkKey:1;
            uint8_t unused                        :2;
        } bits;
        uint8_t byte;
    } flags;

} zhalDeviceEntry;

typedef struct
{
    uint16_t nodeId;
    char *strEui64;
} zhalAddressTableEntry;

typedef void (*zhalResponseHandler)(const char *responseType, ZHAL_STATUS resultCode);

/*
 * Initialize the zhal library and connect to the specified zigbee service.
 *
 * @param host ip address of host
 * @param port port to connect to zigbee service
 * @param callbacks functions to be called for events handling
 * @param callbackContext context to be provided to callbacks
 * @param handler function to be called only on valid responses from zigbee service for our requests.
 * @return 0 on success
 */
int zhalInit(const char *host, int port, zhalCallbacks *callbacks, void *callbackContext, zhalResponseHandler handler);

/*
 * Initialize the zigbee network using the provided EUI64 and the blob of previously stored
 * opaque network configuration data.
 *
 * @param eui64 our locally generated EUI64 for this device
 * @param region the ISO 3166 2 character region code to use for this network, or NULL for default
 * @param networkConfigData an opaque blob of data that is required for zigbee network operation or NULL if there isnt
 * any yet
 * @param properties a hash map of all of the custom properties needed to start zigbee network or NULL if there are no
 * properties to add
 *
 * @return 0 on success
 */
int zhalNetworkInit(uint64_t eui64, const char *region, const char *networkConfigData, icStringHashMap *properties);

/*
 * Stop the zigbee network. ZigbeeCore will behave as if the network is not initialized
 * once this request is processed.
 * @see @ref zhalNetworkInit to resume normal operations.
 * @return 0 on success
 */
int zhalNetworkTerm(void);

/*
 * Retrieve the current status of the ZigBee system into the provided status struct.
 *
 * @return 0 on success
 */
int zhalGetSystemStatus(zhalSystemStatus *status);

/*
 * Open the network for joining (any type and number of devices).
 *
 * @return 0 on success
 */
int zhalNetworkEnableJoin(void);

/*
 * Close the network for joining.
 *
 * @return 0 on success
 */
int zhalNetworkDisableJoin(void);

/*
 * Retrieve the available endpoint IDs from the target device.
 *
 * Caller frees non-null value returned in endpointIds.
 *
 * @return 0 on success
 */
int zhalGetEndpointIds(uint64_t eui64, uint8_t **endpointIds, uint8_t *numEndpointIds);

/*
 * Retrieve the details of an endpoint.
 *
 * @return 0 on success
 */
int zhalGetEndpointInfo(uint64_t eui64, uint8_t endpointId, zhalEndpointInfo *info);

/*
 * Get the details of attributes on a target device's cluster.
 *
 * Caller frees non-null value returned in infos.
 *
 * @return 0 on success
 */
int zhalGetAttributeInfos(uint64_t eui64,
                          uint8_t endpointId,
                          uint16_t clusterId,
                          bool toServer,
                          zhalAttributeInfo **infos,
                          uint16_t *numInfos);

/*
 * Read one or more attributes from an endpoint's client/server cluster.
 *
 * The attributeData argument must be pre-allocated by the caller and have the same number of elements as the
 * attributeIds input.
 *
 * If any of the requested attributes fail to read, the entire request will fail.
 *
 * NOTE: The order of entries in the attributeData result may not match the order of the attributeIds input.
 *
 * NOTE: The caller must free the non-null data elements in each returned attributeData entry.
 *
 * @return 0 on success
 */
int zhalAttributesRead(uint64_t eui64,
                       uint8_t endpointId,
                       uint16_t clusterId,
                       bool toServer,
                       const uint16_t *attributeIds,
                       uint8_t numAttributeIds,
                       zhalAttributeData *attributeData);

/*
 * Read one or more manufacturer specific attributes from an endpoint's client/server cluster.
 *
 * The attributeData argument must be pre-allocated by the caller and have the same number of elements as the
 * attributeIds input.
 *
 * If any of the requested attributes fail to read, the entire request will fail.
 *
 * NOTE: The order of entries in the attributeData result may not match the order of the attributeIds input.
 *
 * NOTE: The caller must free the non-null data elements in each returned attributeData entry.
 *
 * @return 0 on success
 */
int zhalAttributesReadMfgSpecific(uint64_t eui64,
                                  uint8_t endpointId,
                                  uint16_t clusterId,
                                  uint16_t mfgId,
                                  bool toServer,
                                  const uint16_t *attributeIds,
                                  uint8_t numAttributeIds,
                                  zhalAttributeData *attributeData);

/*
 * Write one or more attributes to an endpoint's client/server cluster.
 *
 * @return 0 on success
 */
int zhalAttributesWrite(uint64_t eui64,
                        uint8_t endpointId,
                        uint16_t clusterId,
                        bool toServer,
                        zhalAttributeData *attributeData,
                        uint8_t numAttributes);

/*
 * Write one or more manufacturer specific attributes to an endpoint's client/server cluster.
 *
 * @return 0 on success
 */
int zhalAttributesWriteMfgSpecific(uint64_t eui64,
                                   uint8_t endpointId,
                                   uint16_t clusterId,
                                   uint16_t mfgId,
                                   bool toServer,
                                   zhalAttributeData *attributeData,
                                   uint8_t numAttributes);

/*
 * Create a binding between us and a remote device.
 *
 * @return 0 on success
 */
int zhalBindingSet(uint64_t eui64, uint8_t endpointId, uint16_t clusterId);

/*
 * Create a binding between two devices.
 *
 * @return 0 on success
 */
int zhalBindingSetTarget(uint64_t eui64,
                         uint8_t endpointId,
                         uint64_t targetEui64,
                         uint8_t targetEndpointId,
                         uint16_t clusterId);

/*
 * Retrieve the bindings for a device.
 *
 * @return linked list of zhalBindingTableEntrys on success, NULL on failure.
 */
icLinkedList *zhalBindingGet(uint64_t eui64);

/*
 * Clear a binding between us and a remote device.
 *
 * @return 0 on success
 */
int zhalBindingClear(uint64_t eui64, uint8_t endpointId, uint16_t clusterId);

/*
 * Clear a binding between a remote device and some other target (not necessarily us).
 *
 * @return 0 on success
 */
int zhalBindingClearTarget(uint64_t eui64,
                           uint8_t endpointId,
                           uint16_t clusterId,
                           uint64_t targetEui64,
                           uint8_t targetEndpoint);

/*
 * Configure attribute reporting on a remote device.
 *
 * @return 0 on success
 */
int zhalAttributesSetReporting(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               zhalAttributeReportingConfig *configs,
                               uint8_t numConfigs);

/*
 * Configure attribute reporting on a remote device with manufacturer-specific attributes.
 *
 * @return 0 on success
 */
int zhalAttributesSetReportingMfgSpecific(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          uint16_t mfgId,
                                          zhalAttributeReportingConfig *configs,
                                          uint8_t numConfigs);

/*
 * Set the list of devices in our network along with their flags.
 *
 * @return 0 on success
 */
int zhalSetDevices(zhalDeviceEntry *devices, uint16_t numDevices);

/*
 * Remove a single Zigbee device address from those allowed on our network.
 *
 * @return 0 on success
 */
int zhalRemoveDeviceAddress(uint64_t eui64);

/*
 * Send a command to an endpoint.
 *
 * @return 0 on success
 */
int zhalSendCommand(uint64_t eui64,
                    uint8_t endpointId,
                    uint16_t clusterId,
                    bool toServer,
                    bool useApsEncryption,
                    bool useDefaultResponse,
                    uint8_t commandId,
                    uint8_t *message,
                    uint16_t messageLen);

/*
 * Send a manufacturer specific command to an endpoint.
 *
 * @return 0 on success
 */
int zhalSendMfgCommand(uint64_t eui64,
                       uint8_t endpointId,
                       uint16_t clusterId,
                       bool toServer,
                       bool useApsEncryption,
                       bool useDefaultResponse,
                       uint8_t commandId,
                       uint16_t mfgId,
                       uint8_t *message,
                       uint16_t messageLen);

/*
 * Send a response to a legacy icontrol sensor via a special aps ack
 */
int zhalSendViaApsAck(uint64_t eui64,
                      uint8_t endpointId,
                      uint16_t clusterId,
                      uint8_t sequenceNum,
                      uint8_t *message,
                      uint16_t messageLen);

/*
 * Send a leave request to a device.
 *
 * @return 0 on success
 */
int zhalRequestLeave(uint64_t eui64, bool withRejoin, bool isEndDevice);

/*
 * Shut down the zhal library.
 *
 * @return 0 on success
 */
int zhalTerm(void);

/*
 * Send a heartbeat request
 *
 * @param pid if not NULL it will be populated with the PID of the zhal process (ZigbeeCore)
 * @param initialized if not NULL it will be populated with true if zhal has been initialized
 *
 * @return 0 on success
 */
int zhalHeartbeat(pid_t *pid, bool *initialized);

/**
 * Inform that there are new OTA files available
 * @return 0 on success
 */
int zhalRefreshOtaFiles(void);

/**
 * On success, caller frees with cJSON_Delete
 * @return non-null cJSON pointer on success
 */
cJSON *zhalGetAndClearCounters(void);

/*
 * Turn on or off the piezo that is integrated with the zigbee chip.  Note not all devices connect
 * the piezo up this way (TCA20x and TCA300 do).
 *
 * @return 0 in success
 */
int zhalSetPiezoTone(zhalPiezoTone tone);

/**
 * Change the configuration of the zigbee network.  Pass 0 for channel, panId, or networkKey to indicate that you
 * don't want that value to change.
 *
 * @return 0 on success
 */
int zhalNetworkChange(zhalNetworkChangeRequest *networkChangeRequest);

/**
 * Check if the provided eui64 is a child of ours
 *
 * @return true if its a child, false otherwise
 */
bool zhalDeviceIsChild(uint64_t eui64);

/**
 * Get the source route for the given eui64
 *
 * @return linked list of eui64s as uint64_t, or NULL if fails
 */
icLinkedList *zhalGetSourceRoute(uint64_t eui64);

/**
 * Get the lqi table for the given eui64
 *
 * @return linked list of zhalLqiData or NULL if fails
 */
icLinkedList *zhalGetLqiTable(uint64_t eui64);

/**
 * Get the monitored devices info
 *
 * @return linked list of zhalLpmMonitoredDeviceInfo or NULL if fails
 */
icLinkedList *zhalGetMonitoredDevicesInfo(void);

/**
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
 * @return 0 on success
 */
int zhalUpgradeDeviceFirmwareLegacy(uint64_t eui64,
                                    uint64_t routerEui64,
                                    const char *appFilename,
                                    const char *bootloaderFilename);

/**
 * Tells Zigbee core that system needs to go into LPM
 * passes the list of LPMMonitoredDeviceInfo objects
 *
 * @param deviceList - the list of devices to wake up LPM
 * @return 0 on success
 */
int zhalEnterLowPowerMode(icLinkedList *deviceList);

/**
 * Tells Zigbee core that systems is now out of low power mode
 *
 * @return 0 on success
 */
int zhalExitLowPowerMode(void);

/**
 * Set a device communication lost timeout for Zigbee
 *
 * @param timeoutSeconds - number of seconds before device is considered to be in comm failure
 * @return 0 on success
 */
int zhalSetCommunicationFailTimeout(uint32_t timeoutSeconds);

/*
 * Create a copy of the ReceivedAttributeReport
 */
ReceivedAttributeReport *cloneReceivedAttributeReport(ReceivedAttributeReport *report);

/*
 * Creates a copy of the ReceivedClusterCommand
 */
ReceivedClusterCommand *cloneReceivedClusterCommand(ReceivedClusterCommand *command);

/*
 * Free a ReceivedAttributeReport
 */
void freeReceivedAttributeReport(ReceivedAttributeReport *report);

/*
 * Return a copy of the provided ReceivedAttributeReport
 */
ReceivedAttributeReport *receivedAttributeReportClone(const ReceivedAttributeReport *report);

/*
 * Free a ReceivedClusterCommand
 */
void freeReceivedClusterCommand(ReceivedClusterCommand *command);

/*
 * Return a copy of the provided ReceivedClusterCommand
 */
ReceivedClusterCommand *receivedClusterCommandClone(const ReceivedClusterCommand *command);

/*
 * Free an OtaUpgradeEvent
 */
void freeOtaUpgradeEvent(OtaUpgradeEvent *otaEvent);

/*
 * Check if the provided endpoint has the provided server cluster
 *
 * @return true if the endpoint has a server cluster with the provided id
 */
bool zhalEndpointHasServerCluster(zhalEndpointInfo *endpointInfo, uint16_t clusterId);

/**
 * Send ZigbeeCore a message to add and recognize this eui64 as a cell data UART.
 *
 * @return status
 */
int zhalAddZigbeeUart(uint64_t eui64, uint8_t endpointId);


/**
 * Send ZigbeeCore a message to tear down the cell data UART.
 *
 * @return status
 */
int zhalRemoveZigbeeUart(uint64_t eui64);

/**
 * Send a UART "resync" command to the port located at the given endpoint.
 * While this is being processed, any data written to the attached socat tty will be momentarily
 * held for processing post-reset.
 *
 * @note Closing the tty before resetting the port is highly recommended.
 *
 * @param eui64 Device long address
 * @param endpointId Device UART endpoint to reset
 * @return request status
 */
int zhalSyncZigbeeUart(uint64_t eui64, uint8_t endpointId);

/**
 * Send ZigbeeCore a message to set devices' OTA firmware upgrade delay
 *
 * @param delaySeconds Time in seconds
 *
 * @return status
 */
int zhalSetOtaUpgradeDelay(uint32_t delaySeconds);

/**
 * Get the firmware version
 * @return the firmware version, or NULL on failure.  Caller must free.
 */
char *zhalGetFirmwareVersion(void);

/**
 * Perform an energy scan on the provided list of channels.
 *
 * @param channelsToScan the list of channels to scan
 * @param numChannelsToScan the number of channels in channelsToScan
 * @param scanDurationMillis the duration in milliseconds of each scan
 * @param numScans the number of scans to perform
 *
 * @return a linked list of scan results
 */
icLinkedList *zhalPerformEnergyScan(const uint8_t *channelsToScan,
                                    uint8_t numChannelsToScan,
                                    uint32_t scanDurationMillis,
                                    uint32_t numScans);

/**
 * Increment the NONCE and frame counters for the Zigbee network
 *
 * @param nonce amount to increment the NONCE counter
 * @param frame amount to increment the frame counter
 * @return
 */
bool zhalIncrementNetworkCounters(int32_t nonce, int32_t frame);

/**
 * Configure (start or stop) monitoring the ZigBee network for its health.
 *
 * @param intervalMillis number of milliseconds between checks or zero to disable
 * @param ccaThreshold the maximum energy allowed on the channel during health checks before declaring a CCA failure
 * @param ccaFailureThreshold number of concurrent CCA failures that must occur before determining there is a problem
 * @param restoreThreshold number of concurrent CCA successes after a failure that must occur before determining there
 * is no longer a problem
 * @param delayBetweenThresholdRetriesMillis number of milliseconds between concurrent checks after detecting our first
 * CCA failure
 *
 * @return true on success
 */
bool zhalConfigureNetworkHealthCheck(uint32_t intervalMillis,
                                     int8_t ccaThreshold,
                                     uint32_t ccaFailureThreshold,
                                     uint32_t restoreThreshold,
                                     uint32_t delayBetweenThresholdRetriesMillis);

/**
 * Configure the ZigbeeNetwork defender, which is responsible for detecting and preventing malicious attacks via Zigbee.
 *
 * @param panIdChangeThreshold the number of PAN ID changes we will allow within the time window before declaring an
 * active attack.  A value of zero disables PAN ID attack detection.
 * @param panIdChangeWindowMillis the number of milliseconds in the window for PAN ID attack detection.
 * @param panIdChangeRestoreMillis the number of milliseconds that must expire without a PAN ID change after an attack
 * has been detected before the attack is determined to be over.
 *
 * @return true on success
 */
bool zhalDefenderConfigure(uint8_t panIdChangeThreshold,
                           uint32_t panIdChangeWindowMillis,
                           uint32_t panIdChangeRestoreMillis);

/**
 * Set a stack property.
 *
 * @param key
 * @param value
 * @return true on success
 */
bool zhalSetProperty(const char *key, const char *value);

#ifdef __cplusplus
}
#endif // __cplusplus

/**
 * Perform a Zigbee test.  Currently this attempts to interact with a special device to test transmit and receive
 * but in the future this can be extended to run various other Zigbee related tests.
 *
 * @return a JSON structure containing the test results or NULL upon failure
 */
char *zhalTest(void);

/**
 * Get address table of Zigbee network
 * @return linked list of address table, which is to be freed
 * by calling linkedListDestroy(list, (linkedListItemFreeFunc) zhalAddressTableEntryDestroy)
 */
icLinkedList *zhalGetAddressTable(void);

/**
 * Helper function to be provided to linkedListDestroy function
 * to delete entry of address table list
 */
void zhalAddressTableEntryDestroy(zhalAddressTableEntry *entry);

#endif // FLEXCORE_ZHAL_H
