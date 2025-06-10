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
 * Created by Thomas Lea on 5/6/2024.
 */

#pragma once

#include "barton-core-commissioning-info.h"
#include "barton-core-device.h"
#include "barton-core-discovery-type.h"
#include "barton-core-endpoint.h"
#include "barton-core-initialize-params-container.h"
#include "barton-core-resource.h"
#include "barton-core-status.h"
#include <glib-object.h>

#include <stdint.h>

G_BEGIN_DECLS

#define B_CORE_CLIENT_TYPE b_core_client_get_type()
G_DECLARE_FINAL_TYPE(BCoreClient, b_core_client, B_CORE, CLIENT, GObject)

/****************** Error Sets Begin *********************************/

#define B_CORE_CLIENT_ERROR (b_core_client_error_quark())

// Error codes for the quark must be unique, so each error set will be
// defined within a specified range, in intervals of 100, to avoid conflicts.

// Error code range specifications:
// - BCoreReadResourceError: 1-99
// - BCoreZigbeeChannelChangeError: 100-199
// - BCoreReadMetadataError: 200-299

typedef enum
{
    RESOURCE_NOT_READABLE = 1 // The resource is not readable
} BCoreReadResourceError;

typedef enum
{
    ZIGBEE_CHANNEL_CHANGE_FAILED = 100,
    ZIGBEE_CHANNEL_CHANGE_NOT_ALLOWED,
    ZIGBEE_CHANNEL_CHANGE_INVALID_CHANNEL,
    ZIGBEE_CHANNEL_CHANGE_IN_PROGRESS,
    ZIGBEE_CHANNEL_CHANGE_UNABLE_TO_CALCULATE,
} BCoreZigbeeChannelChangeError;

typedef enum
{
    METADATA_NOT_ACCESSIBLE = 200 // The metadata is not accessible
} BCoreReadMetadataError;

GQuark b_core_client_error_quark(void);

/****************** Error Sets End **********************************/

/****************** Signals Begin **********************************/

#define B_CORE_CLIENT_SIGNAL_NAME_STATUS_CHANGED                 "status-changed"
// signal handler args: BCoreStatusEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STARTED              "discovery-started"
// signal handler args: BCoreDiscoveryStartedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_RECOVERY_STARTED               "recovery-started"
// signal handler args: BCoreRecoveryStartedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_FAILED        "device-discovery-failed"
// signal handler args: BCoreDiscoveryFailedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERED              "device-discovered"
// signal handler args: BCoreDeviceDiscoveredEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REJECTED                "device-rejected"
// signal handler args: BCoreDeviceRejectedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_STARTED   "device-configuration-started"
// signal handler args: BCoreDeviceConfigurationStartedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_COMPLETED "device-configuration-completed"
// signal handler args: BCoreDeviceConfigurationCompletedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_FAILED    "device-configuration-failed"
// signal handler args: BCoreDeviceConfigurationFailedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_ADDED                   "device-added"
// signal handler args: BCoreDeviceAddedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_RECOVERED               "device-recovered"
// signal handler args: BCoreDeviceRecoveredEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_COMPLETED     "device-discovery-completed"
// signal handler args: BCoreDeviceDiscoveryCompletedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STOPPED              "discovery-stopped"
// signal handler args: BCoreDiscoveryStoppedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_RECOVERY_STOPPED               "recovery-stopped"
// signal handler args: BCoreRecoveryStoppedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_RESOURCE_UPDATED               "resource-updated"
// signal handler args: BCoreResourceUpdatedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REMOVED                 "device-removed"
// signal handler args: BCoreDeviceRemovedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_REMOVED               "endpoint-removed"
// signal handler args: BCoreEndpointRemovedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED                 "endpoint-added"
// signal handler args: BCoreEndpointAddedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_CHANNEL_CHANGED         "zigbee-channel-changed"
// signal handler args: BCoreZigbeeChannelChangedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_STORAGE_CHANGED                "storage-changed"
// signal handler args: BCoreStorageChangedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_INTERFERENCE            "zigbee-interference"
// signal handler args: BCoreZigbeeInterferenceEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_PAN_ID_ATTACK_CHANGED   "zigbee-pan-id-attack-changed"
// signal handler args: BCoreZigbeePanIdAttackChangedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DATABASE_FAILURE        "device-database-failure"
// signal handler args: BCoreDeviceDatabaseFailureEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_METADATA_UPDATED               "metadata-updated"
// signal handler args: BCoreMetadataUpdatedEvent *event
#define B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED                                          \
    "zigbee-remote-cli-command-response-received"
// signal handler args: BCoreZigbeeRemoteCliCommandResponseReceivedEvent *event

// TODO - implement these
#define B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_SUBSYSTEM_STATUS_CHANGED

/****************** Signals End **********************************/

typedef enum
{
    B_CORE_CLIENT_PROP_INITIALIZE_PARAMS = 1, // BCoreInitializeParamsContainer*

    N_B_CORE_CLIENT_PROPERTIES
} BCoreClientProperty;

static const char *B_CORE_CLIENT_PROPERTY_NAMES[] = {NULL, "initialize-params"};

/**
 * b_core_client_new
 * @params: the BCoreInitializeParamsContainer instance containing injected implementations
 * of device service dependencies and configuration.
 *
 * @brief Create a new BCoreClient instance. Call @see b_core_client_start() to start the service.
 *
 * Returns: (transfer full): BCoreClient* - the new BCoreClient instance.
 */
BCoreClient *b_core_client_new(BCoreInitializeParamsContainer *params);

/**
 * b_core_client_get_initialize_params
 * @self: the BCoreClient instance.
 *
 * @brief Get the initialize params for the BCoreClient.
 *
 * Returns: (transfer none): BCoreInitializeParamsContainer* - the initialize params for the
 * BCoreClient.
 */
BCoreInitializeParamsContainer *b_core_client_get_initialize_params(BCoreClient *self);

/**
 * b_core_client_start
 * @self: the BCoreClient instance.
 *
 * @brief Start the BCoreClient. This will start the service and its internal systems.
 * The service will be running but not fully functional until @see b_core_client_dependencies_ready() is
 * called.
 *
 * Call @see b_core_client_stop() to stop the service.
 *
 * Returns: gboolean - TRUE if the service was started successfully.
 */
gboolean b_core_client_start(BCoreClient *self);

/**
 * b_core_client_stop
 * @self: the BCoreClient instance.
 *
 * @brief Stop the BCoreClient. This will stop the service and its internal systems.
 */
void b_core_client_stop(BCoreClient *self);

/**
 * b_core_client_dependencies_ready
 * @self: the BCoreClient instance.
 *
 * @brief Notify the BCoreClient that all dependent services are ready.
 *
 * TODO: This function should be removed as it doesn't make sense universally, but is needed for existing barton
 * clients for now.
 */
void b_core_client_dependencies_ready(BCoreClient *self);

/**
 * b_core_client_get_status
 * @self: the BCoreClient instance.
 *
 * @brief Get the current status of the BCoreClient.
 *
 * Returns: (transfer full): BCoreStatus* - the current status of the BCoreClient.
 */
BCoreStatus *b_core_client_get_status(BCoreClient *self);

/**
 * b_core_client_discover_start
 * @deviceClasses: (element-type utf8): a list of device classes to discover
 * @filters: (element-type utf8): an optional list of device filters to perform the filter check once device discoverd
 * @timeoutSeconds: the number of seconds to perform discovery before automatically stopping. If 0 is specified,
 * discovery will continue forever until explicitly stopped via b_core_client_discover_stop().
 * @err: a GError to store any error that occurs. At present, this is not used.
 *
 * @brief Start discovery for specific device classes.
 *
 * For each of the provided device classes, find registered device drivers and instruct them to start discovery.
 *
 * Returns: gboolean - TRUE if at least one device driver successfully started discovery.
 */
gboolean b_core_client_discover_start(BCoreClient *self,
                                      GList *deviceClasses,
                                      GList *filters,
                                      guint16 timeoutSeconds,
                                      GError **err);

/**
 * b_core_client_recover_start
 * @deviceClasses: (element-type utf8): a list of device classes to recover
 * @filters: (element-type utf8): an optional list of device filters to perform the filter check once device recovered
 * @timeoutSeconds: the number of seconds to perform recovery before automatically stopping. If 0 is specified,
 * recovery will continue forever until explicitly stopped via b_core_client_recover_stop().
 * @err: a GError to store any error that occurs. At present, this is not used.
 *
 * @brief Start recovery for specific device classes.
 *
 * For each of the provided device classes, find registered device drivers and instruct them to start recovery.
 *
 * Returns: gboolean - TRUE if at least one device driver successfully started recovery.
 */
gboolean b_core_client_recover_start(BCoreClient *self,
                                     GList *deviceClasses,
                                     GList *filters,
                                     guint16 timeoutSeconds,
                                     GError **err);

/**
 * b_core_client_discover_stop
 * @self: the BCoreClient instance.
 * @deviceClasses: (element-type utf8): a list of device classes to stop discovery for. NULL means stop all discovery.
 *
 * @brief Stop discovery.
 *
 * Stop discovery for the provided device classes. If deviceClasses is NULL, stop all discovery.
 *
 * Returns: gboolean - TRUE upon success
 */
gboolean b_core_client_discover_stop(BCoreClient *self, GList *deviceClasses);

/**
 * b_core_client_recover_stop
 * @self: the BCoreClient instance.
 * @deviceClasses: (element-type utf8): a list of device classes to stop recovery for. NULL means stop all recovery.
 *
 * @brief Stop recovery.
 *
 * Stop recovery for the provided device classes. If deviceClasses is NULL, stop all recovery.
 *
 * Returns: gboolean - TRUE upon success
 */
gboolean b_core_client_recover_stop(BCoreClient *self, GList *deviceClasses);

/**
 * b_core_client_get_discovery_type
 * @self: the BCoreClient instance.
 *
 * @brief Check the current discovery type if applicable
 *
 * @return BCoreDiscoveryType - The type of discovery active, or B_CORE_DISCOVERY_TYPE_NONE if
 * discovery is not active
 */
BCoreDiscoveryType b_core_client_get_discovery_type(BCoreClient *self);

/**
 * b_core_client_commission_device
 * @self: the BCoreClient instance.
 * @setupPayload: the setup payload for the device to commission. Technology dependent. Currently only Matter is
 * supported, so this is a Matter setup payload.
 * @timeoutSeconds: the number of seconds to wait for commissioning to complete.
 * @err: a GError to store any error that occurs. At present, this is not used.
 *
 * @brief Commission a device.
 * @note Matter is the only supported technology for device commissioning
 *
 * Returns: gboolean
 */
gboolean b_core_client_commission_device(BCoreClient *self, gchar *setupPayload, guint16 timeoutSeconds, GError **err);

/**
 * b_core_client_add_matter_device
 * @self: the BCoreClient instance.
 * @nodeId: the node id of the matter device to add.
 * @timeoutSeconds: the number of seconds to wait for commissioning to complete.
 * @err: a GError to store any error that occurs. At present, this is not used.
 *
 * @brief Add a matter device that should already be commissioned on the network.
 * This is steps 13-15 of the standard commissioning sequence as the Administrator
 * role (Matter spec 5.5.2).
 *
 * Returns: gboolean
 */
gboolean b_core_client_add_matter_device(BCoreClient *self, guint64 nodeId, guint16 timeoutSeconds, GError **err);

/**
 * b_core_client_get_devices
 * @self: the BCoreClient instance.
 *
 * @brief Get all devices in the system.
 *
 * Returns: (element-type BCoreDevice) (transfer full): GList* - a list of devices in the system.
 */
GList *b_core_client_get_devices(BCoreClient *self);

/**
 * b_core_client_get_device_by_id
 * @self: the BCoreClient instance.
 * @id: the device id to search for.
 *
 * @brief Get a device by its id.
 *
 * Returns: (transfer full): BCoreDevice* - the device with the given id, or NULL if not found.
 */
BCoreDevice *b_core_client_get_device_by_id(BCoreClient *self, const gchar *id);

/**
 * b_core_client_get_endpoints_by_profile
 * @self: the BCoreClient instance.
 * @profile: the profile to search for.
 *
 * @brief Retrieve a list of device endpoints that provide the given profile.
 *
 * Returns: (element-type BCoreEndpoint) (transfer full): GList* - a list of BCoreEndpoints that
 * provide the given profile.
 */
GList *b_core_client_get_endpoints_by_profile(BCoreClient *self, const gchar *profile);

/**
 * b_core_client_get_endpoint_by_id
 * @self: the BCoreClient instance.
 * @deviceId: the endpoint's owning device id
 * @endpointId: the endpoint id to search for.
 *
 * @brief Retrieve an endpoint from the system by id.
 *
 * Returns: (transfer full): BCoreEndpoint* - the endpoint with the given id, or NULL if not found.
 */
BCoreEndpoint *b_core_client_get_endpoint_by_id(BCoreClient *self, const gchar *deviceId, const gchar *endpointId);

/**
 * b_core_client_get_endpoint_by_uri
 * @self: the BCoreClient instance.
 * @uri: the endpoint's uri
 *
 * @brief Retrieve an endpoint from the system by the endpoint's uri.
 *
 * Returns: (transfer full): BCoreEndpoint* - the endpoint with the given uri, or NULL if not found.
 */
BCoreEndpoint *b_core_client_get_endpoint_by_uri(BCoreClient *self, const gchar *uri);

/**
 * b_core_client_change_zigbee_channel
 * @self: the BCoreClient instance.
 * @channel: the targeted channel, if it is zero then device service will try to find the best channel.
 * @dryRun: the zigbee channel dry run.
 * @err: a GError to store any error that occurs.
 *
 * @brief Change the zigbee channel
 *
 * Returns: guint8 - 0 when there's an error while changing the channel.
 */
guint8 b_core_client_change_zigbee_channel(BCoreClient *self, guint8 channel, gboolean dryRun, GError **err);

/**
 * b_core_client_get_devices_by_device_class
 * @self: the BCoreClient instance.
 * @deviceClass: the device class to search for.
 *
 * @brief Get all devices in the system that are of the given device class.
 *
 * Returns: (element-type BCoreDevice) (transfer full): GList* - a list of devices in the system that are of
 * the given device class.
 */
GList *b_core_client_get_devices_by_device_class(BCoreClient *self, const gchar *deviceClass);

/**
 * b_core_client_remove_device
 * @self: the BCoreClient instance.
 * @uuid: the device id to remove.
 *
 * @brief Remove a device from the system by id.
 *
 * Returns: gboolean - TRUE if the device was removed successfully.
 */
gboolean b_core_client_remove_device(BCoreClient *self, const gchar *uuid);

/**
 * b_core_client_remove_endpoint_by_id
 * @self: the BCoreClient instance.
 * @deviceUuid: the device's unique identifier
 * @endpointId: the endpoint's id
 *
 * @brief Remove an endpoint from the system by the endpoint's id.
 *
 * Returns: gboolen - TRUE if the endpoint was removed successfully, otherwise FALSE.
 */
gboolean b_core_client_remove_endpoint_by_id(BCoreClient *self, const gchar *deviceUuid, const gchar *endpointId);

/**
 * b_core_client_execute_resource
 * @self: the BCoreClient instance.
 * @uri: the URI of the resource to execute.
 * @payload: JSON payload to send to the resource to execute. (optional)
 * @response: the response from the executed resource.(optional)
 *
 * @brief Execute a resource based on the URI with the optional payload.
 *
 * Returns: gboolean - TRUE if the resource was executed successfully.
 */
gboolean b_core_client_execute_resource(BCoreClient *self, const gchar *uri, const gchar *payload, char **response);

/**
 * b_core_client_write_resource
 * @self: the BCoreClient instance.
 * @uri: the URI of the resource.
 * @resourceValue: the new resource value.
 *
 * @brief Writes the new resourceValue to the resource with the specified uri.
 *
 * Returns: gboolean - true if all resources were written and false if one or more failed.
 */
gboolean b_core_client_write_resource(BCoreClient *self, const gchar *uri, const gchar *resourceValue);

/**
 * b_core_client_get_devices_by_subsystem
 * @self: the BCoreClient instance.
 * @subsystem: the subsystem to search for.
 *
 * @brief Get all devices within the given subsystem.
 *
 * Returns: (element-type BCoreDevice) (transfer full): GList* - a list of devices within the given subsystem.
 */
GList *b_core_client_get_devices_by_subsystem(BCoreClient *self, const gchar *subsystem);

/**
 * b_core_client_read_resource
 * @self: the BCoreClient instance.
 * @uri: endpoint id
 * @err: a GError to store any error that occurs.
 *
 * @brief Read a resource from a device or endpoint by URI.
 *
 * Returns: gchar* - A string representing the resource value
 */
gchar *b_core_client_read_resource(BCoreClient *self, const gchar *uri, GError **err);

/**
 * b_core_client_get_device_by_uri
 * @self: the BCoreClient instance.
 * @uri: the device uri to search for.
 *
 * @brief Get a device by its uri.
 *
 * Returns: (transfer full): BCoreDevice* - the device with the given uri, or NULL if not found.
 */
BCoreDevice *b_core_client_get_device_by_uri(BCoreClient *self, const gchar *uri);

/**
 * b_core_client_get_resource_by_uri
 * @self: the BCoreClient instance.
 * @uri: the URI of the resource.
 *
 * @brief Get resource by its uri.
 *
 * Returns: (transfer full): BCoreResource* - the resource with the given uri, or NULL if not found.
 */
BCoreResource *b_core_client_get_resource_by_uri(BCoreClient *self, const gchar *uri);

/**
 * b_core_client_get_system_property
 * @self: the BCoreClient instance.
 * @key: the name of the system property to retrieve.
 *
 * @brief Retrieve a system property by name.
 *
 * Returns: gchar* - the value of the system property, or NULL if not found.
 *
 */
gchar *b_core_client_get_system_property(BCoreClient *self, const gchar *key);

/**
 * b_core_client_set_system_property
 * @self: the BCoreClient instance.
 * @name: the name of the property to set.
 * @value: the value to set the property to.
 *
 * @brief Set a device service system property.
 *
 * Returns: gboolean - TRUE if the property was successfully set
 */
gboolean b_core_client_set_system_property(BCoreClient *self, const gchar *name, const gchar *value);

/**
 * b_core_client_write_metadata
 * @self: the BCoreClient instance.
 * @uri: the URI of the device or endpoint.
 * @value: the value to write to the resource's metadata.
 *
 * @brief Write metadata to a device or endpoint
 *
 * Returns: gboolean - TRUE if the resource was written successfully, otherwise FALSE.
 */
gboolean b_core_client_write_metadata(BCoreClient *self, const gchar *uri, const gchar *value);

/**
 * b_core_client_get_metadata_by_uri
 * @self: the BCoreClient instance.
 * @uriPattern: the uri pattern to search with
 *
 * @brief Query for metadata based on a uri pattern
 *
 * Returns: (element-type utf8) (transfer full): GList* - a list of metadata with the given uri, or NULL if not found.
 */
GList *b_core_client_get_metadata_by_uri(BCoreClient *self, gchar *uriPattern);

/**
 * b_core_client_read_metadata
 * @self: the BCoreClient instance.
 * @uri: the URI to a metadata item.
 * @err: a GError to store any error that occurs.
 *
 * @brief Get the value of a metadata item at the provided URI.
 *
 * Returns: gchar* - the value of the metadata if successful and NULL on failure.
 *
 */
gchar *b_core_client_read_metadata(BCoreClient *self, const gchar *uri, GError **err);

/**
 * b_core_client_query_resources_by_uri
 * @self: the BCoreClient instance.
 * @uri: resource uri pattern to search for.
 *
 * @brief Get a list of resources that match the URI.
 *
 * Returns: (element-type BCoreResource) (transfer full): GList* - a list of resources that match the URI
 * pattern.
 */
GList *b_core_client_query_resources_by_uri(BCoreClient *self, gchar *uri);

/**
 * b_core_change_resource_mode
 * @self: the BCoreClient instance.
 * @uri: the URI of the resource.
 * @mode: the new resource mode.
 *
 * @brief Change the mode of a resource specified by the URI.
 *
 * Returns: gboolean - TRUE if the resource mode was changed successfully.
 */
gboolean b_core_change_resource_mode(BCoreClient *self, const gchar *uri, const guint16 mode);

/**
 * b_core_process_device_descriptors
 * @self: the BCoreClient instance.
 *
 * @brief Process the device descriptors for each device.
 */
void b_core_process_device_descriptors(BCoreClient *self);

/**
 * b_core_client_zigbee_test
 * @self: the BCoreClient instance.
 *
 * @brief Perform a Zigbee test.  Currently this attempts to interact with a special device to test transmit and receive
 * but in the future this can be extended to run various other Zigbee related tests.
 *
 * Returns: gchar* - a JSON structure containing the test results or NULL upon failure
 */
gchar *b_core_client_zigbee_test(BCoreClient *self);

/**
 * b_core_client_zigbee_energy_scan
 * @self: the BCoreClient instance.
 * @channels: (element-type guint8): List of guint8 types representing the set of channels to perform the scan on.
 *     - Channel values must be valid 802.15.4 channels [11-26].
 * @maxScanDuration: The maximum time to scan each channel in milliseconds.
 * @scanCount: The number of scans to perform on each channel.
 *
 * @brief Perform an energy scan on the provided list of channels.
 *
 * Returns: (element-type BCoreZigbeeEnergyScanResult) (transfer full): GList* - a list of
 * BCoreZigbeeEnergyScanResult instances.
 */
GList *b_core_client_zigbee_energy_scan(BCoreClient *self, GList *channels, guint32 maxScanDuration, guint32 scanCount);

/**
 * b_core_client_thread_set_nat64_enabled
 * @self: the BCoreClient instance.
 * @enabled: true to enable NAT64 in the Thread Border Router, false to disable
 *
 * @brief Enable or disable NAT64 functionality in the Thread Border Router
 *
 * Returns: gboolean - TRUE if NAT64 was enabled or disabled successfully, otherwise FALSE.
 */
gboolean b_core_client_thread_set_nat64_enabled(BCoreClient *self, gboolean enabled);

/**
 * b_core_client_thread_activate_ephemeral_key_mode
 * @self: the BCoreClient instance.
 *
 * @brief Activate ephemeral key mode and return the generated key (ePSKc). The
 *        mode will be active for the default duration of the border router, then
 *        automatically deactivate.
 *
 * Returns: gchar* - the generated ephemeral key (ePSKc, which is a 9 digit decimal value
 *          represented as an ASCII string) or NULL on failure.
 */
gchar *b_core_client_thread_activate_ephemeral_key_mode(BCoreClient *self);

/**
 * b_core_client_config_restore
 * @self: the BCoreClient instance.
 * @tempRestoreDir: the path to a temporary device storage directory containing the configuration
 * data to restore.
 *
 * @brief Restore configuration data from another directory.
 *
 * Returns: gboolean - TRUE if the configuration was restored successfully, otherwise FALSE.
 */
gboolean b_core_client_config_restore(BCoreClient *self, const gchar *tempRestoreDir);

/**
 * b_core_client_get_account_id
 * @self: the BCoreClient instance.
 * @accountId: the account id for device service
 *
 * @brief Set the account id for device service.
 *
 * The account id is a unique identifier for the logical concept of an account associated with this installation
 * of device service. The purpose and scope for this account is opaque to device service and are determined by
 * the client application. The account id is used to encode certain identifying information, such as the id of the
 * Matter fabric managed by device service.
 *
 * @note Once an account id is set, changing it could have undefined behavior.
 */
void b_core_client_set_account_id(BCoreClient *self, const gchar *accountId);

/**
 * b_core_client_open_commissioning_window
 * @self: the BCoreClient instance.
 * @deviceId: the device id to open the commissioning window for or NULL to open locally on this device. "0" also
 *            indicates local commissioning.
 * @timeoutSeconds: the number of seconds to keep the commissioning window open or 0 to use the default timeout.

 * @brief Open a Matter commissioning window for a remote device or locally on this device.  Upon success, a 11 digit
 * setup code and a textual QR code will be returned for use by a commissioner to commission the device over the
 * operational network.
 *
 * Returns: (transfer full): BCoreCommissioningInfo* - the info about the open commissioning window upon
 *                           success
 * or NULL upon failure.
 */
BCoreCommissioningInfo *
b_core_client_open_commissioning_window(BCoreClient *self, const gchar *deviceId, guint16 timeoutSeconds);

G_END_DECLS
