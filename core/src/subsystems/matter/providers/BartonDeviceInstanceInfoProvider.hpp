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
 * Modified by Comcast to make all relevant properties read from the
 * PosixConfig class (filesystem).
 * Copyright 2023 Project CHIP Authors
 * All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 */

#pragma once

#include <platform/DeviceInstanceInfoProvider.h>

namespace barton {

    class BartonDeviceInstanceInfoProvider final : public chip::DeviceLayer::DeviceInstanceInfoProvider
    {
    public:
        BartonDeviceInstanceInfoProvider() = default;
        virtual ~BartonDeviceInstanceInfoProvider() = default;

        /**
         * @brief Checks if all required properties are available.
         *        Property results (found/missing/required/optional) are logged.
         *
         * @return true if all required properties are available, false otherwise.
         */
        bool ValidateProperties();

        CHIP_ERROR GetVendorName(char *buf, size_t bufSize) override;
        CHIP_ERROR GetVendorId(uint16_t &vendorId) override;
        CHIP_ERROR GetProductName(char *buf, size_t bufSize) override;
        CHIP_ERROR GetProductId(uint16_t &productId) override;
        CHIP_ERROR GetPartNumber(char *buf, size_t bufSize) override;
        CHIP_ERROR GetProductURL(char *buf, size_t bufSize) override;
        CHIP_ERROR GetProductLabel(char *buf, size_t bufSize) override;
        CHIP_ERROR GetSerialNumber(char *buf, size_t bufSize) override;
        CHIP_ERROR GetManufacturingDate(uint16_t &year, uint8_t &month, uint8_t &day) override;
        CHIP_ERROR GetHardwareVersion(uint16_t &hardwareVersion) override;
        CHIP_ERROR GetHardwareVersionString(char *buf, size_t bufSize) override;
        CHIP_ERROR GetSoftwareVersionString(char *buf, size_t bufSize) override;
        CHIP_ERROR GetRotatingDeviceIdUniqueId(chip::MutableByteSpan &uniqueIdSpan) override;
    };

inline barton::BartonDeviceInstanceInfoProvider & GetBartonDeviceInstanceInfoProvider()
{
    static barton::BartonDeviceInstanceInfoProvider sInstance;
    return sInstance;
}
} // namespace barton
