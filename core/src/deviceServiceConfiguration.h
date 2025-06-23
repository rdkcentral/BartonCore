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

#pragma once

#include "barton-core-initialize-params-container.h"
#include "provider/barton-core-property-provider.h"
#include "provider/barton-core-token-provider.h"
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
bool deviceServiceConfigurationStartup(BCoreInitializeParamsContainer *initializeParams);

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
BCoreTokenProvider *deviceServiceConfigurationGetTokenProvider(void);

/**
 * deviceServiceConfigurationGetNetworkCredentialsProvider
 *
 * @brief This retrieves the network credentials provider.
 *
 * @return the network credentials provider
 */
BCoreNetworkCredentialsProvider *deviceServiceConfigurationGetNetworkCredentialsProvider(void);

/**
 * deviceServiceConfigurationGetPropertyProvider
 *
 * @brief This retrieves the property provider.
 *
 * @return the property provider
 */
BCorePropertyProvider *deviceServiceConfigurationGetPropertyProvider(void);

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
 * @return true if successful, otherwise false
 */
bool deviceServiceConfigurationSetAccountId(const gchar *accountId);

/**
 * Callback type for account id changes
 */
typedef void (*deviceServiceConfigurationAccountIdListener)(const gchar *accountId);

/**
 * deviceServiceConfigurationRegisterAccountIdListener
 *
 * @brief Add listener for account id changes
 *
 * @param listener - the callback to be invoked
 * @return true if successful, otherwise false
 */
bool deviceServiceConfigurationRegisterAccountIdListener(deviceServiceConfigurationAccountIdListener listener);

/**
 * deviceServiceConfigurationUnregisterAccountIdListener
 *
 * @brief Remove listener for account id changes
 *
 * @param listener - the callback to be removed
 * @return true if successful, otherwise false
 */
bool deviceServiceConfigurationUnregisterAccountIdListener(deviceServiceConfigurationAccountIdListener listener);
