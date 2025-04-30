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

#pragma once

#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>
#include <platform/CommissionableDataProvider.h>

namespace barton
{
    /**
     * This class is the common parent for implementors of the CommissionableDataProvider.
     * It provides some common code for data validation.
     * Subclasses must implement the Getters for the data such that they either return valid data set by a Barton
     * client, or an appropriate error code when the data is either not available or invalid.
     * The Setters should remain unimplemented.
     */

    class BartonCommissionableDataProvider : public chip::DeviceLayer::CommissionableDataProvider
    {
    public:
        // These are implemented by the base class; it is the responsibility of subclasses to implement the
        // "Retrieve*()" methods below.
        CHIP_ERROR GetSetupDiscriminator(uint16_t &setupDiscriminator) override;
        CHIP_ERROR GetSpake2pIterationCount(uint32_t &iterationCount) override;
        CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan &saltBuf) override;
        CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan &verifierBuf, size_t &outVerifierLen) override;
        CHIP_ERROR GetSetupPasscode(uint32_t &setupPasscode) override;

        virtual CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override
        {
            (void) setupDiscriminator;
            return CHIP_ERROR_NOT_IMPLEMENTED;
        }

        virtual CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override
        {
            (void) setupPasscode;
            return CHIP_ERROR_NOT_IMPLEMENTED;
        }

    protected:
        virtual CHIP_ERROR RetrieveSetupDiscriminator(uint16_t &setupDiscriminator) = 0;
        virtual CHIP_ERROR RetrieveSpake2pIterationCount(uint32_t &iterationCount) = 0;
        virtual CHIP_ERROR RetrieveSpake2pSalt(chip::MutableByteSpan &saltBuf, size_t &outSaltLen) = 0;
        virtual CHIP_ERROR RetrieveSpake2pVerifier(chip::MutableByteSpan &verifierBuf, size_t &outVerifierLen) = 0;
        virtual CHIP_ERROR RetrieveSetupPasscode(uint32_t &setupPasscode) = 0;

    private:
        bool ValidateDiscriminator(uint16_t discriminator);
        bool ValidateSpake2pIterationCount(uint32_t iterationCount);
        bool ValidateSalt(chip::MutableByteSpan &saltBuf);
        // This method assumes that the additional parameters other than the verifier are valid
        bool ValidateVerifier(chip::MutableByteSpan &verifierBuf,
                              size_t verifierLen,
                              chip::MutableByteSpan &saltBuf,
                              const uint32_t passcode,
                              const uint32_t iterationCount);
    };
} // namespace barton
