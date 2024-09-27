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

/*-----------------------------------------------
 * deviceEventProducer.h
 *
 * Responsible for sending internal events to API clients via GObject signals.
 *
 * Author: tlea
 *-----------------------------------------------*/

#pragma once

#include "device-service-client.h"
#include "events/device-service-device-database-failure-event.h"
#include "events/device-service-storage-changed-event.h"
#include <glib-object.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <device-driver/device-driver.h>

/*
 * Init the 'class' by wiring up the signals
 */
void deviceEventProducerClassInit(BDeviceServiceClientClass *deviceServiceClass);

/*
 * Initialize the device event producer
 */
void deviceEventProducerStartup(BDeviceServiceClient *service);

/*
 * Shut down the device event producer
 */
void deviceEventProducerShutdown(void);

/*
 * broadcast a discovery started event
 *
 * @param deviceClasses - the list of device classes for which discovery has started
 * @param timeoutSeconds - the timeout in seconds until discovery ends
 * @param findOrphanedDevices - whether we are discovering orphaned devices, or normal discovery
 */
void sendDiscoveryStartedEvent(const icLinkedList *deviceClasses, uint16_t timeoutSeconds, bool findOrphanedDevices);

/*
 * broadcast a discovery stopped event
 *
 * @param deviceClass - the device class for which discovery has stopped
 */
void sendDiscoveryStoppedEvent(const char *deviceClass);

/*
 * broadcast a recovery stopped event
 *
 * @param deviceClass - the device class for which recovery has stopped
 */
void sendRecoveryStoppedEvent(const char *deviceClass);

/*
 * broadcast a device discovered event to any listeners
 *
 * @param deviceFoundDetails - info about the device that was found
 * @param forRecovery - we are in recovery mode
 */
void sendDeviceDiscoveredEvent(const DeviceFoundDetails *deviceFoundDetails, bool forRecovery);

/*
 * broadcast a device discovery failed event to any listeners
 *
 * @param deviceFoundDetails - info about the device that failed discovery
 * @param forRecovery - we were trying to recover this device
 */
void sendDeviceDiscoveryFailedEvent(const DeviceFoundDetails *deviceFoundDetails, bool forRecovery);

/*
 * broadcast a device rejected event to any listeners
 *
 * @param deviceFoundDetails - info about the device that was found
 * @param forRecovery - we were trying to recover this device
 */
void sendDeviceRejectedEvent(const DeviceFoundDetails *deviceFoundDetails, bool forRecovery);

/*
 * broadcast a device discovery completed event to any listeners
 *
 * @param device - the device
 * @param forRecovery - we were trying to recover this device
 */
void sendDeviceDiscoveryCompletedEvent(icDevice *device, bool forRecovery);

/*
 * broadcast a device added event to any listeners
 *
 * @param uuid - the uuid of the device
 */
void sendDeviceAddedEvent(const char *uuid);

/*
 * broadcast a device recovered event to any listeners
 *
 * @param uuid - the uuid of the device
 */
void sendDeviceRecoveredEvent(const char *uuid);

/*
 * broadcast a device removed event to any listeners
 *
 * @param uuid - the uuid of the device
 * @param deviceClass - the class of the device
 */
void sendDeviceRemovedEvent(const char *uuid, const char *deviceClass);

/*
 * broadcast a resource updated event to any listeners
 *
 * @param resource - the resource that changed
 * @param metadata - any optional metadata about the resource updated event
 */
void sendResourceUpdatedEvent(icDeviceResource *resource, cJSON* metadata);

/*
 * broadcast an endpoint added event to any listeners
 */
void sendEndpointAddedEvent(icDeviceEndpoint *endpoint, const char *deviceClass);

/*
 * broadcast an endpoint removed event to any listeners
 */
void sendEndpointRemovedEvent(icDeviceEndpoint *endpoint, const char *deviceClass, bool deviceRemoved);

/*
 * broadcast device service status event
 */
void sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReason reason);

/*
 * broadcast a zigbee channel changed event
 */
void sendZigbeeChannelChangedEvent(bool success, uint8_t currentChannel, uint8_t targetedChannel);

/*
 * broadcast a zigbee network interference changed event
 */
void sendZigbeeNetworkInterferenceEvent(bool interferenceDetected);

/*
 * broadcast a zigbee pan id attack changed event
 */
void sendZigbeePanIdAttackEvent(bool attackDetected);

/*
 * broadcast a event for the remote cli response data we received
 */
void sendZigbeeRemoteCliCommandResponseReceivedEvent(const char *uuid, const char *commandResponse);

/*
 * broadcast a device configuration started event
 *
 * @param uuid - the UUID of the device that is being configured
 * @param deviceClass - the class of the device being configured
 * @param forRecovery - we were trying to recover this device
 */
void sendDeviceConfigureStartedEvent(const char *deviceClass, const char *uuid, bool forRecovery);

/*
 * broadcast a device configuration completed event
 *
 * @param uuid - the UUID of the device that was configured
 * @param deviceClass - the class of the device being configured
 * @param forRecovery - we were trying to recover this device
 */
void sendDeviceConfigureCompletedEvent(const char *deviceClass, const char *uuid, bool forRecovery);

/*
 * broadcast a device configuration failed event
 *
 * @param uuid - the UUID of the device that was configured
 * @param deviceClass - the class of the device being configured
 * @param forRecovery - we were trying to recover this device
 */
void sendDeviceConfigureFailedEvent(const char *deviceClass, const char *uuid, bool forRecovery);

/*
 * broadcast a device service storage changed event
 *
 * @param whatChanged - what kind of file change occurred
 */
void sendStorageChangedEvent(GFileMonitorEvent whatChanged);

/*
 * broadcast a device database failure event
 *
 * @param failureType - the type of failure that occurred
 * @param deviceId - the id of the device that failed, if relevant (may be NULL)
 */
void sendDeviceDatabaseFailureEvent(BDeviceServiceDeviceDatabaseFailureType failureType, const char *deviceId);