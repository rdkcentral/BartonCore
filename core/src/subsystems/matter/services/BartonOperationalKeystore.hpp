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
// Created by Kevin Funderburg on 7/24/2025.
//

#pragma once

#include <../PersistentStorageDelegate.h>
#include <crypto/OperationalKeystore.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>

namespace barton
{
    /**
     * @brief Abstract interface for operational keystore implementations in Barton.
     *
     * This class defines the required interface for operational keystores used by the Barton system.
     * It extends chip::Crypto::OperationalKeystore and adds an optional initialization method for
     * persistent storage-backed implementations.
     *
     * The primary purpose of this class is to provide a flexible extension point: clients can supply
     * their own keystore implementation by inheriting from BartonOperationalKeystore. If a client does
     * not provide a custom implementation, the system will fall back to a default adapter that wraps
     * PersistentStorageOperationalKeystore.
     *
     * All methods are pure virtual, making this a strict interface that enforces implementation of
     * all required keystore operations in derived classes.
     */
    class BartonOperationalKeystore : public chip::Crypto::OperationalKeystore
    {
    public:
        BartonOperationalKeystore() = default;
        virtual ~BartonOperationalKeystore() = default;

        // Non-copyable
        BartonOperationalKeystore(BartonOperationalKeystore const &) = delete;
        void operator=(BartonOperationalKeystore const &) = delete;

        /**
         * @brief Optionally initialize the keystore with a PersistentStorageDelegate.
         *
         * Implement this method if your BartonOperationalKeystore requires access to a PersistentStorageDelegate
         * (e.g., for persistent storage-backed implementations). If your implementation does not require it,
         * you may provide an empty implementation.
         *
         * @param storageDelegate Pointer to the persistent storage delegate, or nullptr if not needed.
         * @return CHIP_NO_ERROR on success, or an appropriate CHIP_ERROR code.
         */
        virtual CHIP_ERROR Init(PersistentStorageDelegate *storageDelegate) = 0;

        // Required interface methods from OperationalKeystore
        bool HasPendingOpKeypair() const override = 0;
        bool HasOpKeypairForFabric(chip::FabricIndex fabricIndex) const override = 0;
        CHIP_ERROR NewOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                         chip::MutableByteSpan &outCertificateSigningRequest) override = 0;
        CHIP_ERROR ActivateOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                              const chip::Crypto::P256PublicKey &nocPublicKey) override = 0;
        CHIP_ERROR CommitOpKeypairForFabric(chip::FabricIndex fabricIndex) override = 0;
        CHIP_ERROR RemoveOpKeypairForFabric(chip::FabricIndex fabricIndex) override = 0;
        void RevertPendingKeypair() override = 0;
        CHIP_ERROR SignWithOpKeypair(chip::FabricIndex fabricIndex,
                                     const chip::ByteSpan &message,
                                     chip::Crypto::P256ECDSASignature &outSignature) const override = 0;
        chip::Crypto::P256Keypair *AllocateEphemeralKeypairForCASE() override = 0;
        void ReleaseEphemeralKeypair(chip::Crypto::P256Keypair *keypair) override = 0;
        CHIP_ERROR ExportOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                            chip::Crypto::P256SerializedKeypair &outKeypair) override = 0;
        CHIP_ERROR MigrateOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                             chip::Crypto::OperationalKeystore &operationalKeystore) const override = 0;
    };
} // namespace barton
