// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// ------------------------------ tabstop = 4 ----------------------------------

/*
 * Code adapted from: https://github.com/project-chip/connectedhomeip
 * Modified by Comcast for use in the Barton project
 * Copyright 2022 Project CHIP Authors
 * All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 */

//
// Created by Raiyan Chowdhury on 2/15/2025.
//

#include "DefaultCommissionableDataProvider.hpp"
#include "BartonMatterProviderRegistry.hpp"
#include "crypto/CHIPCryptoPAL.h"
#include "glib.h"
#include "lib/core/CHIPError.h"
#include "lib/support/Base64.h"
#include "lib/support/CodeUtils.h"
#include "lib/support/Span.h"
#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" {
#include <barton-core-properties.h>
#include <provider/barton-core-property-provider.h>
#include <deviceServiceConfiguration.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
}

#define LOG_TAG     "CommishDataProvider"
#define logFmt(fmt) "(%s): " fmt, __func__

using namespace ::barton;
using namespace chip;

namespace
{
    std::shared_ptr<DefaultCommissionableDataProvider> commissionableDataProvider =
        std::make_shared<DefaultCommissionableDataProvider>();
    auto providerRegistered = BartonMatterProviderRegistry::Instance().RegisterBartonCommissionableDataProvider(
        commissionableDataProvider);
}

CHIP_ERROR DefaultCommissionableDataProvider::RetrieveSetupDiscriminator(uint16_t &setupDiscriminator)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    VerifyOrReturnError(propertyProvider != nullptr, CHIP_ERROR_INTERNAL, icError("Property provider is null"));

    guint16 discriminator = b_core_property_provider_get_property_as_uint16(
        propertyProvider, B_CORE_BARTON_MATTER_SETUP_DISCRIMINATOR, UINT16_MAX);

    VerifyOrReturnError(
        discriminator != UINT16_MAX, CHIP_ERROR_INTERNAL, icError("Failed to read setup discriminator property"));

    setupDiscriminator = discriminator;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DefaultCommissionableDataProvider::RetrieveSpake2pIterationCount(uint32_t &iterationCount)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    VerifyOrReturnError(propertyProvider != nullptr, CHIP_ERROR_INTERNAL, icError("Property provider is null"));

    guint32 iteration = b_core_property_provider_get_property_as_uint32(
        propertyProvider, B_CORE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, UINT32_MAX);

    VerifyOrReturnError(
        iteration != UINT32_MAX, CHIP_ERROR_INTERNAL, icError("Failed to read spake2p iteration count property"));

    iterationCount = iteration;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DefaultCommissionableDataProvider::RetrieveSpake2pSalt(MutableByteSpan &saltBuf, size_t &outSaltLen)
{
    VerifyOrReturnError(saltBuf.size() >= Crypto::kSpake2p_Max_PBKDF_Salt_Length, CHIP_ERROR_BUFFER_TOO_SMALL);

    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    VerifyOrReturnError(propertyProvider != nullptr, CHIP_ERROR_INTERNAL, icError("Property provider is null"));

    g_autofree gchar *base64Salt = b_core_property_provider_get_property_as_string(
        propertyProvider, B_CORE_BARTON_MATTER_SPAKE2P_SALT, nullptr);

    VerifyOrReturnError(base64Salt != nullptr, CHIP_ERROR_INTERNAL, icError("Failed to read spake2p salt property"));

    VerifyOrReturnError(Base64StringToMutableByteSpan(base64Salt, saltBuf, outSaltLen), CHIP_ERROR_INTERNAL);

    return CHIP_NO_ERROR;
}

CHIP_ERROR DefaultCommissionableDataProvider::RetrieveSpake2pVerifier(MutableByteSpan &verifierBuf,
                                                                      size_t &outVerifierLen)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    VerifyOrReturnError(propertyProvider != nullptr, CHIP_ERROR_INTERNAL, icError("Property provider is null"));

    VerifyOrReturnError(verifierBuf.size() >= Crypto::kSpake2p_VerifierSerialized_Length, CHIP_ERROR_BUFFER_TOO_SMALL);

    g_autofree gchar *base64Verifier = b_core_property_provider_get_property_as_string(
        propertyProvider, B_CORE_BARTON_MATTER_SPAKE2P_VERIFIER, nullptr);

    VerifyOrReturnError(
        base64Verifier != nullptr, CHIP_ERROR_INTERNAL, icError("Failed to read spake2p verifier property"));

    VerifyOrReturnError(Base64StringToMutableByteSpan(base64Verifier, verifierBuf, outVerifierLen),
                        CHIP_ERROR_INTERNAL);

    return CHIP_NO_ERROR;
}

CHIP_ERROR DefaultCommissionableDataProvider::RetrieveSetupPasscode(uint32_t &setupPasscode)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    VerifyOrReturnError(propertyProvider != nullptr, CHIP_ERROR_INTERNAL, icError("Property provider is null"));

    guint32 passcode = b_core_property_provider_get_property_as_uint32(
        propertyProvider, B_CORE_BARTON_MATTER_SETUP_PASSCODE, UINT32_MAX);

    VerifyOrReturnError(passcode != UINT32_MAX, CHIP_ERROR_INTERNAL, icError("Failed to read setup passcode property"));

    setupPasscode = passcode;
    return CHIP_NO_ERROR;
}

bool DefaultCommissionableDataProvider::Base64StringToMutableByteSpan(const char *base64String,
                                                                      chip::MutableByteSpan &outBuf,
                                                                      size_t &outBufLen)
{
    size_t maxPossibleSize = BASE64_MAX_DECODED_LEN(strlen(base64String));
    std::vector<uint8_t> tempBuf(maxPossibleSize);
    size_t decodedSize = Base64Decode(base64String, strlen(base64String), tempBuf.data());

    if (decodedSize == UINT16_MAX)
    {
        icError("Failed to decode the provided base64 string");
        return false;
    }

    if (decodedSize > outBuf.size())
    {
        icError("Size of decoded base64 string is larger than the provided buffer");
        return false;
    }

    memcpy(outBuf.data(), tempBuf.data(), decodedSize);
    outBuf.reduce_size(decodedSize);
    outBufLen = outBuf.size();

    return true;
}
