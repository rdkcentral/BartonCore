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
 * Created by Christian Leithner on 8/8/2024.
 */

#include "deviceServiceConfiguration.h"
#include "device-service-initialize-params-container.h"
#include "icConcurrent/threadUtils.h"
#include "icLog/logging.h"
#include <pthread.h>

#define LOG_TAG     "deviceServiceConfiguration"
#define logFmt(fmt) "%s: " fmt, __func__

#define DEFAULT_DS_DIR            "/tmp/barton/device_service"
#define DEFAULT_STORAGE_DIR       DEFAULT_DS_DIR "/storage"
#define DEFAULT_FIRMWARE_FILE_DIR DEFAULT_DS_DIR "/firmware"

static pthread_mutex_t deviceServiceConfigurationMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static BDeviceServiceInitializeParamsContainer *initializeParams = NULL;

bool deviceServiceConfigurationStartup(BDeviceServiceInitializeParamsContainer *_initializeParams)
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

BDeviceServiceTokenProvider *deviceServiceConfigurationGetTokenProvider(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    BDeviceServiceTokenProvider *retVal = NULL;
    g_object_get(initializeParams,
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_TOKEN_PROVIDER],
                 &retVal,
                 NULL);

    return retVal;
}

BDeviceServiceNetworkCredentialsProvider *deviceServiceConfigurationGetNetworkCredentialsProvider(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    BDeviceServiceNetworkCredentialsProvider *retVal = NULL;
    g_object_get(initializeParams,
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_NETWORK_CREDENTIALS_PROVIDER],
                 &retVal,
                 NULL);

    return retVal;
}

BDeviceServicePropertyProvider *deviceServiceConfigurationGetPropertyProvider(void)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return NULL;
    }

    BDeviceServicePropertyProvider *retVal = NULL;
    g_object_get(initializeParams,
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_PROPERTY_PROVIDER],
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
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_STORAGE_DIR],
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
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_FIRMWARE_FILE_DIR],
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
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_STORAGE_DIR],
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
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_MATTER_ATTESTATION_TRUST_STORE_DIR],
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
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID],
                 &retVal,
                 NULL);

    return retVal;
}

void deviceServiceConfigurationSetAccountId(const gchar *accountId)
{
    LOCK_SCOPE(deviceServiceConfigurationMutex);

    if (initializeParams == NULL)
    {
        icError("device service configuration not started");
        return;
    }

    g_object_set(initializeParams,
                 B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER_PROP_ACCOUNT_ID],
                 accountId,
                 NULL);
}