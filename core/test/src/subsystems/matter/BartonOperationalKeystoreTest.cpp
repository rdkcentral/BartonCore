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
// Created by Kevin Funderburg on 8/5/2025.
//

#include "../PersistentStorageDelegate.h"
#include "BartonMatterServiceRegistry.hpp"
#include "DefaultOperationalKeystore.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace barton
{
    class MockPersistentStorageDelegate : public barton::PersistentStorageDelegate
    {
    public:
        MOCK_METHOD(CHIP_ERROR, SyncGetKeyValue, (const char *key, void *value, uint16_t &size), (override));
        MOCK_METHOD(CHIP_ERROR, SyncSetKeyValue, (const char *key, const void *value, uint16_t size), (override));
        MOCK_METHOD(CHIP_ERROR, SyncDeleteKeyValue, (const char *key), (override));
    };

    class MockBartonOperationalKeystore : public barton::BartonOperationalKeystore
    {
    public:
        MOCK_METHOD(CHIP_ERROR, Init, (barton::PersistentStorageDelegate * storageDelegate), (override));
        MOCK_METHOD(bool, HasPendingOpKeypair, (), (const, override));
        MOCK_METHOD(bool, HasOpKeypairForFabric, (chip::FabricIndex fabricIndex), (const, override));
        MOCK_METHOD(CHIP_ERROR,
                    NewOpKeypairForFabric,
                    (chip::FabricIndex fabricIndex, chip::MutableByteSpan &outCsr),
                    (override));
        MOCK_METHOD(CHIP_ERROR,
                    ActivateOpKeypairForFabric,
                    (chip::FabricIndex fabricIndex, const chip::Crypto::P256PublicKey &nocPublicKey),
                    (override));
        MOCK_METHOD(CHIP_ERROR, CommitOpKeypairForFabric, (chip::FabricIndex fabricIndex), (override));
        MOCK_METHOD(CHIP_ERROR, RemoveOpKeypairForFabric, (chip::FabricIndex fabricIndex), (override));
        MOCK_METHOD(void, RevertPendingKeypair, (), (override));
        MOCK_METHOD(CHIP_ERROR,
                    SignWithOpKeypair,
                    (chip::FabricIndex fabricIndex,
                     const chip::ByteSpan &message,
                     chip::Crypto::P256ECDSASignature &outSignature),
                    (const, override));
        MOCK_METHOD(chip::Crypto::P256Keypair *, AllocateEphemeralKeypairForCASE, (), (override));
        MOCK_METHOD(void, ReleaseEphemeralKeypair, (chip::Crypto::P256Keypair * keypair), (override));
        MOCK_METHOD(CHIP_ERROR,
                    ExportOpKeypairForFabric,
                    (chip::FabricIndex fabricIndex, chip::Crypto::P256SerializedKeypair &outKeypair),
                    (override));
        MOCK_METHOD(CHIP_ERROR,
                    MigrateOpKeypairForFabric,
                    (chip::FabricIndex fabricIndex, chip::Crypto::OperationalKeystore &operationalKeystore),
                    (const, override));
    };

    class BartonOperationalKeystoreTest : public ::testing::Test
    {
    public:
        DefaultOperationalKeystore *operationalKeystore;

    protected:
        void SetUp() override
        {
            // Necessary to facilitate some underlying implementation operations
            chip::Platform::MemoryInit();
            operationalKeystore = new DefaultOperationalKeystore();
        }

        void TearDown() override
        {
            delete operationalKeystore;
            operationalKeystore = nullptr;
            chip::Platform::MemoryShutdown();
        }

        MockPersistentStorageDelegate mockStorage;
    };

    // Test that DefaultOperationalKeystore correctly forwards method calls to the underlying implementation.
    // This test doesn't validate the correctness of the underlying PersistentStorageOperationalKeystore,
    // but rather ensures our adapter pattern is functioning as expected by verifying that:
    // 1. Method calls on the adapter are properly forwarded to the underlying implementation
    // 2. Return values from the implementation are correctly passed back through the adapter
    // 3. The adapter doesn't modify or interfere with the implementation's behavior
    TEST_F(BartonOperationalKeystoreTest, DefaultKeystoreMethodForwarding)
    {
        // Set up test values
        chip::FabricIndex testFabricIndex = 1;
        uint8_t csrData[128] = {0};
        chip::MutableByteSpan csrSpan(csrData);
        chip::Crypto::P256PublicKey nocPublicKey;
        chip::Crypto::P256SerializedKeypair serializedKeypair;
        chip::ByteSpan message(csrData, 32); // Reuse buffer for message data
        chip::Crypto::P256ECDSASignature signature;

        // Set up default mock behaviors
        ON_CALL(mockStorage, SyncGetKeyValue(testing::_, testing::_, testing::_))
            .WillByDefault(testing::Return(CHIP_NO_ERROR));
        ON_CALL(mockStorage, SyncDeleteKeyValue(testing::_)).WillByDefault(testing::Return(CHIP_NO_ERROR));
        ON_CALL(mockStorage, SyncSetKeyValue(testing::_, testing::_, testing::_))
            .WillByDefault(testing::Return(CHIP_NO_ERROR));

        // Test Init
        CHIP_ERROR result = operationalKeystore->Init(&mockStorage);
        EXPECT_EQ(result, CHIP_NO_ERROR);

        // Test HasPendingOpKeypair
        // No explicit expectations needed, internal state check
        EXPECT_FALSE(operationalKeystore->HasPendingOpKeypair());

        // Test HasOpKeypairForFabric
        EXPECT_CALL(mockStorage, SyncGetKeyValue(testing::_, testing::_, testing::_))
            .WillOnce(testing::Return(CHIP_NO_ERROR));
        EXPECT_TRUE(operationalKeystore->HasOpKeypairForFabric(testFabricIndex));

        // Test NewOpKeypairForFabric
        // Complex method, expect it to fail due to keypair initialization issues
        result = operationalKeystore->NewOpKeypairForFabric(testFabricIndex, csrSpan);
        EXPECT_NE(result, CHIP_NO_ERROR);

        // Test ActivateOpKeypairForFabric
        // Should fail since we don't have a valid keypair
        result = operationalKeystore->ActivateOpKeypairForFabric(testFabricIndex, nocPublicKey);
        EXPECT_NE(result, CHIP_NO_ERROR);

        // Test CommitOpKeypairForFabric
        // Should fail since we don't have an active keypair
        result = operationalKeystore->CommitOpKeypairForFabric(testFabricIndex);
        EXPECT_NE(result, CHIP_NO_ERROR);

        // Test RemoveOpKeypairForFabric
        EXPECT_CALL(mockStorage, SyncDeleteKeyValue(testing::_)).WillOnce(testing::Return(CHIP_NO_ERROR));
        result = operationalKeystore->RemoveOpKeypairForFabric(testFabricIndex);
        EXPECT_EQ(result, CHIP_NO_ERROR);

        // Test RevertPendingKeypair
        // Void method, just make sure it doesn't crash
        operationalKeystore->RevertPendingKeypair();

        // Test SignWithOpKeypair
        EXPECT_CALL(mockStorage, SyncGetKeyValue(testing::_, testing::_, testing::_))
            .WillOnce(testing::Return(CHIP_NO_ERROR));
        result = operationalKeystore->SignWithOpKeypair(testFabricIndex, message, signature);
        EXPECT_NE(result, CHIP_NO_ERROR);

        // Test AllocateEphemeralKeypairForCASE
        auto keypair = operationalKeystore->AllocateEphemeralKeypairForCASE();
        EXPECT_NE(keypair, nullptr);

        // Test ReleaseEphemeralKeypair
        // Void method, just make sure it doesn't crash
        operationalKeystore->ReleaseEphemeralKeypair(keypair);

        // Test ExportOpKeypairForFabric
        EXPECT_CALL(mockStorage, SyncGetKeyValue(testing::_, testing::_, testing::_))
            .WillOnce(testing::Return(CHIP_ERROR_KEY_NOT_FOUND));
        result = operationalKeystore->ExportOpKeypairForFabric(testFabricIndex, serializedKeypair);
        EXPECT_NE(result, CHIP_NO_ERROR);

        // Test MigrateOpKeypairForFabric
        EXPECT_CALL(mockStorage, SyncGetKeyValue(testing::_, testing::_, testing::_))
            .WillOnce(testing::Return(CHIP_ERROR_KEY_NOT_FOUND));
        chip::PersistentStorageOperationalKeystore otherKeystore;
        result = operationalKeystore->MigrateOpKeypairForFabric(testFabricIndex, otherKeystore);
        EXPECT_NE(result, CHIP_NO_ERROR);
    }

    // Test that the service registry creates a DefaultOperationalKeystore when none is provided
    TEST_F(BartonOperationalKeystoreTest, ServiceRegistryUsesDefaultImplementation)
    {
        // Get the keystore from the service registry
        auto keystore = barton::BartonMatterServiceRegistry::Instance().GetBartonOperationalKeystore();

        // Verify it's a DefaultOperationalKeystore
        EXPECT_NE(dynamic_cast<barton::DefaultOperationalKeystore *>(keystore.get()), nullptr);
    }

    // Test that a custom implementation can be registered and used
    TEST_F(BartonOperationalKeystoreTest, RegisterAndUseCustomImplementation)
    {
        auto customKeystore = std::make_shared<MockBartonOperationalKeystore>();

        // Register the custom implementation with the registry
        bool registered =
            barton::BartonMatterServiceRegistry::Instance().RegisterBartonOperationalKeystore(customKeystore);
        EXPECT_TRUE(registered);

        // Get it back from the registry and use it
        auto keystore = barton::BartonMatterServiceRegistry::Instance().GetBartonOperationalKeystore();
        EXPECT_EQ(keystore.get(), customKeystore.get()); // Should be exactly our implementation

        // Verify the expectations by using the keystore
        chip::FabricIndex testFabricIndex = 1;
        EXPECT_CALL(*customKeystore, HasPendingOpKeypair()).WillOnce(testing::Return(true));
        EXPECT_CALL(*customKeystore, HasOpKeypairForFabric(testing::_)).WillOnce(testing::Return(true));
        EXPECT_TRUE(keystore->HasPendingOpKeypair());
        EXPECT_TRUE(keystore->HasOpKeypairForFabric(testFabricIndex));
    }
} // namespace barton
