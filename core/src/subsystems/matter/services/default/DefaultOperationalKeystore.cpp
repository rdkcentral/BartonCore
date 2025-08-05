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
// Created by Kevin Funderburg on 7/30/2025.
//

#include "DefaultOperationalKeystore.hpp"

namespace barton
{
    CHIP_ERROR DefaultOperationalKeystore::Init(PersistentStorageDelegate *storageDelegate)
    {
        return mImplementation.Init(storageDelegate);
    }

    bool DefaultOperationalKeystore::HasPendingOpKeypair() const
    {
        return mImplementation.HasPendingOpKeypair();
    }

    bool DefaultOperationalKeystore::HasOpKeypairForFabric(const chip::FabricIndex fabricIndex) const
    {
        return mImplementation.HasOpKeypairForFabric(fabricIndex);
    }

    CHIP_ERROR DefaultOperationalKeystore::NewOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                                                 chip::MutableByteSpan &outCertificateSigningRequest)
    {
        return mImplementation.NewOpKeypairForFabric(fabricIndex, outCertificateSigningRequest);
    }

    CHIP_ERROR DefaultOperationalKeystore::ActivateOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                                                      const chip::Crypto::P256PublicKey &nocPublicKey)
    {
        return mImplementation.ActivateOpKeypairForFabric(fabricIndex, nocPublicKey);
    }

    CHIP_ERROR DefaultOperationalKeystore::CommitOpKeypairForFabric(chip::FabricIndex fabricIndex)
    {
        return mImplementation.CommitOpKeypairForFabric(fabricIndex);
    }

    CHIP_ERROR DefaultOperationalKeystore::RemoveOpKeypairForFabric(chip::FabricIndex fabricIndex)
    {
        return mImplementation.RemoveOpKeypairForFabric(fabricIndex);
    }

    void DefaultOperationalKeystore::RevertPendingKeypair()
    {
        mImplementation.RevertPendingKeypair();
    }

    CHIP_ERROR DefaultOperationalKeystore::SignWithOpKeypair(chip::FabricIndex fabricIndex,
                                                             const chip::ByteSpan &message,
                                                             chip::Crypto::P256ECDSASignature &outSignature) const
    {
        return mImplementation.SignWithOpKeypair(fabricIndex, message, outSignature);
    }

    chip::Crypto::P256Keypair *DefaultOperationalKeystore::AllocateEphemeralKeypairForCASE()
    {
        return mImplementation.AllocateEphemeralKeypairForCASE();
    }

    void DefaultOperationalKeystore::ReleaseEphemeralKeypair(chip::Crypto::P256Keypair *keypair)
    {
        mImplementation.ReleaseEphemeralKeypair(keypair);
    }

    CHIP_ERROR DefaultOperationalKeystore::ExportOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                                                    chip::Crypto::P256SerializedKeypair &outKeypair)
    {
        return mImplementation.ExportOpKeypairForFabric(fabricIndex, outKeypair);
    }

    CHIP_ERROR
    DefaultOperationalKeystore::MigrateOpKeypairForFabric(chip::FabricIndex fabricIndex,
                                                          chip::Crypto::OperationalKeystore &operationalKeystore) const
    {
        return mImplementation.MigrateOpKeypairForFabric(fabricIndex, operationalKeystore);
    }

} // namespace barton
