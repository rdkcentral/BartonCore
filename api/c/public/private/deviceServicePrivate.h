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
// Created by Thomas Lea on 7/27/15.
//

#ifndef FLEXCORE_DEVICESERVICEPRIVATE_H
#define FLEXCORE_DEVICESERVICEPRIVATE_H

#include "device-driver/device-driver.h"
#include "provider/device-service-token-provider.h"
#include <deviceService/securityState.h>
#include <stdbool.h>

#define CURRENT_DEVICE_DESCRIPTOR_URL "currentDeviceDescriptorUrl"
#define CURRENT_DEVICE_DESCRIPTOR_MD5 "currentDeviceDescriptorMd5"
#define CURRENT_DENYLIST_URL          "currentDenylistUrl"
#define CURRENT_DENYLIST_MD5          "currentDenylistMd5"

bool deviceServiceDeviceFound(DeviceFoundDetails *deviceFoundDetails, bool neverReject);

void deviceConfigured(icDevice *device);

typedef struct ReconfigureDeviceContext ReconfigureDeviceContext;
typedef void (*reconfigurationCompleteCallback)(bool result, const char *deviceUuid);

/*
 * Get the age (in milliseconds) since the provided resource was last updated/sync'd with the device.
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the desired endpoint
 * @param resourceId - the unique id of the desired resource
 * @param ageMillis  - the amount of time elapsed since the resource was last sync'd (valid if result is true)
 *
 * @returns true on success
 */
bool deviceServiceGetResourceAgeMillis(const char *deviceUuid,
                                       const char *endpointId,
                                       const char *resourceId,
                                       uint64_t *ageMillis);

/*
 * Retrieve an icDeviceResource by id.
 *
 * Caller is responsible for calling resourceDestroy() on the non-NULL result
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the desired endpoint
 * @param resourceId - the unique id of the desired resource
 *
 * @returns the icDeviceResource or NULL if not found
 */
icDeviceResource *deviceServiceGetResourceById(const char *deviceUuid, const char *endpointId, const char *resourceId);

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
icDeviceResource *deviceServiceFindDeviceResourceById(icDevice *device, const char *resourceId);

void updateResource(const char *deviceUuid,
                    const char *endpointId,
                    const char *resourceId,
                    const char *newValue,
                    cJSON *metadata);

void setMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *value);
char *getMetadata(const char *deviceUuid, const char *endpointId, const char *name);
bool deviceServiceRemoveMetadata(const char *deviceUuid, const char *endpointId, const char *name);
void setBooleanMetadata(const char *deviceUuid, const char *endpointId, const char *name, bool value);
bool getBooleanMetadata(const char *deviceUuid, const char *endpointId, const char *name);
cJSON *getJsonMetadata(const char *deviceUuid, const char *endpointId, const char *name);
void setJsonMetadata(const char *deviceUuid, const char *endpointId, const char *name, cJSON *value);
void deviceServiceProcessDeviceDescriptors();
void timeZoneChanged(const char *timeZone);
void processDenylistedDevices(const char *propValue);
bool addNewResource(const char *ownerUri, icDeviceResource *resource);

/**
 * Caller must call resourceOperationResultDestroy
 */

/**
 * Set devices' OTA firmware upgrade delay
 * @param delaySeconds Time in seconds
 */
void deviceServiceSetOtaUpgradeDelay(uint32_t delaySeconds);

/**
 * Add an endpoint to an existing device, persist to database and send out events.  The endpoint provided
 * must already be added to the device provided.
 * @param device
 * @param endpoint
 */
void deviceServiceAddEndpoint(icDevice *device, icDeviceEndpoint *endpoint);

/**
 * Re-enable an endpoint for a device, persist to database and send out events.
 * @param device the device
 * @param endpoint the endpoint
 */
void deviceServiceUpdateEndpoint(icDevice *device, icDeviceEndpoint *endpoint);

/*
 * Tells all of the subsystem to enter LPM
 */
void deviceServiceEnterLowPowerMode();

/*
 * Tells all of the subsystem to exit LPM
 */
void deviceServiceExitLowPowerMode();

/*
 * Determines if system is in LPM
 */
bool deviceServiceIsInLowPowerMode();

/**
 * Indicate that we have successfully communicated with the given device and update its timestamp.
 * @param deviceUuid
 */
void updateDeviceDateLastContacted(const char *deviceUuid);

/**
 * Retrieve the last contact date for a device
 *
 * @param deviceUuid
 *
 * @return the date of last contact in millis
 */
uint64_t getDeviceDateLastContacted(const char *deviceUuid);

/**
 * Retrieve a string representing the current time.
 *
 * @return a timestamp string.  Caller frees.
 */
char *getCurrentTimestampString();

/**
 * device service is ready to pair devices or not
 * @return true if devices can be paired
 */
bool deviceServiceIsReadyForPairing(void);

/**
 * device service is ready for devices operations or not
 * @return true if device service is ready for device operation
 */
bool deviceServiceIsReadyForDeviceOperation(void);
/*
 * Checks if the device needs reconfiguration
 *
 * @param deviceUuid the uuid of device
 * @return true if reconfiguration is required for the device
 */
bool deviceServiceDeviceNeedsReconfiguring(const char *deviceUuid);

/*
 * Re-configures the device Asynchronously. Rediscovers all the
 * endpoints, clusters and attributes of device and updates the
 * device parameters in JSON database.
 *
 * @param deviceUuid the uuid of device to be reconfigured
 * @param delaySeconds the delay in seconds after which reconfiguration should be scheduled
 * @param reconfigurationCompleted callback that will be triggered when reconfiguration is complete
 * @param allowAsap indicates if reconfiguration allowed as soon as possible
 */
void deviceServiceReconfigureDevice(const char *deviceUuid,
                                    uint32_t delaySeconds,
                                    reconfigurationCompleteCallback reconfigurationCompleted,
                                    bool allowAsap);

/*
 * Retrieves the post upgrade action specified for this device from its device descriptor.
 */
PostUpgradeAction deviceServiceGetPostUpgradeAction(const char *deviceUuid);

/*
 * Does a timed wait on reconfiguration condition variable.
 * This API blocks until it gets a signal on the condition
 * variable or timeout occurs.
 * @return true if the condition is signaled  within timeout
 *         else returns false in case of a timeout
 * @see deviceServiceSendReconfigurationSignal
 */
bool deviceServiceWaitForReconfigure(const char *deviceUuid);

/*
 * Sends a signal on reconfiguration condition variable.
 * @param shouldTerminate if true terminates reconfiguration task
 * @return true if signal is sent successfully
 */
bool deviceServiceSendReconfigurationSignal(const char *deviceUuid, bool shouldTerminate);

/*
 * @return true if reconfiguration is pending for the device
 */
bool deviceServiceIsReconfigurationPending(const char *deviceUuid);

/*
 * Check if reconfiguration allowed as soon as possible
 * @param deviceUuid the uuid of device to be reconfigured
 * @return true if reconfiguration allowed on device rejoin
 */
bool deviceServiceIsReconfigurationAllowedAsap(const char *deviceUuid);

/**
 * Determine if the service is to stop. Use this when
 * executing slow tasks (e.g., network I/O) to safely quit at cancellation points, e.g.,
 * just before synchronous I/O.
 * @return true when service shutdown was requested
 */
bool deviceServiceIsShuttingDown(void);

/**
 * Get the token provider for the device service. Intended for internal use only.
 * @return the token provider
 */
BDeviceServiceTokenProvider *deviceServiceGetTokenProvider(void);

#endif // FLEXCORE_DEVICESERVICEPRIVATE_H
