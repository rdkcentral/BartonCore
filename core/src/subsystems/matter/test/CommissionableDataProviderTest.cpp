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
// Created by Raiyan Chowdhury on 3/27/2025.
//

#include <cstdint>
#include <gtest/gtest.h>
#include "CustomCommissionableDataProvider.hpp"
#include "crypto/CHIPCryptoPAL.h"
#include "lib/core/Optional.h"
#include "lib/support/Span.h"
#include "lib/support/Base64.h"

extern "C" {
#include "device-service-mock-property-provider.h"
#include "device-service-properties.h"
#include "provider/device-service-property-provider.h"
#include "icTypes/icStringHashMap.h"

bool __wrap_deviceServiceGetSystemProperty(const char *name, char **value)
{
    return false;
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    return false;
}

bool __wrap_deviceServiceGetAllSystemProperties(icStringHashMap *map)
{
    return false;
}

}

#define TEST_PASSCODE 20252026
#define TEST_SALT "TXkgVGVzdCBLZXkgU2FsdA=="
#define TEST_VERIFIER \
    "oR8ZbQYBMQs3iVt32T2l+6N9au2UimU8CmiGLZSL0LkEvaTYL9rVtjuA7oTNH2NpKDlL+4581tSzhe1+" \
    "Q4IpNlZcdCPINUo9UpdoDEeAYphTuP6nDXPqAK4Doz1H9gua+w=="
#define TEST_DISCRIMINATOR 3840
#define TEST_DISCRIMINATOR2 3850
#define TEST_ITERATION_COUNT chip::Crypto::kSpake2p_Min_PBKDF_Iterations
#define TEST_ITERATION_COUNT2 chip::Crypto::kSpake2p_Max_PBKDF_Iterations

#define TEST_PASSCODE_INVALID 12345678
#define TEST_PASSCODE_INVALID_2 22222222
#define TEST_PASSCODE_INVALID_3 0
#define TEST_DISCRIMINATOR_INVALID 4096
#define TEST_ITERATION_COUNT_TOO_SMALL chip::Crypto::kSpake2p_Min_PBKDF_Iterations - 1
#define TEST_ITERATION_COUNT_TOO_LARGE chip::Crypto::kSpake2p_Max_PBKDF_Iterations + 1
#define TEST_SALT_TOO_SHORT "TXkgVGVzdCBLZXkgU2Fs"
#define TEST_SALT_TOO_LONG "TXkgVGVzdCBLZXkgU2FsdCBCdXQgSXQncyB0b28gTG9uZw=="
#define TEST_VERIFIER_TOO_SHORT \
    "oR8ZbQYBMQs3iVt32T2l+6N9au2UimU8CmiGLZSL0LkEvaTYL9rVtjuA7oTNH2NpKDlL+4581tSzhe1+" \
    "Q4IpNlZcdCPINUo9UpdoDEeAYphTuP6nDXPqAK4Doz1H9gua"
#define TEST_VERIFIER_TOO_LONG \
    "oR8ZbQYBMQs3iVt32T2l+6N9au2UimU8CmiGLZSL0LkEvaTYL9rVtjuA7oTNH2NpKDlL+4581tSzhe1+" \
    "Q4IpNlZcdCPINUo9UpdoDEeAYphTuP6nDXPqAK4Doz1H9gua+/4="

namespace barton {

class CustomCommissionableDataProviderTest : public ::testing::Test
{
public:
    BDeviceServicePropertyProvider *mockProvider;
    CustomCommissionableDataProvider *commissionableDataProvider;

    void SetUp() override
    {
        mockProvider = (BDeviceServicePropertyProvider *) b_device_service_mock_property_provider_new();
        commissionableDataProvider = new CustomCommissionableDataProvider(mockProvider);
    }

    void TearDown() override
    {
        delete commissionableDataProvider;
        g_clear_object(&mockProvider);
    }

    // This can be called to test multiple successful Inits in a row
    void ResetCommissionableDataProviderAndProperties()
    {
        if (commissionableDataProvider != nullptr)
        {
            delete commissionableDataProvider;
        }

        if (mockProvider != nullptr)
        {
            g_clear_object(&mockProvider);
        }

        mockProvider = (BDeviceServicePropertyProvider *) b_device_service_mock_property_provider_new();
        commissionableDataProvider = new CustomCommissionableDataProvider(mockProvider);
    }

    // Caller must free the returned buffer
    char *Base64EncodeBuffer(const uint8_t *buffer, size_t bufferLen)
    {
        size_t base64EncodedLen = BASE64_ENCODED_LEN(bufferLen);
        char *base64EncodedBuffer = (char *) malloc(base64EncodedLen + 1);
        chip::Base64Encode(buffer, bufferLen, base64EncodedBuffer);
        base64EncodedBuffer[base64EncodedLen] = '\0';
        return base64EncodedBuffer;
    }
};

using namespace ::testing;

/**
 * @brief Tests the initialization of our commissionable data provider when the client externally provides a valid
 *        passcode, spake2p salt, and spake2p verifier.
 */
TEST_F(CustomCommissionableDataProviderTest, ProvideValidPasscodeSaltVerifierTest)
{
    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR),
                                         TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);
}

/**
 * @brief Tests the initialization of our commissionable data provider when the client provides various invalid
 *        parameters.
 */
TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidParamsTest)
{
    // Try an invalid discriminator value
    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR_INVALID),
                                         TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Try several invalid spake2p iteration count values (invalid sizes)
    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR),
                                         TEST_ITERATION_COUNT_TOO_SMALL),
        CHIP_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR),
                                         TEST_ITERATION_COUNT_TOO_LARGE),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Try several invalid passcode values
    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE_INVALID);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE_INVALID_2);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE_INVALID_3);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Try several invalid spake2p salt values (invalid sizes)
    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT_TOO_SHORT);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT_TOO_LONG);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Try several invalid spake2p verifier values (invalid sizes)
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER_TOO_SHORT);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER_TOO_LONG);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Now make everything valid
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR),
                                         TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);
}

/**
 * @brief Tests the initialization of our commissionable data provider depending on how the discriminator is provided.
 *
 * When the Optional passed to the Init function is empty, the discriminator is read from the property provider. If
 * there is no discriminator in the property provider, then Init() should fail. If the Optional is not empty, then the
 * discriminator is set to the value in the Optional and the property provider is updated with that value.
 */
TEST_F(CustomCommissionableDataProviderTest, SetDiscriminatorTest)
{
    // Test the case where the Optional and property provider are both empty
    EXPECT_EQ(
        commissionableDataProvider->Init(chip::NullOptional, TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Test the case where the Optional has a value and the property provider is empty
    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Verify that the property is set to the value in the Optional
    // Also, each time we verify a value in the property provider, we should also verify that the commissionable data
    // provider's public API returns the same value
    EXPECT_EQ(b_device_service_property_provider_get_property_as_uint16(
                  mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, UINT16_MAX),
              TEST_DISCRIMINATOR);

    uint16_t setupDiscriminator;
    EXPECT_EQ(commissionableDataProvider->GetSetupDiscriminator(setupDiscriminator), CHIP_NO_ERROR);
    EXPECT_EQ(setupDiscriminator, TEST_DISCRIMINATOR);

    // Test the case where the Optional is empty and the property provider has a value
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR2);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::NullOptional, TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Verify that the property provider still has the value we set before after running Init()
    EXPECT_EQ(b_device_service_property_provider_get_property_as_uint16(
                  mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, UINT16_MAX),
              TEST_DISCRIMINATOR2);

    EXPECT_EQ(commissionableDataProvider->GetSetupDiscriminator(setupDiscriminator), CHIP_NO_ERROR);
    EXPECT_EQ(setupDiscriminator, TEST_DISCRIMINATOR2);

    // Test the case where the Optional has a value and the property provider has a value
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR2);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Verify that the property provider has the value set in the Optional, rather than the value previously set in the
    // property provider
    EXPECT_EQ(b_device_service_property_provider_get_property_as_uint16(
                  mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, UINT16_MAX),
              TEST_DISCRIMINATOR);

    EXPECT_EQ(commissionableDataProvider->GetSetupDiscriminator(setupDiscriminator), CHIP_NO_ERROR);
    EXPECT_EQ(setupDiscriminator, TEST_DISCRIMINATOR);
}

/**
 * @brief Tests the initialization of our commissionable data provider depending on how the spake2p iteration count is
 *        provided.
 *
 * The Init() method takes a default value to fall back on when the property is not set. If the property is set, then
 * the value in the property provider should be used.
 */
TEST_F(CustomCommissionableDataProviderTest, SetSpake2pIterationCountTest)
{
    // Use default value
    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR),
                                         TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Verify that the property provider has the default value
    EXPECT_EQ(b_device_service_property_provider_get_property_as_uint32(
                  mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, UINT32_MAX),
              TEST_ITERATION_COUNT);

    uint32_t spake2pIterationCount;
    EXPECT_EQ(commissionableDataProvider->GetSpake2pIterationCount(spake2pIterationCount), CHIP_NO_ERROR);
    EXPECT_EQ(spake2pIterationCount, TEST_ITERATION_COUNT);

    // Set the property to a different value from the default value provided to Init()
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT2);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR),
                                         TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Verify that the property provider has the value we set before
    EXPECT_EQ(b_device_service_property_provider_get_property_as_uint32(
                  mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, UINT32_MAX),
              TEST_ITERATION_COUNT2);

    EXPECT_EQ(commissionableDataProvider->GetSpake2pIterationCount(spake2pIterationCount), CHIP_NO_ERROR);
    EXPECT_EQ(spake2pIterationCount, TEST_ITERATION_COUNT2);
}

/**
 * @brief Tests the initialization of our commissionable data provider depending on how the passcode, spake2p salt, and
 *        spake2p verifier are provided.
 *
 * If the client provides a verifier, then the client MUST also provide a salt AND passcode. If the client does not
 * provide a verifier, then the client may or may not provide a salt or passcode. If both are provided, then they will
 * be used to generate a verifier. If one or both are not provided, then a random salt and/or passcode will be
 * generated as needed, and the verifier will be generated from those values.
 */
TEST_F(CustomCommissionableDataProviderTest, SetSaltPasscodeAndVerifierTest)
{
    // Provide nothing, and let the data provider generate everything
    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Provide a salt and passcode, and let the data provider generate the verifier
    // Init() should succeed, and the generated verifier should match the value of TEST_VERIFIER
    // Also verify that the salt and passcode are what we expect (via both the property provider and data provider
    // API), just in case
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);
    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    EXPECT_STREQ(b_device_service_property_provider_get_property_as_string(
                     mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, nullptr),
                 TEST_SALT);
    EXPECT_EQ(b_device_service_property_provider_get_property_as_uint32(
                  mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, UINT32_MAX),
              TEST_PASSCODE);
    EXPECT_STREQ(b_device_service_property_provider_get_property_as_string(
                     mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, nullptr),
                 TEST_VERIFIER);

    uint32_t setupPasscode;
    EXPECT_EQ(commissionableDataProvider->GetSetupPasscode(setupPasscode), CHIP_NO_ERROR);
    EXPECT_EQ(setupPasscode, TEST_PASSCODE);

    std::vector<uint8_t> spake2pSaltVector(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    chip::MutableByteSpan spake2pSaltSpan(spake2pSaltVector.data(), spake2pSaltVector.size());
    EXPECT_EQ(commissionableDataProvider->GetSpake2pSalt(spake2pSaltSpan), CHIP_NO_ERROR);
    scoped_generic char *base64EncodedSalt = Base64EncodeBuffer(spake2pSaltSpan.data(), spake2pSaltSpan.size());
    EXPECT_STREQ(base64EncodedSalt, TEST_SALT);

    std::vector<uint8_t> spake2pVerifierVector(chip::Crypto::kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan spake2pVerifierSpan(spake2pVerifierVector.data(), spake2pVerifierVector.size());
    size_t outVerifierLen;
    EXPECT_EQ(commissionableDataProvider->GetSpake2pVerifier(spake2pVerifierSpan, outVerifierLen), CHIP_NO_ERROR);
    scoped_generic char *base64EncodedVerifier =
        Base64EncodeBuffer(spake2pVerifierSpan.data(), spake2pVerifierSpan.size());
    EXPECT_STREQ(base64EncodedVerifier, TEST_VERIFIER);
    EXPECT_EQ(outVerifierLen, chip::Crypto::kSpake2p_VerifierSerialized_Length);

    // Provide a salt, but no passcode or verifier, and let the data provider generate the passcode and verifier
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Provide a passcode, but no salt or verifier, and let the data provider generate the salt and verifier
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_NO_ERROR);

    // Provide a verifier, but no salt or passcode; Init() should fail
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Provide a verifier and salt, but no passcode; Init() should fail
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);

    // Provide a verifier and passcode, but no salt; Init() should fail
    ResetCommissionableDataProviderAndProperties();
    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);
    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    EXPECT_EQ(
        commissionableDataProvider->Init(chip::MakeOptional<uint16_t>(TEST_DISCRIMINATOR), TEST_ITERATION_COUNT),
        CHIP_ERROR_INVALID_ARGUMENT);
}

} // namespace barton
