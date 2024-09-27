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

#pragma once

#include "device-service-initialize-params-container.h"
#include "provider/device-service-property-provider.h"
#include "provider/device-service-token-provider.h"
#include <stdbool.h>

/**
 * deviceServiceConfigurationStartup
 *
 * @brief This primes device service configuration with params provided by the client.
 *
 * @param initializeParams - the params to use for device service configuration
 *
 * @return true if successful, otherwise false
 */
bool deviceServiceConfigurationStartup(BDeviceServiceInitializeParamsContainer *initializeParams);

/**
 * deviceServiceConfigurationShutdown
 *
 * @brief This cleans up device service configuration.
 */
void deviceServiceConfigurationShutdown(void);

/**
 * deviceServiceConfigurationGetTokenProvider
 *
 * @brief This retrieves the token provider.
 *
 * @return the token provider
 */
BDeviceServiceTokenProvider *deviceServiceConfigurationGetTokenProvider(void);

/**
 * deviceServiceConfigurationGetNetworkCredentialsProvider
 *
 * @brief This retrieves the network credentials provider.
 *
 * @return the network credentials provider
 */
BDeviceServiceNetworkCredentialsProvider *deviceServiceConfigurationGetNetworkCredentialsProvider(void);

/**
 * deviceServiceConfigurationGetPropertyProvider
 *
 * @brief This retrieves the property provider.
 *
 * @return the property provider
 */
BDeviceServicePropertyProvider *deviceServiceConfigurationGetPropertyProvider(void);

/**
 * deviceServiceConfigurationGetStorageDir
 *
 * @brief This retrieves the storage directory.
 *
 * @return the storage directory
 */
gchar *deviceServiceConfigurationGetStorageDir(void);

/**
 * deviceServiceConfigurationGetFirmwareFileDir
 *
 * @brief This retrieves the firmware file directory.
 *
 * @return the firmware file directory
 */
gchar *deviceServiceConfigurationGetFirmwareFileDir(void);

/**
 * deviceServiceConfigurationGetMatterStorageDir
 *
 * @brief This retrieves the Matter storage directory.
 *
 * @return the Matter storage directory
 */
gchar *deviceServiceConfigurationGetMatterStorageDir(void);

/**
 * deviceServiceConfigurationGetMatterAttestationTrustStoreDir
 *
 * @brief This retrieves the Matter attestation trust store directory.
 *
 * @return the Matter attestation trust store directory
 */
gchar *deviceServiceConfigurationGetMatterAttestationTrustStoreDir(void);

/**
 * deviceServiceConfigurationGetAccountId
 *
 * @brief This retrieves the account ID.
 *
 * @return the account ID
 */
gchar *deviceServiceConfigurationGetAccountId(void);


/**
 * deviceServiceConfigurationSetAccountId
 *
 * @brief This sets the account ID.
 *
 * @param accountId - the account ID
 */
void deviceServiceConfigurationSetAccountId(const gchar *accountId);