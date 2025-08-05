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
// Created by Kevin Funderburg on 7/29/2025.
//

#pragma once

#include "../PersistentStorageDelegate.h"
#include "BartonOperationalKeystore.hpp"
#include "crypto/PersistentStorageOperationalKeystore.h"
#include "lib/core/CHIPError.h"

namespace barton
{
    /**
     * @brief Adapter for PersistentStorageOperationalKeystore using composition.
     *
     * The BartonOperationalKeystore abstraction allows clients to supply their own
     * OperationalKeystore implementation. If a client does not provide one, the system
     * falls back to using PersistentStorageOperationalKeystore, which itself inherits
     * from OperationalKeystore.
     *
     * Rather than introducing a complex inheritance hierarchy to support this fallback,
     * we use the adapter pattern: DefaultOperationalKeystore composes a
     * PersistentStorageOperationalKeystore and forwards all required operations to it.
     * This approach simplifies our design, making it easy to swap between
     * different keystore implementations without creating a mess of complicated
     * inheritance relationships.
     */
    class DefaultOperationalKeystore : public BartonOperationalKeystore
    {
    private:
        chip::PersistentStorageOperationalKeystore mImplementation;

    public:
        DefaultOperationalKeystore() = default;
        ~DefaultOperationalKeystore() override = default;

        // Required methods from BartonOperationalKeystore
        CHIP_ERROR Init(PersistentStorageDelegate *storageDelegate) override;
        bool HasPendingOpKeypair() const override;
        bool HasOpKeypairForFabric(chip::FabricIndex fabricIndex) const override;
        CHIP_ERROR NewOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                         chip::MutableByteSpan &outCertificateSigningRequest) override;
        CHIP_ERROR ActivateOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                              const chip::Crypto::P256PublicKey &nocPublicKey) override;
        CHIP_ERROR CommitOpKeypairForFabric(chip::FabricIndex fabricIndex) override;
        CHIP_ERROR RemoveOpKeypairForFabric(chip::FabricIndex fabricIndex) override;
        void RevertPendingKeypair() override;
        CHIP_ERROR SignWithOpKeypair(chip::FabricIndex fabricIndex,
                                     const chip::ByteSpan &message,
                                     chip::Crypto::P256ECDSASignature &outSignature) const override;
        chip::Crypto::P256Keypair *AllocateEphemeralKeypairForCASE() override;
        void ReleaseEphemeralKeypair(chip::Crypto::P256Keypair *keypair) override;
        CHIP_ERROR ExportOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                            chip::Crypto::P256SerializedKeypair &outKeypair) override;
        CHIP_ERROR MigrateOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                             chip::Crypto::OperationalKeystore &operationalKeystore) const override;
    };
} // namespace barton
