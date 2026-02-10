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
 * Created by Christian Leithner on 7/30/2024.
 */

#pragma once

#include "provider/barton-core-network-credentials-provider.h"
#include "provider/barton-core-property-provider.h"
#include "provider/barton-core-token-provider.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_INITIALIZE_PARAMS_CONTAINER_TYPE (b_core_initialize_params_container_get_type())
G_DECLARE_FINAL_TYPE(BCoreInitializeParamsContainer,
                     b_core_initialize_params_container,
                     B_CORE,
                     INITIALIZE_PARAMS_CONTAINER,
                     GObject);

typedef enum
{
    // providers
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER = 1,
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER,
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER,

    // configuration
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR,
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR,
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR,
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR,
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIR,
    B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID,

    N_B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTIES
} BCoreInitializeParamsContainerProperty;

static const gchar *B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES[] = {
    NULL,
    "token-provider",
    "network-credentials-provider",
    "property-provider",
    "storage-dir",
    "firmware-file-dir",
    "matter-storage-dir",
    "matter-attestation-trust-store-dir",
    "sbmd-dir",
    "account-id",
};

/**
 * b_core_initialize_params_container_new
 *
 * @brief
 *
 * Returns: (transfer full): BCoreInitializeParamsContainer*
 */
BCoreInitializeParamsContainer *b_core_initialize_params_container_new(void);

/**
 * b_core_initialize_params_container_get_token_provider
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The token provider for device service
 */
BCoreTokenProvider *
b_core_initialize_params_container_get_token_provider(BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_network_credentials_provider
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The network credentials provider for device service
 */
BCoreNetworkCredentialsProvider *b_core_initialize_params_container_get_network_credentials_provider(
    BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_property_provider
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The property provider for device service
 */
BCorePropertyProvider *
b_core_initialize_params_container_get_property_provider(BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_storage_dir
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The storage directory for device service configuration
 */
gchar *b_core_initialize_params_container_get_storage_dir(BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_firmware_file_dir
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The directory device service will install firmware files
 */
gchar *
b_core_initialize_params_container_get_firmware_file_dir(BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_matter_storage_dir
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The storage directory for externally managed Matter configuration
 */
gchar *
b_core_initialize_params_container_get_matter_storage_dir(BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_matter_attestation_trust_store_dir
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The directory device service will use as an atttestation trust store for Matter. Supported
 * Product Attestation Authorities should be installed there.
 */
gchar *b_core_initialize_params_container_get_matter_attestation_trust_store_dir(
    BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_sbmd_dir
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The directory device service will use for SBMD specification files.
 */
gchar *b_core_initialize_params_container_get_sbmd_dir(BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_get_account_id
 * @self: BCoreInitializeParamsContainer*
 *
 * Returns: (transfer full): The account ID for device service
 */
gchar *b_core_initialize_params_container_get_account_id(BCoreInitializeParamsContainer *self);

/**
 * b_core_initialize_params_container_set_token_provider
 * @self: BCoreInitializeParamsContainer*
 * @tokenProvider: BCoreTokenProvider*
 *
 * Returns: whether or not the token provider was set
 */
gboolean b_core_initialize_params_container_set_token_provider(BCoreInitializeParamsContainer *self,
                                                                         BCoreTokenProvider *tokenProvider);

/**
 * b_core_initialize_params_container_set_property_provider
 * @self: BCoreInitializeParamsContainer*
 * @propertyProvider: BCorePropertyProvider*
 *
 * Returns: whether or not the property provider was set
 */
gboolean
b_core_initialize_params_container_set_property_provider(BCoreInitializeParamsContainer *self,
                                                                   BCorePropertyProvider *propertyProvider);

/**
 * b_core_initialize_params_container_set_network_credentials_provider
 * @self: BCoreInitializeParamsContainer*
 * @networkCredentialsProvider: BCoreNetworkCredentialsProvider*
 *
 * Returns: whether or not the network credentials provider was set
 */
gboolean b_core_initialize_params_container_set_network_credentials_provider(
    BCoreInitializeParamsContainer *self,
    BCoreNetworkCredentialsProvider *networkCredentialsProvider);

/**
 * b_core_initialize_params_container_set_storage_dir
 * @self: BCoreInitializeParamsContainer*
 * @storageDir: const gchar*
 */
void b_core_initialize_params_container_set_storage_dir(BCoreInitializeParamsContainer *self,
                                                                  const gchar *storageDir);

/**
 * b_core_initialize_params_container_set_firmware_file_dir
 * @self: BCoreInitializeParamsContainer*
 * @firmwareFileDir: const gchar*
 */
void b_core_initialize_params_container_set_firmware_file_dir(BCoreInitializeParamsContainer *self,
                                                                        const gchar *firmwareFileDir);

/**
 * b_core_initialize_params_container_set_matter_storage_dir
 * @self: BCoreInitializeParamsContainer*
 * @matterStorageDire: const gchar*
 */
void b_core_initialize_params_container_set_matter_storage_dir(BCoreInitializeParamsContainer *self,
                                                                         const gchar *matterStorageDire);

/**
 * b_core_initialize_params_container_set_matter_attestation_trust_store_dir
 * @self: BCoreInitializeParamsContainer*
 * @matterAttestationTrustStoreDir: const gchar*
 */
void b_core_initialize_params_container_set_matter_attestation_trust_store_dir(
    BCoreInitializeParamsContainer *self,
    const gchar *matterAttestationTrustStoreDir);

/**
 * b_core_initialize_params_container_set_sbmd_dir
 * @self: BCoreInitializeParamsContainer*
 * @sbmdDir: const gchar*
 */
void b_core_initialize_params_container_set_sbmd_dir(BCoreInitializeParamsContainer *self, const gchar *sbmdDir);

/**
 * b_core_initialize_params_container_set_account_id
 * @self: BCoreInitializeParamsContainer*
 * @accountId: const gchar*
 */
void b_core_initialize_params_container_set_account_id(BCoreInitializeParamsContainer *self,
                                                                 const gchar *accountId);

G_END_DECLS
