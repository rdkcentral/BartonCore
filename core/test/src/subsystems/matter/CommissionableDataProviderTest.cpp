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

#include "DefaultCommissionableDataProvider.hpp"
#include "crypto/CHIPCryptoPAL.h"
#include "lib/support/Base64.h"
#include "lib/support/Span.h"
#include <cstdint>
#include <gtest/gtest.h>

// C includes
extern "C" {
#include "device-service-properties.h"
#include "deviceServiceConfiguration.h"
#include "provider/device-service-default-property-provider.h"
#include "provider/device-service-property-provider.h"
#include "icTypes/icStringHashMap.h"
}

static GHashTable *properties; // hash table to simulate property storage

// C function mocks
extern "C" {

static BDeviceServicePropertyProvider *mockPropertyProvider = nullptr;

BDeviceServicePropertyProvider *__wrap_deviceServiceConfigurationGetPropertyProvider()
{
    if (mockPropertyProvider == nullptr)
    {
        mockPropertyProvider = (BDeviceServicePropertyProvider *) b_device_service_default_property_provider_new();
        g_assert(G_IS_OBJECT(mockPropertyProvider));
    }

    // Need to increment ref count because the unit under test uses g_autoptr, and we need this object to persist for
    // the duration of the test
    g_object_ref(mockPropertyProvider);
    return mockPropertyProvider;
}

void resetMockPropertyProvider()
{
    if (mockPropertyProvider != nullptr)
    {
        g_clear_object(&mockPropertyProvider);
    }
}

bool __wrap_deviceServiceGetSystemProperty(const char *name, char **value)
{
    if (properties == nullptr)
    {
        return false;
    }

    char *propertyValue = (char *) g_hash_table_lookup(properties, name);

    if (propertyValue == nullptr)
    {
        return false;
    }

    *value = g_strdup(propertyValue);
    return true;
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    if (properties == nullptr)
    {
        return false;
    }

    if (value == nullptr)
    {
        g_hash_table_remove(properties, name);
    }
    else
    {
        g_hash_table_replace(properties, g_strdup(name), g_strdup(value));
    }

    return true;
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
#define TEST_SALT_MISMATCH             "TXkgVGVzdCBLZXkgU2FsdCAy"
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
    DefaultCommissionableDataProvider *commissionableDataProvider;

    void SetUp() override
    {
        commissionableDataProvider = new DefaultCommissionableDataProvider();
        properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }

    void TearDown() override
    {
        delete commissionableDataProvider;
        commissionableDataProvider = nullptr;
        g_hash_table_destroy(properties);
        properties = nullptr;
        resetMockPropertyProvider();
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
 *        discriminator, passcode, spake2p iteration count, spake2p salt, and spake2p verifier.
 */
TEST_F(CustomCommissionableDataProviderTest, ProvideValidCommissionableDataTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    // Test that the getter APIs return the expected values.
    // The getter APIs will also return an error if the values found are invalid, so in this test, they must return
    // CHIP_NO_ERROR.
    uint16_t setupDiscriminator;
    EXPECT_EQ(commissionableDataProvider->GetSetupDiscriminator(setupDiscriminator), CHIP_NO_ERROR);
    EXPECT_EQ(setupDiscriminator, TEST_DISCRIMINATOR);

    uint32_t spake2pIterationCount;
    EXPECT_EQ(commissionableDataProvider->GetSpake2pIterationCount(spake2pIterationCount), CHIP_NO_ERROR);
    EXPECT_EQ(spake2pIterationCount, TEST_ITERATION_COUNT);

    uint32_t setupPasscode;
    EXPECT_EQ(commissionableDataProvider->GetSetupPasscode(setupPasscode), CHIP_NO_ERROR);
    EXPECT_EQ(setupPasscode, TEST_PASSCODE);

    std::vector<uint8_t> saltVector(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    chip::MutableByteSpan spake2pSalt(saltVector.data(), saltVector.size());
    EXPECT_EQ(commissionableDataProvider->GetSpake2pSalt(spake2pSalt), CHIP_NO_ERROR);
    scoped_generic char* base64Salt = Base64EncodeBuffer(spake2pSalt.data(), spake2pSalt.size());
    EXPECT_STREQ(base64Salt, TEST_SALT);

    std::vector<uint8_t> verifierVector(chip::Crypto::kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan spake2pVerifier(verifierVector.data(), verifierVector.size());
    size_t verifierLen;
    EXPECT_EQ(commissionableDataProvider->GetSpake2pVerifier(spake2pVerifier, verifierLen), CHIP_NO_ERROR);
    scoped_generic char* base64Verifier = Base64EncodeBuffer(spake2pVerifier.data(), verifierLen);
    EXPECT_STREQ(base64Verifier, TEST_VERIFIER);
    EXPECT_EQ(verifierLen, chip::Crypto::kSpake2p_VerifierSerialized_Length);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidDiscriminatorTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR_INVALID);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    uint16_t setupDiscriminator;
    EXPECT_NE(commissionableDataProvider->GetSetupDiscriminator(setupDiscriminator), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidSpake2pIterationCountTooSmallTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT_TOO_SMALL);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    uint32_t spake2pIterationCount;
    EXPECT_NE(commissionableDataProvider->GetSpake2pIterationCount(spake2pIterationCount), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidSpake2pIterationCountTooLargeTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT_TOO_LARGE);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    uint32_t spake2pIterationCount;
    EXPECT_NE(commissionableDataProvider->GetSpake2pIterationCount(spake2pIterationCount), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidPasscodeTest1)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE_INVALID);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    uint32_t setupPasscode;
    EXPECT_NE(commissionableDataProvider->GetSetupPasscode(setupPasscode), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidPasscodeTest2)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE_INVALID_2);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    uint32_t setupPasscode;
    EXPECT_NE(commissionableDataProvider->GetSetupPasscode(setupPasscode), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidPasscodeTest3)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE_INVALID_3);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    uint32_t setupPasscode;
    EXPECT_NE(commissionableDataProvider->GetSetupPasscode(setupPasscode), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidSpake2pSaltTooShortTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT_TOO_SHORT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    std::vector<uint8_t> saltVector(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    chip::MutableByteSpan spake2pSalt(saltVector.data(), saltVector.size());
    EXPECT_NE(commissionableDataProvider->GetSpake2pSalt(spake2pSalt), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidSpake2pSaltTooLongTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT_TOO_LONG);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    std::vector<uint8_t> saltVector(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    chip::MutableByteSpan spake2pSalt(saltVector.data(), saltVector.size());
    EXPECT_NE(commissionableDataProvider->GetSpake2pSalt(spake2pSalt), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidSpake2pVerifierTooShortTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER_TOO_SHORT);

    std::vector<uint8_t> verifierVector(chip::Crypto::kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan spake2pVerifier(verifierVector.data(), verifierVector.size());
    size_t verifierLen;
    EXPECT_NE(commissionableDataProvider->GetSpake2pVerifier(spake2pVerifier, verifierLen), CHIP_NO_ERROR);
}

TEST_F(CustomCommissionableDataProviderTest, ProvideInvalidSpake2pVerifierTooLongTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER_TOO_LONG);

    std::vector<uint8_t> verifierVector(chip::Crypto::kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan spake2pVerifier(verifierVector.data(), verifierVector.size());
    size_t verifierLen;
    EXPECT_NE(commissionableDataProvider->GetSpake2pVerifier(spake2pVerifier, verifierLen), CHIP_NO_ERROR);
}

/**
 * @brief In this test, the client provides all valid values, but the spake2p iteration count is not the one that was
 *        used to generate the spake2p verifier.
 */
TEST_F(CustomCommissionableDataProviderTest, ProvideBadSpake2pIterationCountMatchTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT2);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    // Verify that this new iteration count is indeed valid...
    uint32_t spake2pIterationCount;
    EXPECT_EQ(commissionableDataProvider->GetSpake2pIterationCount(spake2pIterationCount), CHIP_NO_ERROR);

    // ...but the verifier which was valid in the first test is now invalid, as it cannot be generated with this new
    // iteration count
    std::vector<uint8_t> verifierVector(chip::Crypto::kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan spake2pVerifier(verifierVector.data(), verifierVector.size());
    size_t verifierLen;
    EXPECT_NE(commissionableDataProvider->GetSpake2pVerifier(spake2pVerifier, verifierLen), CHIP_NO_ERROR);
}

/**
 * @brief In this test, the client provides all valid values, but the passcode is not the one that was used to generate
 *        the spake2p verifier.
 */
TEST_F(CustomCommissionableDataProviderTest, ProvideBadPasscodeMatchTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE + 1);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    // Verify that this new passcode is indeed valid...
    uint32_t setupPasscode;
    EXPECT_EQ(commissionableDataProvider->GetSetupPasscode(setupPasscode), CHIP_NO_ERROR);

    // ...but the verifier which was valid in the first test is now invalid, as it cannot be generated with this new
    // passcode
    std::vector<uint8_t> verifierVector(chip::Crypto::kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan spake2pVerifier(verifierVector.data(), verifierVector.size());
    size_t verifierLen;
    EXPECT_NE(commissionableDataProvider->GetSpake2pVerifier(spake2pVerifier, verifierLen), CHIP_NO_ERROR);
}

/**
 * @brief In this test, the client provides all valid values, but the salt is not the one that was used to generate the
 *        spake2p verifier.
 */
TEST_F(CustomCommissionableDataProviderTest, ProvideBadSaltMatchTest)
{
    g_autoptr(BDeviceServicePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_device_service_property_provider_set_property_uint16(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_DISCRIMINATOR, TEST_DISCRIMINATOR);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, TEST_ITERATION_COUNT);

    b_device_service_property_provider_set_property_uint32(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SETUP_PASSCODE, TEST_PASSCODE);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_SALT, TEST_SALT_MISMATCH);

    b_device_service_property_provider_set_property_string(
        mockProvider, B_DEVICE_SERVICE_BARTON_MATTER_SPAKE2P_VERIFIER, TEST_VERIFIER);

    // Verify that this new salt is indeed valid...
    std::vector<uint8_t> saltVector(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length);
    chip::MutableByteSpan spake2pSalt(saltVector.data(), saltVector.size());
    EXPECT_EQ(commissionableDataProvider->GetSpake2pSalt(spake2pSalt), CHIP_NO_ERROR);

    // ...but the verifier which was valid in the first test is now invalid, as it cannot be generated with this new
    // salt
    std::vector<uint8_t> verifierVector(chip::Crypto::kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan spake2pVerifier(verifierVector.data(), verifierVector.size());
    size_t verifierLen;
    EXPECT_NE(commissionableDataProvider->GetSpake2pVerifier(spake2pVerifier, verifierLen), CHIP_NO_ERROR);
}

} // namespace barton
