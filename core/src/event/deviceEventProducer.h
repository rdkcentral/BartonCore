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

/*-----------------------------------------------
 * deviceEventProducer.h
 *
 * Responsible for sending internal events to API clients via GObject signals.
 *
 * Author: tlea
 *-----------------------------------------------*/

#pragma once

#include "device/icDeviceMetadata.h"
#include "barton-core-client.h"
#include "events/barton-core-device-database-failure-event.h"
#include "events/barton-core-storage-changed-event.h"
#include <glib-object.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <device-driver/device-driver.h>

/*
 * Init the 'class' by wiring up the signals
 */
void deviceEventProducerClassInit(BCoreClientClass *deviceServiceClass);

/*
 * Initialize the device event producer
 */
void deviceEventProducerStartup(BCoreClient *service);

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
void sendResourceUpdatedEvent(icDeviceResource *resource, cJSON *metadata);

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
void sendDeviceDatabaseFailureEvent(BCoreDeviceDatabaseFailureType failureType, const char *deviceId);

/*
 * broadcast a metadata updated event
 * @param metadata - the metadata that changed
 */
void sendMetadataUpdatedEvent(icDeviceMetadata *metadata);
