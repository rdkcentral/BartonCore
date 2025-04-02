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

#include "CustomCommissionableDataProvider.hpp"
#include "crypto/CHIPCryptoPAL.h"
#include "glib.h"
#include "lib/core/CHIPError.h"
#include "lib/support/CodeUtils.h"
#include "lib/support/Span.h"
#include "lib/support/Base64.h"
#include "setup_payload/SetupPayload.h"
#include <provider/device-service-property-provider.h>
#include <cstdint>

extern "C" {
#include <device-service-properties.h>
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
    bool IsValidVerifier(const gchar *base64Verifier)
    {
        // This max possible size might exceed the valid verifier length, but the actual size might still be within
        // the acceptable bounds, so we'll still check it. However, if the max possible size is too small, then there's
        // no chance that the verifier is valid.
        size_t maxPossibleVerifierSize = BASE64_MAX_DECODED_LEN(strlen(base64Verifier));

        if (maxPossibleVerifierSize < Crypto::kSpake2p_VerifierSerialized_Length)
        {
            icError("Size of provided verifier is invalid: %zu", maxPossibleVerifierSize);
            return false;
        }

        std::vector<uint8_t> verifierVector(maxPossibleVerifierSize);
        size_t verifierSize = Base64Decode(base64Verifier, strlen(base64Verifier), verifierVector.data());

        if (verifierSize == UINT16_MAX)
        {
            icError("Failed to decode the provided base64 verifier");
            return false;
        }

        if (verifierSize != Crypto::kSpake2p_VerifierSerialized_Length)
        {
            icError("Size of provided verifier is invalid: %zu", verifierSize);
            return false;
        }

        Crypto::Spake2pVerifier verifier;
        ByteSpan verifierSpan {verifierVector.data(), verifierVector.size()};
        CHIP_ERROR err = verifier.Deserialize(verifierSpan);

        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to deserialize the provided verifier: %" CHIP_ERROR_FORMAT, err.Format());
            return false;
        }

        return true;
    }

    bool IsValidSalt(const gchar *base64Salt)
    {
        // This max possible size might exceed the max salt length allowed, but the actual size might still be within
        // the acceptable bounds, so we'll still check it. However, if the max possible size is too small, then there's
        // no chance that the salt is valid.
        size_t maxPossibleSaltSize = BASE64_MAX_DECODED_LEN(strlen(base64Salt));

        if (maxPossibleSaltSize < Crypto::kSpake2p_Min_PBKDF_Salt_Length)
        {
            icError("Size of provided salt is invalid: %zu", maxPossibleSaltSize);
            return false;
        }

        std::vector<uint8_t> saltVector(maxPossibleSaltSize);
        size_t saltSize = Base64Decode(base64Salt, strlen(base64Salt), saltVector.data());

        if (saltSize == UINT16_MAX)
        {
            icError("Failed to decode the provided base64 salt");
            return false;
        }

        if (saltSize < Crypto::kSpake2p_Min_PBKDF_Salt_Length || saltSize > Crypto::kSpake2p_Max_PBKDF_Salt_Length)
        {
            icError("Size of provided salt is invalid: %zu", saltSize);
            return false;
        }

        return true;
    }
} // namespace

CHIP_ERROR CustomCommissionableDataProvider::Init(Optional<uint16_t> discriminator,
                                                  uint32_t defaultSpake2pIterationCount)
{
    VerifyOrReturnError(!mIsInitialized, CHIP_ERROR_INCORRECT_STATE);

    ReturnErrorOnFailure(InitDiscriminator(discriminator));

    ReturnErrorOnFailure(InitSpake2pIterationCount(defaultSpake2pIterationCount));

    // Check to see if the client has externally provided the remaining values. If so, validate them first before
    // dealing with them.
    g_autofree gchar *providedBase64Verifier = b_device_service_property_provider_get_property_as_string(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, nullptr);

    if (providedBase64Verifier != nullptr && !IsValidVerifier(providedBase64Verifier))
    {
        icError("Provided spake2p verifier is invalid: %s; validate inputs", providedBase64Verifier);
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    g_autofree gchar *providedBase64Salt = b_device_service_property_provider_get_property_as_string(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, nullptr);

    if (providedBase64Salt != nullptr && !IsValidSalt(providedBase64Salt))
    {
        icError("Provided spake2p salt is invalid: %s; validate inputs", providedBase64Salt);
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    guint32 providedPasscode = b_device_service_property_provider_get_property_as_uint32(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, UINT32_MAX);

    if (providedPasscode != UINT32_MAX && !SetupPayload::IsValidSetupPIN(providedPasscode))
    {
        icError("Provided passcode is invalid: %u; validate inputs", providedPasscode);
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // If the client has externally provided the spake2p verifier, then they must have also externally provided a
    // passcode and spake2p salt, as the spake2p verifier is derived from those values -- see Matter 1.4 spec 3.10.
    if (providedBase64Verifier != nullptr && (providedBase64Salt == nullptr || providedPasscode == UINT32_MAX))
    {
        icError("Spake2p verifier provided, but salt and/or passcode are missing; both must be provided by setting "
                "the %s and %s properties if the verifier has been provided",
                B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT,
                B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE);
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // If the client has not provided the verifier, but has provided the salt and passcode, then we can generate the
    // verifier from those values. If the client has not provided the salt or passcode or both, then we can still
    // generate those values and then use them to generate the verifier. As per Matter 1.4 spec 5.1.7., a device can
    // support dynamic passcodes. The Matter SDK's LinuxCommissionableDataProvider implementation also supports the
    // generation of a random salt, which we will follow here as needed.
    // If the client has provided the verifier, then we will validate it by generating a verifier from the provided salt
    // and passcode and ensuring that it matches the provided verifier.

    // This will store the decoded salt that will be used to generate a verifier
    std::vector<uint8_t> spake2pSaltVector(Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    CHIP_ERROR err = CHIP_NO_ERROR;

    if (providedBase64Salt == nullptr)
    {
        ReturnErrorOnFailure(InitRandomSpake2pSalt(spake2pSaltVector));
    }
    else
    {
        size_t saltSize = Base64Decode(providedBase64Salt, strlen(providedBase64Salt), spake2pSaltVector.data());

        if (saltSize == UINT16_MAX)
        {
            icError("Failed to decode the provided base64 salt");
            return CHIP_ERROR_INTERNAL;
        }

        spake2pSaltVector.resize(saltSize);
    }

    uint32_t setupPasscode;
    if (providedPasscode == UINT32_MAX)
    {
        ReturnErrorOnFailure(InitRandomSetupPasscode(setupPasscode));
    }
    else
    {
        setupPasscode = providedPasscode;
    }

    ReturnErrorOnFailure(InitSpake2pVerifier(spake2pSaltVector, setupPasscode, providedBase64Verifier));

    mIsInitialized = true;
    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::GetSetupDiscriminator(uint16_t &setupDiscriminator)
{
    VerifyOrReturnError(mIsInitialized, CHIP_ERROR_UNINITIALIZED);

    guint16 discriminator = b_device_service_property_provider_get_property_as_uint16(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, UINT16_MAX);

    if (discriminator == UINT16_MAX)
    {
        icError("Failed to read setup discriminator property");
        return CHIP_ERROR_INTERNAL;
    }

    setupDiscriminator = discriminator;
    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::SetSetupDiscriminator(uint16_t setupDiscriminator)
{
    (void) setupDiscriminator;
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR CustomCommissionableDataProvider::GetSpake2pIterationCount(uint32_t &iterationCount)
{
    VerifyOrReturnError(mIsInitialized, CHIP_ERROR_UNINITIALIZED);

    guint32 iteration = b_device_service_property_provider_get_property_as_uint32(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, UINT32_MAX);

    if (iteration == UINT32_MAX)
    {
        icError("Failed to read spake2p iteration count property");
        return CHIP_ERROR_INTERNAL;
    }

    iterationCount = iteration;
    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::GetSpake2pSalt(MutableByteSpan &saltBuf)
{
    VerifyOrReturnError(mIsInitialized, CHIP_ERROR_UNINITIALIZED);

    VerifyOrReturnError(saltBuf.size() >= Crypto::kSpake2p_Max_PBKDF_Salt_Length, CHIP_ERROR_BUFFER_TOO_SMALL);

    g_autofree gchar *base64Salt = b_device_service_property_provider_get_property_as_string(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, nullptr);
    if (base64Salt == nullptr)
    {
        icError("Failed to read spake2p salt property");
        return CHIP_ERROR_INTERNAL;
    }

    // The max possible size might be greater than the actual size of the salt. The salt should be of valid length at
    // this point, but just in case, we'll allocate a buffer of the max possible decoded length.
    size_t maxPossibleSize = BASE64_MAX_DECODED_LEN(strlen(base64Salt));
    std::vector<uint8_t> tempSaltVector(maxPossibleSize);
    size_t saltSize = Base64Decode(base64Salt, strlen(base64Salt), tempSaltVector.data());

    // Upon confirming that the salt can fit in the provided buffer, we can copy the data over.
    VerifyOrReturnError(saltSize <= saltBuf.size(), CHIP_ERROR_INTERNAL);
    memcpy(saltBuf.data(), tempSaltVector.data(), saltSize);
    saltBuf.reduce_size(saltSize);
    return CHIP_NO_ERROR;

    return CHIP_ERROR_INTERNAL;
}

CHIP_ERROR CustomCommissionableDataProvider::GetSpake2pVerifier(MutableByteSpan &verifierBuf, size_t &outVerifierLen)
{
    VerifyOrReturnError(mIsInitialized, CHIP_ERROR_UNINITIALIZED);

    VerifyOrReturnError(verifierBuf.size() >= Crypto::kSpake2p_VerifierSerialized_Length, CHIP_ERROR_BUFFER_TOO_SMALL);

    g_autofree gchar *base64Verifier = b_device_service_property_provider_get_property_as_string(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, nullptr);
    if (base64Verifier == nullptr)
    {
        icError("Failed to get spake2p verifier");
        return CHIP_ERROR_INTERNAL;
    }

    // The max possible size might be greater than the actual size of the verifier. The verifier should be of valid
    // length at this point, but just in case, we'll allocate a buffer of the max possible decoded length.
    size_t maxPossibleSize = BASE64_MAX_DECODED_LEN(strlen(base64Verifier));
    std::vector<uint8_t> tempVerifierVector(maxPossibleSize);
    size_t verifierSize = Base64Decode(base64Verifier, strlen(base64Verifier), tempVerifierVector.data());

    // Upon confirming that the verifier can fit in the provided buffer, we can copy the data over.
    VerifyOrReturnError(verifierSize <= verifierBuf.size(), CHIP_ERROR_INTERNAL);
    memcpy(verifierBuf.data(), tempVerifierVector.data(), verifierSize);
    verifierBuf.reduce_size(verifierSize);
    outVerifierLen = verifierBuf.size();
    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::GetSetupPasscode(uint32_t &setupPasscode)
{
    VerifyOrReturnError(mIsInitialized, CHIP_ERROR_UNINITIALIZED);

    guint32 passcode = b_device_service_property_provider_get_property_as_uint32(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, UINT32_MAX);

    if (passcode == UINT32_MAX)
    {
        icError("Failed to get setup passcode");
        return CHIP_ERROR_INTERNAL;
    }

    setupPasscode = passcode;
    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::SetSetupPasscode(uint32_t setupPasscode)
{
    (void) setupPasscode;
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR CustomCommissionableDataProvider::InitDiscriminator(Optional<uint16_t> discriminator)
{
    uint16_t discriminatorValue;

    if (discriminator.HasValue())
    {
        discriminatorValue = discriminator.Value();
    }
    else
    {
        // If the Optional has no value, then that means the discriminator value should already be set in the property
        // provider.
        discriminatorValue = b_device_service_property_provider_get_property_as_uint16(
            mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, UINT16_MAX);

        if (discriminatorValue == UINT16_MAX)
        {
            icError("Discriminator value not provided. Please provide a valid discriminator by setting the %s property",
                    B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR);
            return CHIP_ERROR_INVALID_ARGUMENT;
        }
    }

    if (discriminatorValue > chip::kMaxDiscriminatorValue)
    {
        icError("Discriminator value invalid: %u", discriminatorValue);
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    if (!b_device_service_property_provider_set_property_uint16(
            mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, discriminatorValue))
    {
        icError("Failed to set the discriminator property");
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::InitSpake2pIterationCount(uint32_t defaultSpake2pIterationCount)
{
    // First check if the client has externally provided the spake2p iteration count, else fall back on the default.
    uint32_t iterationCount = b_device_service_property_provider_get_property_as_uint32(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, UINT32_MAX);

    if (iterationCount == UINT32_MAX)
    {
        icDebug("No spake2p iteration count provided, using default value: %u", defaultSpake2pIterationCount);
        iterationCount = defaultSpake2pIterationCount;
    }

    if ((iterationCount < Crypto::kSpake2p_Min_PBKDF_Iterations) ||
        (iterationCount > Crypto::kSpake2p_Max_PBKDF_Iterations))
    {
        icError("Spake2p iteration count invalid: %u", iterationCount);
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    if (!b_device_service_property_provider_set_property_uint32(
            mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, iterationCount))
    {
        icError("Failed to set the spake2p iteration count property");
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::InitRandomSpake2pSalt(std::vector<uint8_t> &outSpake2pSaltVector)
{
    // Generate a random spake2p salt
    outSpake2pSaltVector.resize(Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    CHIP_ERROR err = Crypto::DRBG_get_bytes(outSpake2pSaltVector.data(), outSpake2pSaltVector.size());

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to generate Spake2p salt: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    // The salt property requires a base64-encoded string value
    size_t base64EncodedSaltLen = BASE64_ENCODED_LEN(outSpake2pSaltVector.size());
    scoped_generic char *base64EncodedSalt = (char *) malloc(base64EncodedSaltLen + 1);
    Base64Encode(outSpake2pSaltVector.data(), outSpake2pSaltVector.size(), base64EncodedSalt);
    base64EncodedSalt[base64EncodedSaltLen] = '\0';

    if (!b_device_service_property_provider_set_property_string(
            mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, base64EncodedSalt))
    {
        icError("Failed to set the spake2p salt property");
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::InitRandomSetupPasscode(uint32_t &outSetupPasscode)
{
    // Generate a random setup passcode
    CHIP_ERROR err = Crypto::DRBG_get_bytes(reinterpret_cast<uint8_t *>(&outSetupPasscode), sizeof(outSetupPasscode));

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to generate setup passcode: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    // ensure that the passcode is valid, as per Matter 1.4 spec 5.1.7.1.
    outSetupPasscode = (outSetupPasscode % chip::kSetupPINCodeMaximumValue) + 1;

    if (outSetupPasscode % 11111111 == 0 || outSetupPasscode == 12345678 || outSetupPasscode == 87654321)
    {
        outSetupPasscode++;
    }

    if (!b_device_service_property_provider_set_property_uint32(
            mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, outSetupPasscode))
    {
        icError("Failed to set the setup passcode property");
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR CustomCommissionableDataProvider::InitSpake2pVerifier(const std::vector<uint8_t> &saltVector,
                                                                 const uint32_t setupPasscode,
                                                                 const gchar *providedBase64Verifier)
{
    // Generate a verifier from the salt and passcode. If a verifier was provided, then that implies a salt and
    // passcode were also provided. In that case, we will validate the provided verifier by comparing it against
    // the generated verifier; they should match. If a verifier was not provided, then we will simply move forward
    // with the generated verifier.
    Crypto::Spake2pVerifier spake2pVerifier;
    ByteSpan saltSpan(saltVector.data(), saltVector.size());

    // Should have already been initialized in a previous step
    uint32_t spake2pIterationCountValue = b_device_service_property_provider_get_property_as_uint32(
        mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, UINT32_MAX);

    if (spake2pIterationCountValue == UINT32_MAX)
    {
        icError("Failed to read spake2p iteration count property");
        return CHIP_ERROR_INTERNAL;
    }

    CHIP_ERROR err = spake2pVerifier.Generate(spake2pIterationCountValue, saltSpan, setupPasscode);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to generate Spake2p verifier from salt and passcode: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    std::vector<uint8_t> serializedVerifier(Crypto::kSpake2p_VerifierSerialized_Length);
    MutableByteSpan serializedVerifierSpan(serializedVerifier.data(), serializedVerifier.size());
    err = spake2pVerifier.Serialize(serializedVerifierSpan);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to serialize the generated Spake2p verifier: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    // The verifier property requires a base64-encoded string value
    size_t base64EncodedVerifierLen = BASE64_ENCODED_LEN(serializedVerifierSpan.size());
    scoped_generic char *base64EncodedVerifier = (char *) malloc(base64EncodedVerifierLen + 1);
    Base64Encode(serializedVerifierSpan.data(), serializedVerifierSpan.size(), base64EncodedVerifier);
    base64EncodedVerifier[base64EncodedVerifierLen] = '\0';

    if (providedBase64Verifier != nullptr)
    {
        if (stringCompare(providedBase64Verifier, base64EncodedVerifier, false) != 0)
        {
            icError("Provided verifier does not match the generated verifier; validate inputs");
            return CHIP_ERROR_INVALID_ARGUMENT;
        }
    }

    if (!b_device_service_property_provider_set_property_string(
            mPropertyProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, base64EncodedVerifier))
    {
        icError("Failed to set the spake2p verifier property");
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}
