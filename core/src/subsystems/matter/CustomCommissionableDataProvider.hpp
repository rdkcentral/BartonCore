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

#pragma once

#include <lib/core/CHIPError.h>
#include <lib/core/Optional.h>
#include <lib/support/Span.h>
#include <platform/CommissionableDataProvider.h>
#include <vector>

extern "C" {
#include "provider/device-service-property-provider.h"
}

namespace barton
{
class CustomCommissionableDataProvider : public chip::DeviceLayer::CommissionableDataProvider
{
public:
    CustomCommissionableDataProvider(BDeviceServicePropertyProvider *provider) :
        mPropertyProvider(g_object_ref(provider))
    {
    }

    ~CustomCommissionableDataProvider()
    {
        g_clear_object(&mPropertyProvider);
    }

    /**
     * Initialize the commissionable data provider.
     *
     * @param discriminator The setup discriminator to use. If not provided, the value will be read from the
     * property provider. If the property is not set, an error will be returned.
     * @param defaultSpake2pIterationCount The default number of iterations to use for the spake2p protocol, if the
     * client has not externally provided a value via the property provider.
     *
     * @return CHIP_ERROR on success, or an error code on failure.
     */
    CHIP_ERROR Init(chip::Optional<uint16_t> discriminator, uint32_t defaultSpake2pIterationCount);

    CHIP_ERROR GetSetupDiscriminator(uint16_t &setupDiscriminator) override;
    CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override;
    CHIP_ERROR GetSpake2pIterationCount(uint32_t &iterationCount) override;
    CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan &saltBuf) override;
    CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan &verifierBuf, size_t &outVerifierLen) override;
    CHIP_ERROR GetSetupPasscode(uint32_t &setupPasscode) override;
    CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override;

private:
    BDeviceServicePropertyProvider *mPropertyProvider;
    bool mIsInitialized = false;

    // Helper methods for Init()
    CHIP_ERROR InitDiscriminator(chip::Optional<uint16_t> discriminator);
    CHIP_ERROR InitSpake2pIterationCount(uint32_t defaultSpake2pIterationCount);

    /**
     * Generates a random salt for the spake2p protocol, stores it in the property provider (base64 encoded), and
     * returns the decoded salt.
     *
     * @param[out] outSpake2pSaltVector The decoded salt.
     *
     * @return CHIP_ERROR on success, or an error code on failure.
     */
    CHIP_ERROR InitRandomSpake2pSalt(std::vector<uint8_t> &outSpake2pSaltVector);

    /**
     * Generates a random setup passcode, stores it in the property provider, and returns the generated passcode.
     *
     * @param[out] outSetupPasscode The generated setup passcode.
     *
     * @return CHIP_ERROR on success, or an error code on failure.
     */
    CHIP_ERROR InitRandomSetupPasscode(uint32_t &outSetupPasscode);

    /**
     * Generates a spake2p verifier from the given salt. If the client provided a verifier, then that verifier will
     * be validated against the generated one. If the client did not provide a verifier, or if the provided verifier
     * matches, then the generated verifier will be stored in the property provider.
     *
     * @param saltVector The salt to use for the spake2p protocol.
     * @param setupPasscode The setup passcode to use for the spake2p protocol.
     * @param providedBase64Verifier The verifier provided by the client, null if not provided.
     *
     * @return CHIP_ERROR on success, or an error code on failure.
     */
    CHIP_ERROR InitSpake2pVerifier(const std::vector<uint8_t> &saltVector,
                                   const uint32_t setupPasscode,
                                   const gchar *providedBase64Verifier);
};

} // namespace barton
