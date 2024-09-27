//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Christian Leithner on 7/30/2024.
 */

#pragma once

#include "provider/device-service-network-credentials-provider.h"
#include "provider/device-service-property-provider.h"
#include "provider/device-service-token-provider.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_TYPE (b_device_service_initialize_params_container_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceInitializeParamsContainer,
                     b_device_service_initialize_params_container,
                     B_DEVICE_SERVICE,
                     INITIALIZE_PARAMS_CONTAINER,
                     GObject);

typedef enum
{
    // providers
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER = 1,
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER,
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER,

    // configuration
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR,
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR,
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR,
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR,
    B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID,

    N_B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTIES
} BDeviceServiceInitializeParamsContainerProperty;

static const gchar *B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES[] = {
    NULL,
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER] = "token-provider",
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER] = "network-credentials-provider",
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER] = "property-provider",
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR] = "storage-dir",
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR] = "firmware-file-dir",
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR] = "matter-storage-dir",
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR] =
        "matter-attestation-trust-store-dir",
    [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID] = "account-id",
};

/**
 * b_device_service_initialize_params_container_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceInitializeParamsContainer*
 */
BDeviceServiceInitializeParamsContainer *b_device_service_initialize_params_container_new(void);

/**
 * b_device_service_initialize_params_container_get_token_provider
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The token provider for device service
 */
BDeviceServiceTokenProvider *
b_device_service_initialize_params_container_get_token_provider(BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_get_network_credentials_provider
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The network credentials provider for device service
 */
BDeviceServiceNetworkCredentialsProvider *b_device_service_initialize_params_container_get_network_credentials_provider(
    BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_get_property_provider
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The property provider for device service
 */
BDeviceServicePropertyProvider *
b_device_service_initialize_params_container_get_property_provider(BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_get_storage_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The storage directory for device service configuration
 */
gchar *b_device_service_initialize_params_container_get_storage_dir(BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_get_firmware_file_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The directory device service will install firmware files
 */
gchar *
b_device_service_initialize_params_container_get_firmware_file_dir(BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_get_matter_storage_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The storage directory for externally managed Matter configuration
 */
gchar *
b_device_service_initialize_params_container_get_matter_storage_dir(BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_get_matter_attestation_trust_store_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The directory device service will use as an atttestation trust store for Matter. Supported
 * Product Attestation Authorities should be installed there.
 */
gchar *b_device_service_initialize_params_container_get_matter_attestation_trust_store_dir(
    BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_get_account_id
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 *
 * Returns: (transfer full): The account ID for device service
 */
gchar *b_device_service_initialize_params_container_get_account_id(BDeviceServiceInitializeParamsContainer *self);

/**
 * b_device_service_initialize_params_container_set_token_provider
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @tokenProvider: BDeviceServiceTokenProvider*
 *
 * Returns: whether or not the token provider was set
 */
gboolean b_device_service_initialize_params_container_set_token_provider(BDeviceServiceInitializeParamsContainer *self,
                                                                         BDeviceServiceTokenProvider *tokenProvider);

/**
 * b_device_service_initialize_params_container_set_property_provider
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @propertyProvider: BDeviceServicePropertyProvider*
 *
 * Returns: whether or not the property provider was set
 */
gboolean
b_device_service_initialize_params_container_set_property_provider(BDeviceServiceInitializeParamsContainer *self,
                                                                   BDeviceServicePropertyProvider *propertyProvider);

/**
 * b_device_service_initialize_params_container_set_network_credentials_provider
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @networkCredentialsProvider: BDeviceServiceNetworkCredentialsProvider*
 *
 * Returns: whether or not the network credentials provider was set
 */
gboolean b_device_service_initialize_params_container_set_network_credentials_provider(
    BDeviceServiceInitializeParamsContainer *self,
    BDeviceServiceNetworkCredentialsProvider *networkCredentialsProvider);

/**
 * b_device_service_initialize_params_container_set_storage_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @storageDir: const gchar*
 */
void b_device_service_initialize_params_container_set_storage_dir(BDeviceServiceInitializeParamsContainer *self,
                                                                  const gchar *storageDir);

/**
 * b_device_service_initialize_params_container_set_firmware_file_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @firmwareFileDir: const gchar*
 */
void b_device_service_initialize_params_container_set_firmware_file_dir(BDeviceServiceInitializeParamsContainer *self,
                                                                        const gchar *firmwareFileDir);

/**
 * b_device_service_initialize_params_container_set_matter_storage_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @matterStorageDire: const gchar*
 */
void b_device_service_initialize_params_container_set_matter_storage_dir(BDeviceServiceInitializeParamsContainer *self,
                                                                         const gchar *matterStorageDire);

/**
 * b_device_service_initialize_params_container_set_matter_attestation_trust_store_dir
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @matterAttestationTrustStoreDir: const gchar*
 */
void b_device_service_initialize_params_container_set_matter_attestation_trust_store_dir(
    BDeviceServiceInitializeParamsContainer *self,
    const gchar *matterAttestationTrustStoreDir);

/**
 * b_device_service_initialize_params_container_set_account_id
 *
 * @self: BDeviceServiceInitializeParamsContainer*
 * @accountId: const gchar*
 */
void b_device_service_initialize_params_container_set_account_id(BDeviceServiceInitializeParamsContainer *self,
                                                                 const gchar *accountId);

G_END_DECLS