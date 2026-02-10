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

#include "barton-core-initialize-params-container.h"
#include "provider/barton-core-default-property-provider.h"
#include "provider/barton-core-property-provider.h"
#include "provider/barton-core-token-provider.h"

struct _BCoreInitializeParamsContainer
{
    GObject parent_instance;

    BCoreTokenProvider *tokenProvider;
    BCoreNetworkCredentialsProvider *networkCredentialsProvider;
    BCorePropertyProvider *propertyProvider;

    gchar *storageDir;
    gchar *firmwareFileDir;
    gchar *matterStorageDir;
    gchar *matterAttestationTrustStoreDir;
    gchar *sbmdDir;
    gchar *accountId;
};

G_DEFINE_TYPE(BCoreInitializeParamsContainer, b_core_initialize_params_container, G_TYPE_OBJECT)

static GParamSpec *properties[N_B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTIES];

static void b_core_initialize_params_container_finalize(GObject *object)
{
    BCoreInitializeParamsContainer *self = B_CORE_INITIALIZE_PARAMS_CONTAINER(object);

    g_clear_object(&self->tokenProvider);
    g_clear_object(&self->networkCredentialsProvider);
    g_clear_object(&self->propertyProvider);

    g_free(self->storageDir);
    g_free(self->firmwareFileDir);
    g_free(self->matterStorageDir);
    g_free(self->matterAttestationTrustStoreDir);
    g_free(self->sbmdDir);
    g_free(self->accountId);

    G_OBJECT_CLASS(b_core_initialize_params_container_parent_class)->finalize(object);
}

static void
b_core_initialize_params_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreInitializeParamsContainer *self = B_CORE_INITIALIZE_PARAMS_CONTAINER(object);

    switch (property_id)
    {
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER:
            g_value_set_object(value, self->tokenProvider);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER:
            g_value_set_object(value, self->networkCredentialsProvider);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER:
            g_value_set_object(value, self->propertyProvider);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR:
            g_value_set_string(value, self->storageDir);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR:
            g_value_set_string(value, self->firmwareFileDir);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR:
            g_value_set_string(value, self->matterStorageDir);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR:
            g_value_set_string(value, self->matterAttestationTrustStoreDir);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIR:
            g_value_set_string(value, self->sbmdDir);
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID:
            g_value_set_string(value, self->accountId);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_initialize_params_set_property(GObject *object,
                                                            guint property_id,
                                                            const GValue *value,
                                                            GParamSpec *pspec)
{
    BCoreInitializeParamsContainer *self = B_CORE_INITIALIZE_PARAMS_CONTAINER(object);

    switch (property_id)
    {
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER:
            b_core_initialize_params_container_set_token_provider(self, g_value_get_object(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER:
            b_core_initialize_params_container_set_network_credentials_provider(self,
                                                                                          g_value_get_object(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER:
            b_core_initialize_params_container_set_property_provider(self, g_value_get_object(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR:
            b_core_initialize_params_container_set_storage_dir(self, g_value_get_string(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR:
            b_core_initialize_params_container_set_firmware_file_dir(self, g_value_get_string(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR:
            b_core_initialize_params_container_set_matter_storage_dir(self, g_value_get_string(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR:
            b_core_initialize_params_container_set_matter_attestation_trust_store_dir(
                self, g_value_get_string(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIR:
            b_core_initialize_params_container_set_sbmd_dir(self, g_value_get_string(value));
            break;
        case B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID:
            b_core_initialize_params_container_set_account_id(self, g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_initialize_params_container_class_init(BCoreInitializeParamsContainerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_initialize_params_set_property;
    object_class->get_property = b_core_initialize_params_get_property;
    object_class->finalize = b_core_initialize_params_container_finalize;

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER] =
        g_param_spec_object(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER],
                            "Token Provider",
                            "The client token provider",
                            B_CORE_TOKEN_PROVIDER_TYPE,
                            G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER] =
        g_param_spec_object(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER],
                            "Network Credentials Provider",
                            "The network credentials provider",
                            B_CORE_NETWORK_CREDENTIALS_PROVIDER_TYPE,
                            G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER] =
        g_param_spec_object(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER],
                            "Property Provider",
                            "The property provider",
                            B_CORE_PROPERTY_PROVIDER_TYPE,
                            G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR] =
        g_param_spec_string(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR],
                            "Storage Directory",
                            "The storage directory for device service",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR] =
        g_param_spec_string(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR],
                            "Firmware File Directory",
                            "The directory for firmware files",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR] =
        g_param_spec_string(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR],
                            "Matter Storage Directory",
                            "The storage directory for externally managed Matter configuration",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR] =
        g_param_spec_string(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR],
                            "Matter Attestation Trust Store Directory",
                            "The directory for Matter attestation trust store",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIR] = g_param_spec_string(
        B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIR],
        "SBMD Directory",
        "The directory for SBMD specification files",
        NULL,
        G_PARAM_READWRITE);

    properties[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID] =
        g_param_spec_string(B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID],
                            "Account ID",
                            "The account ID",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTIES, properties);
}

static void b_core_initialize_params_container_init(BCoreInitializeParamsContainer *self)
{
    self->tokenProvider = NULL;
    self->networkCredentialsProvider = NULL;
    self->propertyProvider = B_CORE_PROPERTY_PROVIDER(b_core_default_property_provider_new());

    self->storageDir = NULL;
    self->firmwareFileDir = NULL;
    self->matterStorageDir = NULL;
    self->matterAttestationTrustStoreDir = NULL;
    self->sbmdDir = NULL;
    self->accountId = NULL;
}

BCoreInitializeParamsContainer *b_core_initialize_params_container_new(void)
{
    return g_object_new(B_CORE_INITIALIZE_PARAMS_CONTAINER_TYPE, NULL);
}

BCoreTokenProvider *
b_core_initialize_params_container_get_token_provider(BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    BCoreTokenProvider *retVal = NULL;
    if (self->tokenProvider)
    {
        retVal = g_object_ref(self->tokenProvider);
    }

    return retVal;
}

BCoreNetworkCredentialsProvider *b_core_initialize_params_container_get_network_credentials_provider(
    BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    BCoreNetworkCredentialsProvider *retVal = NULL;
    if (self->networkCredentialsProvider)
    {
        retVal = g_object_ref(self->networkCredentialsProvider);
    }

    return retVal;
}

BCorePropertyProvider *
b_core_initialize_params_container_get_property_provider(BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    BCorePropertyProvider *retVal = NULL;
    if (self->propertyProvider)
    {
        retVal = g_object_ref(self->propertyProvider);
    }

    return retVal;
}

gchar *b_core_initialize_params_container_get_storage_dir(BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return g_strdup(self->storageDir);
}

gchar *b_core_initialize_params_container_get_firmware_file_dir(BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return g_strdup(self->firmwareFileDir);
}

gchar *
b_core_initialize_params_container_get_matter_storage_dir(BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return g_strdup(self->matterStorageDir);
}

gchar *b_core_initialize_params_container_get_matter_attestation_trust_store_dir(
    BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return g_strdup(self->matterAttestationTrustStoreDir);
}

gchar *b_core_initialize_params_container_get_sbmd_dir(BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return g_strdup(self->sbmdDir);
}

gchar *b_core_initialize_params_container_get_account_id(BCoreInitializeParamsContainer *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return g_strdup(self->accountId);
}

gboolean b_core_initialize_params_container_set_token_provider(BCoreInitializeParamsContainer *self,
                                                                         BCoreTokenProvider *tokenProvider)
{
    g_return_val_if_fail(self != NULL, FALSE);

    g_clear_object(&self->tokenProvider);

    self->tokenProvider = tokenProvider ? g_object_ref(tokenProvider) : NULL;

    return TRUE;
}

gboolean
b_core_initialize_params_container_set_property_provider(BCoreInitializeParamsContainer *self,
                                                                   BCorePropertyProvider *propertyProvider)
{
    g_return_val_if_fail(self != NULL, FALSE);

    g_clear_object(&self->propertyProvider);

    self->propertyProvider = propertyProvider ? g_object_ref(propertyProvider) : NULL;

    return TRUE;
}

gboolean b_core_initialize_params_container_set_network_credentials_provider(
    BCoreInitializeParamsContainer *self,
    BCoreNetworkCredentialsProvider *networkCredentialsProvider)
{
    g_return_val_if_fail(self != NULL, FALSE);

    g_clear_object(&self->networkCredentialsProvider);

    self->networkCredentialsProvider = networkCredentialsProvider ? g_object_ref(networkCredentialsProvider) : NULL;

    return TRUE;
}

void b_core_initialize_params_container_set_storage_dir(BCoreInitializeParamsContainer *self,
                                                                  const gchar *storageDir)
{
    g_return_if_fail(self != NULL);

    g_free(self->storageDir);
    self->storageDir = g_strdup(storageDir);
}

void b_core_initialize_params_container_set_firmware_file_dir(BCoreInitializeParamsContainer *self,
                                                                        const gchar *firmwareFileDir)
{
    g_return_if_fail(self != NULL);

    g_free(self->firmwareFileDir);
    self->firmwareFileDir = g_strdup(firmwareFileDir);
}

void b_core_initialize_params_container_set_matter_storage_dir(BCoreInitializeParamsContainer *self,
                                                                         const gchar *matterStorageDire)
{
    g_return_if_fail(self != NULL);

    g_free(self->matterStorageDir);
    self->matterStorageDir = g_strdup(matterStorageDire);
}

void b_core_initialize_params_container_set_matter_attestation_trust_store_dir(
    BCoreInitializeParamsContainer *self,
    const gchar *matterAttestationTrustStoreDir)
{
    g_return_if_fail(self != NULL);

    g_free(self->matterAttestationTrustStoreDir);
    self->matterAttestationTrustStoreDir = g_strdup(matterAttestationTrustStoreDir);
}

void b_core_initialize_params_container_set_sbmd_dir(BCoreInitializeParamsContainer *self, const gchar *sbmdDir)
{
    g_return_if_fail(self != NULL);

    g_free(self->sbmdDir);
    self->sbmdDir = g_strdup(sbmdDir);
}

void b_core_initialize_params_container_set_account_id(BCoreInitializeParamsContainer *self,
                                                                 const gchar *accountId)
{
    g_return_if_fail(self != NULL);

    g_free(self->accountId);
    self->accountId = g_strdup(accountId);
}
