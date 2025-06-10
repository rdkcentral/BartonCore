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

#include "BartonCommissionableDataProvider.hpp"
#include "glib.h"
#include <functional>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>
#include <platform/CommissionableDataProvider.h>

extern "C" {
#include <provider/barton-core-property-provider.h>
}

namespace barton
{
class DefaultCommissionableDataProvider : public barton::BartonCommissionableDataProvider
{
public:
    CHIP_ERROR RetrieveSetupDiscriminator(uint16_t &setupDiscriminator) override;
    CHIP_ERROR RetrieveSpake2pIterationCount(uint32_t &iterationCount) override;
    CHIP_ERROR RetrieveSpake2pSalt(chip::MutableByteSpan &saltBuf, size_t &outSaltLen) override;
    CHIP_ERROR RetrieveSpake2pVerifier(chip::MutableByteSpan &verifierBuf, size_t &outVerifierLen) override;
    CHIP_ERROR RetrieveSetupPasscode(uint32_t &setupPasscode) override;

private:
    bool Base64StringToMutableByteSpan(const char *base64String, chip::MutableByteSpan &outBuf, size_t &outBufLen);
};

} // namespace barton
