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

#ifndef FLEXCORE_DEVICESERVICE_H
#define FLEXCORE_DEVICESERVICE_H

#include "device-service-client.h"
#include "deviceServiceStatus.h"
#include "events/device-service-storage-changed-event.h"
#include <device/icDevice.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <deviceDescriptors.h>
#include <deviceServiceProps.h>
#include <icTypes/icLinkedList.h>

// TODO: These three shouldn't be public, just moving them here now for compiling deviceServiceMain on the zilker side
bool deviceServiceInitialize(BDeviceServiceClient *service);
bool deviceServiceStart(void);
void deviceServiceShutdown();
void allServicesAvailableNotify(void);

typedef enum
{
    POWER_EVENT_AC_LOST,
    POWER_EVENT_AC_RESTORED,
    POWER_EVENT_LPM_ENTER,
    POWER_EVENT_LPM_EXIT
} DeviceServiceSystemPowerEventType;

static const char *const DeviceServiceSystemPowerEventTypeLabels[] = {
    "AC_LOST",
    "AC_RESTORED",
    "LPM_ENTER",
    "LPM_EXIT",
};

/*
 * Restore configuration data from another directory.  This is the standard RMA restore case.
 *
 * @param tempRestoreDir the directory containing the config files to restore from
 *
 * @returns true on success
 */
bool deviceServiceRestoreConfig(const char *tempRestoreDir);

/*
 * For each of the provided device classes, find registered device drivers and instruct them to start discovery.
 *
 * @param deviceClasses - a list of device classes to discover
 * @param filters - an optional list of device filters to perform the filter check once device discoverd
 * @param timeoutSeconds - the number of seconds to perform discovery before automatically stopping
 * @param findOrphanedDevices - if true, limit discovery to orphaned previously paired devices
 *
 * @returns true if at least one device driver successfully started discovery.
 */
bool deviceServiceDiscoverStart(icLinkedList *deviceClasses,
                                icLinkedList *filters,
                                uint16_t timeoutSeconds,
                                bool findOrphanedDevices);

/*
 * Stop any discovery in progress.
 *
 * @param deviceClasses list of device classes to stop discovery for or NULL to stop all discovery
 *
 * @returns true if discovery stops successfully.
 */
bool deviceServiceDiscoverStop(icLinkedList *deviceClasses);

/*
 * Commission a specific device as specified in the provided setup payload.  This setup payload is initially
 * CHIP compliant.
 *
 * @param setupPayload - the payload with the details required to locate and commission the device
 * @param timeoutSeconds - the number of seconds to perform discovery before automatically stopping
 * @returns true if commissioning process is started
 */
bool deviceServiceCommissionDevice(const char *setupPayload, uint16_t timeoutSeconds);

/*
 * Add an already commissioned matter device identified by NodeId.  This is typically used when another
 * device performs the commissioning of a device onto our fabric and is just notifying us that it is
 * available for configuration and use.  Standard device service device events are emitted if/when
 * found.
 *
 * @param nodeId - the nodeId of device to add
 * @param timeoutSeconds - the number of seconds to attempt to locate before automatically stopping
 * @returns true if adding process is started
 */
bool deviceServiceAddMatterDevice(uint64_t nodeId, uint16_t timeoutSeconds);

/*
 * Open a commissioning window locally or for a specific device. When successful, the generated setup code and
 * QR code are returned.  The caller is responsible for freeing the setupCode and qrCode.
 *
 * @param nodeId - the nodeId of the device to open the commissioning window for, or NULL for local
 * @param timeoutSeconds - the number of seconds to perform discovery before automatically stopping or 0 for default
 * @param setupCode - receives the setup code if successful (caller frees)
 * @param qrCode - receives the QR code if successful (caller frees)
 * @returns true if commissioning window is opened
 */
bool deviceServiceOpenCommissioningWindow(const char *nodeId, uint16_t timeoutSeconds, char **setupCode, char **qrCode);

/*
 * Determine if any device discovery is currently running.
 *
 * @return true if any discovery is running
 */
bool deviceServiceIsDiscoveryActive(void);

/*
 * Determine if we are in recovery mode or not
 *
 * @return true if we are in device recovery mode
 */
bool deviceServiceIsInRecoveryMode(void);

/*
 * Retrieve a list of all icDevices in the system.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @returns a linked list of all icDevices in the system.
 */
icLinkedList *deviceServiceGetAllDevices(void);

/*
 * Retrieve a list of devices that contains the metadataId or
 * contains the metadataId value that is equal to valueToCompare
 *
 * If valueToCompare is NULL, will only look if the metadata exists.
 * Otherwise will only add devices that equal the metadata Id and it's value.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @return - linked list of icDevices with found metadata, or NULL if error occurred
 */
icLinkedList *deviceServiceGetDevicesByMetadata(const char *metadataId, const char *valueToCompare);

/*
 * Retrieve an icDevice by its universally unique identifier.
 *
 * Caller is responsible for calling deviceDestroy() the non-NULL returned device.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns the icDevice matching the uuid or NULL if not found.
 */
icDevice *deviceServiceGetDevice(const char *uuid);

/*
 * Check if the provided device uuid is known to device service.  This is meant to be fast/efficient.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns true if the device is known
 */
bool deviceServiceIsDeviceKnown(const char *uuid);

/*
 * Check if the provided device uuid is denylisted by CPE property.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns true if the device is denylisted
 */
bool deviceServiceIsDeviceDenylisted(const char *uuid);

/*
 * Remove a device by uuid.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns true on success
 */
bool deviceServiceRemoveDevice(const char *uuid);

/*
 * Retrieve an icDevice by URI.
 *
 * Caller is responsible for calling deviceDestroy() the non-NULL result
 *
 * @param uri - the URI of the device
 *
 * @returns the icDevice or NULL if not found
 */
icDevice *deviceServiceGetDeviceByUri(const char *uri);

/*
 * Retrieve a list of icDevices that have at least one endpoint supporting the provided profile.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param profileId - the profile id for the lookup
 *
 * @returns a linked list of icDevices that have at least one endpoint supporting the provided profile.
 */
icLinkedList *deviceServiceGetDevicesByProfile(const char *profileId);

/*
 * Retrieve a list of icDevices that have the provided device class
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param deviceClass - the device class for the lookup
 *
 * @returns a linked list of icDevices that support the provided class.
 */
icLinkedList *deviceServiceGetDevicesByDeviceClass(const char *deviceClass);

/*
 * Retrieve a list of icDevices that have the provided device driver
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param deviceDriver - the device driver for the lookup
 *
 * @returns a linked list of icDevices that use the provided driver.
 */
icLinkedList *deviceServiceGetDevicesByDeviceDriver(const char *deviceDriver);

/*
 * Retrieve a list of icDevices that belong to the provided subsystem.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param subsystem - the name of the subsystem
 *
 * @returns a linked list of icDevices that belong to the provided subsystem
 */
icLinkedList *deviceServiceGetDevicesBySubsystem(const char *subsystem);

/*
 * Retrieve a list of icDeviceEndpoints that support the provided profile.
 *
 * Caller is responsible for calling endpointDestroy() on each returned endpoint.
 *
 * @param profileId - the profile id for the lookup
 *
 * @returns a linked list of icDeviceEndpoints that support the provided profile.
 */
icLinkedList *deviceServiceGetEndpointsByProfile(const char *profileId);

/*
 * Retrieve an icDeviceResource by URI.
 *
 * Caller is responsible for calling resourceDestroy() on the non-NULL result
 *
 * @param uri - the URI of the resource
 *
 * @returns the icDeviceResource or NULL if not found
 */
icDeviceResource *deviceServiceGetResourceByUri(const char *uri);

/*
 * Retrieve an icDeviceEndpoint by URI.
 *
 * Caller is responsible for calling endpointDestroy() on the non-NULL result
 *
 * @param uri - the URI of the endpoint
 *
 * @returns the icDeviceEndpoint or NULL if not found
 */
icDeviceEndpoint *deviceServiceGetEndpointByUri(const char *uri);

/*
 * Retrieve an icDeviceEndpoint by its uuid.
 *
 * Caller is responsible for calling endpointDestroy() on the non-NULL result
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the endpoint
 *
 * @returns the icDeviceEndpoint or NULL if not found
 */
icDeviceEndpoint *deviceServiceGetEndpointById(const char *deviceUuid, const char *endpointId);

/*
 * Remove an icDeviceEndpoint by its uuid.
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the endpoint
 *
 * @returns true on success
 */
bool deviceServiceRemoveEndpointById(const char *deviceUuid, const char *endpointId);

/*
 * Write a resource.
 *
 * @param uri - the URI of the resource
 * @param value - the new value
 *
 * @returns true on success
 */
bool deviceServiceWriteResource(const char *uri, const char *value);

/*
 * Execute a resource.
 *
 * @param uri - the URI of the resource
 * @param arg - optional JSON object with arguments
 * @param response - an optional response
 *
 * @returns true on success
 */
bool deviceServiceExecuteResource(const char *uri, const char *arg, char **response);

/*
 * Change the mode of a resource.
 *
 * @param uri = the URI of the resource
 * @param newMode - the new mode bits for the resource
 *
 * @returns true on success
 */
bool deviceServiceChangeResourceMode(const char *uri, uint8_t newMode);

/*
 * Retrieve a device service system property. Caller frees value output.
 *
 * @param name - the name of the property to retrieve
 * @param value - receives the value of the property if successful (caller frees)
 *
 * @returns true on success
 */
bool deviceServiceGetSystemProperty(const char *name, char **value);

/*
 * Retrieve all device service system properties. Caller frees value output.
 *
 * @param map - the map of system properties to populate
 *
 * @returns true on success
 */
bool deviceServiceGetAllSystemProperties(icStringHashMap *map);

/*
 * Set a device service system property.
 *
 * @param name - the name of the property to set
 * @param value - the value of the property to set
 *
 * @return true on success
 */
bool deviceServiceSetSystemProperty(const char *name, const char *value);

/*
 * Get the value of a metadata item at the provided URI.
 *
 * @param uri - the URI to a metadata item
 * @param value - receives the value of the metadata if successful.  Note, value may be null.  Caller frees.
 *
 * @returns true on success (even though the value may be null)
 */
bool deviceServiceGetMetadata(const char *uri, char **value);

/*
 * Set the value of a metadata item.
 *
 * @param uri - the URI of the metadata to set
 * @param value - the value of the metadata to set
 *
 * @return true on success
 */
bool deviceServiceSetMetadata(const char *uri, const char *value);

/*
 * Reload the database from storage
 *
 * @return true on success
 */
bool deviceServiceReloadDatabase(void);

/*
 * Get a device descriptor for a device
 * @param device the device to retrieve the descriptor for
 * @return the device descriptor or NULL if its not found.  Caller is responsible for calling deviceDescriptorDestroy()
 */
DeviceDescriptor *deviceServiceGetDeviceDescriptorForDevice(icDevice *device);

/*
 * Get a debug appropriate dump of our Zigbee configuration.  Caller frees.
 */
char *deviceServiceDumpZigbeeConfig(void);

/*
 * Query for metadata based on a uri pattern, currently only supported matching is with wildcards, e.g. *
 *
 * @param uriPattern the uri pattern to search with
 * @return the list of matching metadata
 */
icLinkedList *deviceServiceGetMetadataByUriPattern(char *uriPattern);

/*
 * Query for resources based on a uri pattern, currently only supported matching is with wildcards, e.g. *
 *
 * @param uriPattern the uri pattern to search with
 * @return the list of matching resources
 */
icLinkedList *deviceServiceGetResourcesByUriPattern(char *uriPattern);

/*
 * Notify the device service of system power state changes
 */
void deviceServiceNotifySystemPowerEvent(DeviceServiceSystemPowerEventType powerEvent);

/*
 * Notify the device service of property change events.
 */
void deviceServiceNotifyPropertyChange(const char *propKey, const char *propValue);

/*
 * Retrieve the current status of device service and its subsystems.  Caller destroys result with cJSON_Delete
 */
DeviceServiceStatus *deviceServiceGetStatus(void);

/*
 * Checks if a provided device is in comm fail. Will return true if the device is in comm fail, false otherwise
 * or on error.
 */
bool deviceServiceIsDeviceInCommFail(const char *deviceUuid);

/*
 * Retrieve the firmware version of a device.  Caller frees
 */
char *deviceServiceGetDeviceFirmwareVersion(const char *deviceUuid);

/*
 * Checks if a provided device was rejected.  Will return true if the device was rejected, false otherwise.
 */
bool deviceServiceWasDeviceRejected(const char *deviceUuid);

/*
 * Checks if a provided device failed to pair. Will return true if so, false otherwise.
 */
bool deviceServiceDidDeviceFailToPair(const char *deviceUuid);

#endif // FLEXCORE_DEVICESERVICE_H
