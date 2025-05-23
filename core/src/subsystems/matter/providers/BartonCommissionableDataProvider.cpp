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

//
// Created by Raiyan Chowdhury on 4/21/2025.
//

#include "BartonCommissionableDataProvider.hpp"
#include "crypto/CHIPCryptoPAL.h"
#include "lib/core/CHIPError.h"
#include "lib/support/Span.h"
#include "setup_payload/SetupPayload.h"
#include <vector>

extern "C" {
#include <icLog/logging.h>
}

#define LOG_TAG     "CommishDataProvider"
#define logFmt(fmt) "(%s): " fmt, __func__

using namespace ::barton;
using namespace chip;

bool BartonCommissionableDataProvider::ValidateDiscriminator(uint16_t discriminator)
{
    if (discriminator > chip::kMaxDiscriminatorValue)
    {
        icError("Discriminator value invalid: %u", discriminator);
        return false;
    }

    return true;
}

bool BartonCommissionableDataProvider::ValidateSpake2pIterationCount(uint32_t iterationCount)
{
    if ((iterationCount < Crypto::kSpake2p_Min_PBKDF_Iterations) ||
        (iterationCount > Crypto::kSpake2p_Max_PBKDF_Iterations))
    {
        icError("Spake2p iteration count invalid: %u", iterationCount);
        return false;
    }

    return true;
}

bool BartonCommissionableDataProvider::ValidateSalt(MutableByteSpan &saltBuf)
{
    if (saltBuf.size() < Crypto::kSpake2p_Min_PBKDF_Salt_Length ||
        saltBuf.size() > Crypto::kSpake2p_Max_PBKDF_Salt_Length)
    {
        icError("Size of provided salt is invalid: %zu", saltBuf.size());
        return false;
    }

    return true;
}

bool BartonCommissionableDataProvider::ValidateVerifier(MutableByteSpan &verifierBuf,
                                                        size_t verifierLen,
                                                        MutableByteSpan &saltBuf,
                                                        const uint32_t passcode,
                                                        const uint32_t iterationCount)
{
    if (verifierBuf.size() != verifierLen)
    {
        icError("The verifier buffer size does not match the returned verifier length: %zu != %zu",
                verifierBuf.size(),
                verifierLen);
        return false;
    }

    if (verifierLen != Crypto::kSpake2p_VerifierSerialized_Length)
    {
        icError("Size of provided verifier is invalid: %zu", verifierLen);
        return false;
    }

    Crypto::Spake2pVerifier providedVerifier;
    CHIP_ERROR err = providedVerifier.Deserialize(verifierBuf);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to deserialize the provided verifier: %" CHIP_ERROR_FORMAT, err.Format());
        return false;
    }

    // Now the verifier needs to be validated against the salt, iteration count, and passcode. If the
    // provided verifier does not match the verifier generated from those values, then it's invalid.
    Crypto::Spake2pVerifier generatedVerifier;
    err = generatedVerifier.Generate(iterationCount, saltBuf, passcode);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to generate verifier from salt, iteration count, and passcode: %" CHIP_ERROR_FORMAT,
                err.Format());
        return false;
    }

    std::vector<uint8_t> generatedVerifierSerialized(Crypto::kSpake2p_VerifierSerialized_Length);
    MutableByteSpan generatedVerifierSpan(generatedVerifierSerialized.data(), generatedVerifierSerialized.size());
    err = generatedVerifier.Serialize(generatedVerifierSpan);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to serialize the generated Spake2p verifier: %" CHIP_ERROR_FORMAT, err.Format());
        return false;
    }

    if (!generatedVerifierSpan.data_equal(verifierBuf))
    {
        icError("Provided verifier does not match the generated verifier; validate inputs");
        return false;
    }

    return true;
}

CHIP_ERROR BartonCommissionableDataProvider::GetSetupDiscriminator(uint16_t &setupDiscriminator)
{
    uint16_t discriminator;
    ReturnErrorOnFailure(RetrieveSetupDiscriminator(discriminator));

    VerifyOrReturnError(ValidateDiscriminator(discriminator),
                        CHIP_ERROR_INTERNAL,
                        icError("Discriminator value invalid: %u", discriminator));

    setupDiscriminator = discriminator;

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonCommissionableDataProvider::GetSpake2pIterationCount(uint32_t &iterationCount)
{
    uint32_t count;
    ReturnErrorOnFailure(RetrieveSpake2pIterationCount(count));

    VerifyOrReturnError(ValidateSpake2pIterationCount(count), CHIP_ERROR_INTERNAL);

    iterationCount = count;

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonCommissionableDataProvider::GetSpake2pSalt(MutableByteSpan &saltBuf)
{
    VerifyOrReturnError(saltBuf.size() >= Crypto::kSpake2p_Max_PBKDF_Salt_Length, CHIP_ERROR_BUFFER_TOO_SMALL);

    std::vector<uint8_t> tempSaltVector(Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    MutableByteSpan tempSaltSpan(tempSaltVector.data(), tempSaltVector.size());
    size_t saltLen;

    ReturnErrorOnFailure(RetrieveSpake2pSalt(tempSaltSpan, saltLen));
    tempSaltSpan.reduce_size(saltLen);

    VerifyOrReturnError(ValidateSalt(tempSaltSpan), CHIP_ERROR_INTERNAL);

    memcpy(saltBuf.data(), tempSaltSpan.data(), saltLen);
    saltBuf.reduce_size(saltLen);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonCommissionableDataProvider::GetSpake2pVerifier(MutableByteSpan &verifierBuf, size_t &outVerifierLen)
{
    VerifyOrReturnError(verifierBuf.size() >= Crypto::kSpake2p_VerifierSerialized_Length, CHIP_ERROR_BUFFER_TOO_SMALL);

    std::vector<uint8_t> tempVerifierVector(Crypto::kSpake2p_VerifierSerialized_Length);
    MutableByteSpan tempVerifierSpan(tempVerifierVector.data(), tempVerifierVector.size());
    size_t verifierLen;

    ReturnErrorOnFailure(RetrieveSpake2pVerifier(tempVerifierSpan, verifierLen));
    tempVerifierSpan.reduce_size(verifierLen);

    // Need to get salt, passcode, and iteration count to validate the verifier
    std::vector<uint8_t> saltVector(Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    MutableByteSpan saltBuf(saltVector.data(), saltVector.size());
    ReturnErrorOnFailure(GetSpake2pSalt(saltBuf));

    uint32_t passcode;
    ReturnErrorOnFailure(GetSetupPasscode(passcode));

    uint32_t iterationCount;
    ReturnErrorOnFailure(GetSpake2pIterationCount(iterationCount));

    VerifyOrReturnError(ValidateVerifier(tempVerifierSpan, verifierLen, saltBuf, passcode, iterationCount),
                        CHIP_ERROR_INTERNAL);

    memcpy(verifierBuf.data(), tempVerifierSpan.data(), verifierLen);
    verifierBuf.reduce_size(verifierLen);
    outVerifierLen = verifierLen;

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonCommissionableDataProvider::GetSetupPasscode(uint32_t &setupPasscode)
{
    uint32_t passcode;
    ReturnErrorOnFailure(RetrieveSetupPasscode(passcode));

    VerifyOrReturnError(SetupPayload::IsValidSetupPIN(passcode), CHIP_ERROR_INTERNAL);

    setupPasscode = passcode;

    return CHIP_NO_ERROR;
}
