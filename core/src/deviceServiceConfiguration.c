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
 * Created by Christian Leithner on 8/8/2024.
 */

#include "deviceServiceConfiguration.h"
#include "barton-core-initialize-params-container.h"
#include "delegate/barton-core-software-watchdog-delegate.h"
#include "glib-object.h"
#include "glib.h"
#include "icConcurrent/threadUtils.h"
#include "icLog/logging.h"
#include <pthread.h>

#define LOG_TAG                   "deviceServiceConfiguration"
#define logFmt(fmt)               "%s: " fmt, __func__

#define DEFAULT_DS_DIR            "/tmp/barton/device_service"
#define DEFAULT_STORAGE_DIR       DEFAULT_DS_DIR "/storage"
#define DEFAULT_FIRMWARE_FILE_DIR DEFAULT_DS_DIR "/firmware"

static void accountIdChangeCallback(GObject *object, GParamSpec *pspec, gpointer userData);

static pthread_mutex_t deviceServiceConfigurationMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static BCoreInitializeParamsContainer *initializeParams = NULL;

bool deviceServiceConfigurationStartup(BCoreInitializeParamsContainer *_initializeParams)
{
    g_return_val_if_fail(_initializeParams != NULL, false);

    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams != NULL)
    {
        icError("device service configuration already started. Call deviceServiceConfigurationShutdown first");
        return false;
    }

    initializeParams = g_object_ref(_initializeParams);

    return true;
}

void deviceServiceConfigurationShutdown(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return;
    }

    g_clear_object(&initializeParams);
}

BCoreTokenProvider *deviceServiceConfigurationGetTokenProvider(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    BCoreTokenProvider *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER],
                 &retVal,
                 NULL);

    return retVal;
}

BCoreNetworkCredentialsProvider *deviceServiceConfigurationGetNetworkCredentialsProvider(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    BCoreNetworkCredentialsProvider *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER],
                 &retVal,
                 NULL);

    return retVal;
}

BCorePropertyProvider *deviceServiceConfigurationGetPropertyProvider(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    BCorePropertyProvider *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER],
                 &retVal,
                 NULL);

    return retVal;
}

BCoreSoftwareWatchdogDelegate *deviceServiceConfigurationGetSoftwareWatchdogDelegate(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    BCoreSoftwareWatchdogDelegate *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SOFTWARE_WATCHDOG_DELEGATE],
                 &retVal,
                 NULL);

    return retVal;
}

gchar *deviceServiceConfigurationGetStorageDir(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    gchar *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR],
                 &retVal,
                 NULL);

    if (retVal == NULL)
    {
        retVal = g_strdup(DEFAULT_STORAGE_DIR);
    }

    return retVal;
}

gchar *deviceServiceConfigurationGetFirmwareFileDir(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    gchar *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR],
                 &retVal,
                 NULL);

    if (retVal == NULL)
    {
        retVal = g_strdup(DEFAULT_FIRMWARE_FILE_DIR);
    }

    return retVal;
}

gchar *deviceServiceConfigurationGetMatterStorageDir(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    gchar *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR],
                 &retVal,
                 NULL);

    return retVal;
}

gchar *deviceServiceConfigurationGetMatterAttestationTrustStoreDir(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    gchar *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR],
                 &retVal,
                 NULL);

    return retVal;
}

gchar *deviceServiceConfigurationGetAccountId(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    gchar *retVal = NULL;
    g_object_get(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID],
                 &retVal,
                 NULL);

    return retVal;
}

bool deviceServiceConfigurationSetAccountId(const gchar *accountId)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return false;
    }

    g_object_set(initializeParams,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID],
                 accountId,
                 NULL);

    return true;
}

bool deviceServiceConfigurationRegisterAccountIdListener(deviceServiceConfigurationAccountIdListener listener)
{
    g_return_val_if_fail(listener != NULL, false);

    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return false;
    }

    g_autofree gchar *signalName = g_strdup_printf("notify::%s",
                                                   B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                                                       [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID]);

    g_signal_connect(initializeParams, signalName, G_CALLBACK(accountIdChangeCallback), listener);

    return true;
}

bool deviceServiceConfigurationUnregisterAccountIdListener(deviceServiceConfigurationAccountIdListener listener)
{
    g_return_val_if_fail(listener != NULL, false);

    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return false;
    }

    g_signal_handlers_disconnect_by_func(initializeParams, G_CALLBACK(accountIdChangeCallback), listener);

    return true;
}

static void accountIdChangeCallback(GObject *object, GParamSpec *pspec, gpointer userData)
{
    g_return_if_fail(object != NULL);
    g_return_if_fail(userData != NULL);

    g_autofree gchar *accountId = NULL;
    g_object_get(object,
                 B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID],
                 &accountId,
                 NULL);

    deviceServiceConfigurationAccountIdListener listener = (deviceServiceConfigurationAccountIdListener) userData;
    if (listener != NULL)
    {
        listener(accountId);
    }
}
