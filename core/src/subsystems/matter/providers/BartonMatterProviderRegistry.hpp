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
// Created by Christian Leithner on 4/21/2025.
//

#pragma once

#include "glib.h"
#include <credentials/DeviceAttestationCredsProvider.h>

#include <memory>
#include <type_traits>

using namespace chip::Credentials;

namespace barton {

    class BartonMatterProviderRegistry final
    {
    public:
        static BartonMatterProviderRegistry &Instance()
        {
            static BartonMatterProviderRegistry instance;
            return instance;
        }

        template <typename T>
        typename std::enable_if<std::is_base_of<DeviceAttestationCredentialsProvider, T>::value, bool>::type
        RegisterBartonDACProvider(std::shared_ptr<T> provider)
        {
            g_return_val_if_fail(provider, false);
            g_return_val_if_fail(!deviceAttestationCredentialsProvider, false);

            deviceAttestationCredentialsProvider = std::static_pointer_cast<DeviceAttestationCredentialsProvider>(provider);
            return true;
        }

        std::shared_ptr<DeviceAttestationCredentialsProvider> GetBartonDACProvider()
        {
            return deviceAttestationCredentialsProvider;
        }

        private:
            BartonMatterProviderRegistry() = default;
            ~BartonMatterProviderRegistry() = default;

            std::shared_ptr<DeviceAttestationCredentialsProvider> deviceAttestationCredentialsProvider;
    };

} // namespace barton
