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
 * Created by Christian Leithner on 5/29/2024.
 */

#include "barton-core-client.h"
#include "barton-core-commissioning-info.h"
#include "barton-core-discovery-filter.h"
#include "barton-core-endpoint.h"
#include "barton-core-initialize-params-container.h"
#include "barton-core-utils.h"
#include "device/icDeviceEndpoint.h"
#include "deviceDiscoveryFilters.h"
#include "deviceService.h"
#include "deviceServiceConfiguration.h"
#include "deviceServicePrivate.h"
#include "event/deviceEventProducer.h"
#include "icTypes/icLinkedList.h"
#include "icTypes/icLinkedListFuncs.h"

#ifdef BARTON_CONFIG_ZIGBEE
#include "private/subsystems/zigbee/zigbeeSubsystem.h"
#endif

#ifdef BARTON_CONFIG_THREAD
#include "subsystems/thread/threadSubsystem.h"
#endif

G_DEFINE_QUARK(b - device - service - client - error - quark, b_core_client_error)

struct _BCoreClient
{
    GObject parent_instance;

    BCoreInitializeParamsContainer *initializeParams;
};

G_DEFINE_TYPE(BCoreClient, b_core_client, G_TYPE_OBJECT)

static GParamSpec *properties[N_B_CORE_CLIENT_PROPERTIES];

static void
b_core_client_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreClient *self = B_CORE_CLIENT(object);

    switch (property_id)
    {
        case B_CORE_CLIENT_PROP_INITIALIZE_PARAMS:
            g_clear_object(&self->initializeParams);
            self->initializeParams = g_value_dup_object(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_client_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreClient *self = B_CORE_CLIENT(object);

    switch (property_id)
    {
        case B_CORE_CLIENT_PROP_INITIALIZE_PARAMS:
            g_value_set_object(value, self->initializeParams);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_client_finalize(GObject *object)
{
    BCoreClient *self = B_CORE_CLIENT(object);

    g_clear_object(&self->initializeParams);

    G_OBJECT_CLASS(b_core_client_parent_class)->finalize(object);
}

static void b_core_client_constructed(GObject *object)
{
    BCoreClient *self = B_CORE_CLIENT(object);

    G_OBJECT_CLASS(b_core_client_parent_class)->constructed(object);

    // Constructed callback basically means the object has been created and
    // constructor properties have been set.
    deviceServiceInitialize(self);
}

static void b_core_client_class_init(BCoreClientClass *klass)
{
    deviceEventProducerClassInit(klass);

    GObjectClass *objectClass = G_OBJECT_CLASS(klass);
    objectClass->finalize = b_core_client_finalize;
    objectClass->constructed = b_core_client_constructed;
    objectClass->set_property = b_core_client_set_property;
    objectClass->get_property = b_core_client_get_property;

    /**
     * BCoreClient:initialize-params: (type BCoreInitializeParamsContainer)
     *
     * The initialize params for the BCoreClient
     */
    properties[B_CORE_CLIENT_PROP_INITIALIZE_PARAMS] =
        g_param_spec_object(B_CORE_CLIENT_PROPERTY_NAMES[B_CORE_CLIENT_PROP_INITIALIZE_PARAMS],
                            "Initialize Params",
                            "The initialize params for the BCoreClient",
                            B_CORE_INITIALIZE_PARAMS_CONTAINER_TYPE,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(objectClass, N_B_CORE_CLIENT_PROPERTIES, properties);
}

static void b_core_client_init(BCoreClient *self)
{
    self->initializeParams = NULL;
}

BCoreClient *b_core_client_new(BCoreInitializeParamsContainer *params)
{
    g_return_val_if_fail(params != NULL, NULL);

    BCoreClient *self =
        g_object_new(B_CORE_CLIENT_TYPE,
                     B_CORE_CLIENT_PROPERTY_NAMES[B_CORE_CLIENT_PROP_INITIALIZE_PARAMS],
                     params,
                     NULL);
    return self;
}

BCoreInitializeParamsContainer *b_core_client_get_initialize_params(BCoreClient *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return self->initializeParams ? g_object_ref(self->initializeParams) : NULL;
}

gboolean b_core_client_start(BCoreClient *self)
{
    g_return_val_if_fail(self != NULL, FALSE);

    return deviceServiceStart() ? TRUE : FALSE;
}

void b_core_client_stop(BCoreClient *self)
{
    g_return_if_fail(self != NULL);

    deviceServiceShutdown();
}

void b_core_client_dependencies_ready(BCoreClient *self)
{
    g_return_if_fail(self != NULL);

    allServicesAvailableNotify();
}

BCoreStatus *b_core_client_get_status(BCoreClient *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    scoped_DeviceServiceStatus *status = deviceServiceGetStatus();
    return convertDeviceServiceStatusToGObject(status);
}

static gboolean doDiscovery(BCoreClient *self,
                            GList *deviceClasses,
                            GList *filters,
                            guint16 timeoutSeconds,
                            bool forRecovery,
                            GError **err)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(deviceClasses != NULL, FALSE);
    g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

    scoped_icLinkedListNofree *deviceClassesList = convertGListToLinkedListGeneric(deviceClasses);

    icLinkedList *filtersList = linkedListCreate();
    for (GList *current = filters; current != NULL; current = current->next)
    {
        BCoreDiscoveryFilter *filter = current->data;
        g_autofree gchar *uriPattern;
        g_autofree gchar *valuePattern;
        g_object_get(filter,
                     B_CORE_DISCOVERY_FILTER_PROPERTY_NAMES[B_CORE_DISCOVERY_FILTER_PROP_URI],
                     &uriPattern,
                     B_CORE_DISCOVERY_FILTER_PROPERTY_NAMES[B_CORE_DISCOVERY_FILTER_PROP_VALUE],
                     &valuePattern,
                     NULL);

        discoveryFilter *internalFilter = discoveryFilterCreate(uriPattern, valuePattern);
        linkedListAppend(filtersList, internalFilter);
    }

    bool retVal = deviceServiceDiscoverStart(deviceClassesList, filtersList, timeoutSeconds, forRecovery);
    linkedListDestroy(filtersList, (linkedListItemFreeFunc) destroyDiscoveryFilterFromList);

    return retVal ? TRUE : FALSE;
}

gboolean b_core_client_discover_start(BCoreClient *self,
                                                GList *deviceClasses,
                                                GList *filters,
                                                guint16 timeoutSeconds,
                                                GError **err)
{
    return doDiscovery(self, deviceClasses, filters, timeoutSeconds, false, err);
}

gboolean b_core_client_recover_start(BCoreClient *self,
                                               GList *deviceClasses,
                                               GList *filters,
                                               guint16 timeoutSeconds,
                                               GError **err)
{
    return doDiscovery(self, deviceClasses, filters, timeoutSeconds, true, err);
}

gboolean b_core_client_discover_stop(BCoreClient *self, GList *deviceClasses)
{
    g_return_val_if_fail(self != NULL, FALSE);

    scoped_icLinkedListNofree *deviceClassesList = NULL;
    if (deviceClasses != NULL)
    {
        deviceClassesList = convertGListToLinkedListGeneric(deviceClasses);
    }

    return deviceServiceDiscoverStop(deviceClassesList) ? TRUE : FALSE;
}

gboolean b_core_client_recover_stop(BCoreClient *self, GList *deviceClasses)
{
    g_return_val_if_fail(self != NULL, FALSE);

    scoped_icLinkedListNofree *deviceClassesList = NULL;
    if (deviceClasses != NULL)
    {
        deviceClassesList = convertGListToLinkedListGeneric(deviceClasses);
    }

    return deviceServiceDiscoverStop(deviceClassesList) ? TRUE : FALSE;
}

BCoreDiscoveryType b_core_client_get_discovery_type(BCoreClient *self)
{
    g_return_val_if_fail(self != NULL, B_CORE_DISCOVERY_TYPE_NONE);

    return deviceServiceIsInRecoveryMode()    ? B_CORE_DISCOVERY_TYPE_RECOVERY
           : deviceServiceIsDiscoveryActive() ? B_CORE_DISCOVERY_TYPE_DISCOVERY
                                              : B_CORE_DISCOVERY_TYPE_NONE;
}

gboolean b_core_client_commission_device(BCoreClient *self,
                                                   gchar *setupPayload,
                                                   guint16 timeoutSeconds,
                                                   GError **err)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(setupPayload != NULL, FALSE);
    g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

    return deviceServiceCommissionDevice(setupPayload, timeoutSeconds) ? TRUE : FALSE;
}

gboolean b_core_client_add_matter_device(BCoreClient *self,
                                                   guint64 nodeId,
                                                   guint16 timeoutSeconds,
                                                   GError **err)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

    return deviceServiceAddMatterDevice(nodeId, timeoutSeconds) ? TRUE : FALSE;
}

BCoreDevice *b_core_client_get_device_by_id(BCoreClient *self, const gchar *id)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(id != NULL, NULL);

    scoped_icDevice *device = deviceServiceGetDevice(id);
    return convertIcDeviceToGObject(device);
}

GList *b_core_client_get_endpoints_by_profile(BCoreClient *self, const gchar *profile)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(profile != NULL, NULL);

    icLinkedList *endpoints = deviceServiceGetEndpointsByProfile(profile);
    GList *endpointsGlist = convertIcDeviceEndpointListToGList(endpoints);
    linkedListDestroy(endpoints, (linkedListItemFreeFunc) endpointDestroy);
    return endpointsGlist;
}

BCoreEndpoint *
b_core_client_get_endpoint_by_id(BCoreClient *self, const gchar *deviceId, const gchar *endpointId)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(deviceId != NULL, NULL);
    g_return_val_if_fail(endpointId != NULL, NULL);

    scoped_icDeviceEndpoint *endpoint = deviceServiceGetEndpointById(deviceId, endpointId);
    return convertIcDeviceEndpointToGObject(endpoint);
}

BCoreEndpoint *b_core_client_get_endpoint_by_uri(BCoreClient *self, const gchar *uri)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uri != NULL, NULL);

    scoped_icDeviceEndpoint *endpoint = deviceServiceGetEndpointByUri(uri);
    return convertIcDeviceEndpointToGObject(endpoint);
}

GList *b_core_client_get_devices(BCoreClient *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    icLinkedList *devices = deviceServiceGetAllDevices();
    GList *devicesGlist = convertIcDeviceListToGList(devices);

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    return devicesGlist;
}

GList *b_core_client_get_devices_by_device_class(BCoreClient *self, const gchar *deviceClass)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(deviceClass != NULL, NULL);

    icLinkedList *devices = deviceServiceGetDevicesByDeviceClass(deviceClass);
    GList *devicesGlist = convertIcDeviceListToGList(devices);

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    return devicesGlist;
}

gchar *b_core_client_read_resource(BCoreClient *self, const gchar *uri, GError **err)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uri != NULL, NULL);

    gchar *retVal = NULL;

    scoped_icDeviceResource *resource = deviceServiceGetResourceByUri(uri);
    if (resource != NULL)
    {
        if ((resource->mode & RESOURCE_MODE_READABLE) == 0) // this resource is not readable
        {
            g_set_error_literal(err, B_CORE_CLIENT_ERROR, RESOURCE_NOT_READABLE, "Resource is not readable");
        }
        else
        {
            retVal = g_strdup(resource->value);
        }
    }

    return retVal;
}

GList *b_core_client_get_devices_by_subsystem(BCoreClient *self, const gchar *subsystem)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(subsystem != NULL, NULL);

    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(subsystem);
    GList *devicesGlist = convertIcDeviceListToGList(devices);

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    return devicesGlist;
}

BCoreDevice *b_core_client_get_device_by_uri(BCoreClient *self, const gchar *uri)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uri != NULL, NULL);

    scoped_icDevice *device = deviceServiceGetDeviceByUri(uri);
    return convertIcDeviceToGObject(device);
}

BCoreResource *b_core_client_get_resource_by_uri(BCoreClient *self, const gchar *uri)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uri != NULL, NULL);

    scoped_icDeviceResource *resource = deviceServiceGetResourceByUri(uri);
    BCoreResource *dsResource = convertIcDeviceResourceToGObject(resource);

    return dsResource;
}

gchar *b_core_client_get_system_property(BCoreClient *self, const gchar *key)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(key != NULL, NULL);

    gchar *value = NULL;
    deviceServiceGetSystemProperty(key, &value);
    return value;
}

gboolean
b_core_client_write_resource(BCoreClient *self, const gchar *uri, const gchar *resourceValue)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(uri != NULL, FALSE);

    return deviceServiceWriteResource(uri, resourceValue);
}

gboolean b_core_client_execute_resource(BCoreClient *self,
                                                  const gchar *uri,
                                                  const gchar *payload,
                                                  gchar **response)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(uri != NULL, FALSE);

    return deviceServiceExecuteResource(uri, payload, response) ? TRUE : FALSE;
}

gboolean b_core_client_remove_device(BCoreClient *self, const gchar *uuid)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(uuid != NULL, FALSE);

    return deviceServiceRemoveDevice(uuid) ? TRUE : FALSE;
}

gboolean b_core_client_remove_endpoint_by_id(BCoreClient *self,
                                                       const gchar *deviceUuid,
                                                       const gchar *endpointId)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(deviceUuid != NULL, FALSE);
    g_return_val_if_fail(endpointId != NULL, FALSE);

    return deviceServiceRemoveEndpointById(deviceUuid, endpointId) ? TRUE : FALSE;
}

gboolean b_core_client_set_system_property(BCoreClient *self, const gchar *name, const gchar *value)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(name != NULL, FALSE);
    g_return_val_if_fail(value != NULL, FALSE);

    return deviceServiceSetSystemProperty(name, value) ? TRUE : FALSE;
}

gboolean b_core_client_write_metadata(BCoreClient *self, const gchar *uri, const gchar *value)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(uri != NULL, FALSE);
    g_return_val_if_fail(value != NULL, FALSE);

    return deviceServiceSetMetadata(uri, value) ? TRUE : FALSE;
}

gchar *b_core_client_read_metadata(BCoreClient *self, const gchar *uri, GError **err)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uri != NULL, NULL);

    gchar *value = NULL;
    if (!deviceServiceGetMetadata(uri, &value))
    {
        g_set_error_literal(err, B_CORE_CLIENT_ERROR, METADATA_NOT_ACCESSIBLE, "Metadata not accessible");
    }
    return value;
}

GList *b_core_client_get_metadata_by_uri(BCoreClient *self, gchar *uriPattern)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uriPattern != NULL, NULL);

    GList *retVal = NULL;

    icLinkedList *metadatas = deviceServiceGetMetadataByUriPattern(uriPattern);
    if (metadatas != NULL)
    {
        retVal = convertIcDeviceMetadataListToGList(metadatas);
        linkedListDestroy(metadatas, (linkedListItemFreeFunc) metadataDestroy);
    }

    return retVal;
}

GList *b_core_client_query_resources_by_uri(BCoreClient *self, gchar *uri)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uri != NULL, NULL);

    icLinkedList *resources = deviceServiceGetResourcesByUriPattern(uri);
    GList *resourcesGlist = convertIcDeviceResourceListToGList(resources);

    linkedListDestroy(resources, (linkedListItemFreeFunc) resourceDestroy);

    return resourcesGlist;
}

gboolean b_core_change_resource_mode(BCoreClient *self, const gchar *uri, guint16 mode)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(uri != NULL, FALSE);

    return deviceServiceChangeResourceMode(uri, mode) ? TRUE : FALSE;
}

guint8
b_core_client_change_zigbee_channel(BCoreClient *self, guint8 channel, gboolean dryRun, GError **err)
{
#ifndef BARTON_CONFIG_ZIGBEE
    g_set_error_literal(
        err, B_CORE_CLIENT_ERROR, ZIGBEE_CHANNEL_CHANGE_NOT_ALLOWED, "Zigbee is not supported");
    return 0;
#else
    g_return_val_if_fail(self != NULL, 0);
    g_return_val_if_fail(err == NULL || *err == NULL, 0);

    ChannelChangeResponse channelChangeResponse = zigbeeSubsystemChangeChannel(channel, dryRun);
    if (channelChangeResponse.responseCode != channelChangeSuccess)
    {
        switch (channelChangeResponse.responseCode)
        {
            case channelChangeNotAllowed:
                g_set_error_literal(err,
                                    B_CORE_CLIENT_ERROR,
                                    ZIGBEE_CHANNEL_CHANGE_NOT_ALLOWED,
                                    "Zigbee channel change is not allowed");
                break;

            case channelChangeInvalidChannel:
                g_set_error_literal(err,
                                    B_CORE_CLIENT_ERROR,
                                    ZIGBEE_CHANNEL_CHANGE_INVALID_CHANNEL,
                                    "Invalid Zigbee channel specified");
                break;

            case channelChangeInProgress:
                g_set_error_literal(err,
                                    B_CORE_CLIENT_ERROR,
                                    ZIGBEE_CHANNEL_CHANGE_IN_PROGRESS,
                                    "Zigbee channel change is currently in progress");
                break;

            case channelChangeUnableToCalculate:
                g_set_error_literal(err,
                                    B_CORE_CLIENT_ERROR,
                                    ZIGBEE_CHANNEL_CHANGE_UNABLE_TO_CALCULATE,
                                    "Failed to calculate the Zigbee channel");
                break;

            case channelChangeFailed:
            default:
                g_set_error_literal(
                    err, B_CORE_CLIENT_ERROR, ZIGBEE_CHANNEL_CHANGE_FAILED, "Zigbee channel change failed");
                break;
        }
    }
    return channelChangeResponse.channelNumber;
#endif
}

void b_core_process_device_descriptors(BCoreClient *self)
{
    g_return_if_fail(self != NULL);

    deviceServiceProcessDeviceDescriptors();
}

#ifdef BARTON_CONFIG_ZIGBEE
gchar *b_core_client_zigbee_test(BCoreClient *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return zhalTest();
}

GList *b_core_client_zigbee_energy_scan(BCoreClient *self,
                                                  GList *channels,
                                                  guint32 maxScanDuration,
                                                  guint32 scanCount)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(channels != NULL, NULL);

    GList *retVal = NULL;

    guint8 numChannels = g_list_length(channels);
    uint8_t *channelArray = (uint8_t *) malloc(numChannels * sizeof(uint8_t));
    int i = 0;
    for (GList *iter = channels; iter != NULL; iter = g_list_next(iter))
    {
        guint8 channel = GPOINTER_TO_UINT(iter->data);
        channelArray[i++] = (uint8_t) channel;
    }

    icLinkedList *energyScanResults =
        zigbeeSubsystemPerformEnergyScan(channelArray, numChannels, maxScanDuration, scanCount);

    if (energyScanResults != NULL)
    {
        retVal = convertIcZigbeeEnergyScanResultListToGList(energyScanResults);
        linkedListDestroy(energyScanResults, NULL);
    }

    free(channelArray);

    return retVal;
}
#endif

gboolean b_core_client_thread_set_nat64_enabled(BCoreClient *self, gboolean enabled)
{
    g_return_val_if_fail(self != NULL, FALSE);

#ifdef BARTON_CONFIG_THREAD
    return threadSubsystemSetNat64Enabled(enabled) ? TRUE : FALSE;
#else
    return FALSE;
#endif
}

gchar *b_core_client_thread_activate_ephemeral_key_mode(BCoreClient *self)
{
    g_return_val_if_fail(self != NULL, NULL);

#ifdef BARTON_CONFIG_THREAD
    return threadSubsystemActivateEphemeralKeyMode();
#else
    return NULL;
#endif
}

gboolean b_core_client_config_restore(BCoreClient *self, const gchar *tempRestoreDir)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(tempRestoreDir != NULL, FALSE);

    return deviceServiceRestoreConfig(tempRestoreDir) ? TRUE : FALSE;
}

void b_core_client_set_account_id(BCoreClient *self, const gchar *accountId)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(accountId != NULL);

    deviceServiceConfigurationSetAccountId(accountId);
}

BCoreCommissioningInfo *b_core_client_open_commissioning_window(BCoreClient *self, const gchar *deviceId, guint16 timeoutSeconds)
{
    BCoreCommissioningInfo *result = NULL;

    g_return_val_if_fail(self != NULL, NULL);

    g_autofree gchar *manualCode = NULL;
    g_autofree gchar *qrCode = NULL;

    if (deviceServiceOpenCommissioningWindow(deviceId, timeoutSeconds, &manualCode, &qrCode))
    {
        result = b_core_commissioning_info_new();

        g_object_set(result,
                     B_CORE_COMMISSIONING_INFO_PROPERTY_NAMES[B_CORE_COMMISSIONING_INFO_PROP_MANUAL_CODE],
                     manualCode,
                     B_CORE_COMMISSIONING_INFO_PROPERTY_NAMES[B_CORE_COMMISSIONING_INFO_PROP_QR_CODE],
                     qrCode,
                     NULL);
    }

    return result;
}
