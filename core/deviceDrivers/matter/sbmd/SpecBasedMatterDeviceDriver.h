//------------------------------ tabstop = 4 ----------------------------------
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
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 10/20/25
 */

#pragma once

#include "../MatterDeviceDriver.h"
#include "../MatterDevice.h"
#include "SbmdSpec.h"
#include <memory>

namespace barton
{
    class SpecBasedMatterDeviceDriver : public MatterDeviceDriver
    {
    public:
        SpecBasedMatterDeviceDriver(std::shared_ptr<SbmdSpec> spec);
        std::vector<uint16_t> GetSupportedDeviceTypes() override;

        bool AddDevice(std::unique_ptr<MatterDevice> device) override;

    protected:
        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override;

        bool DoRegisterResources(icDevice *device) override;

        void DoReadResource(std::forward_list<std::promise<bool>> &promises,
                            const std::string &deviceId,
                            icDeviceResource *resource,
                            char **value,
                            chip::Messaging::ExchangeManager &exchangeMgr,
                            const chip::SessionHandle &sessionHandle) override;

        bool WriteResource(std::forward_list<std::promise<bool>> &promises,
                           const std::string &deviceId,
                           icDeviceResource *resource,
                           const char *previousValue,
                           const char *newValue,
                           chip::Messaging::ExchangeManager &exchangeMgr,
                           const chip::SessionHandle &sessionHandle) override;

        void ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                             const std::string &deviceId,
                             icDeviceResource *resource,
                             const char *arg,
                             char **response,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle) override;

    private:
        static bool registeredWithFactory;

        std::shared_ptr<SbmdSpec> spec;

        /**
         * Search the device data cache to find the correct attribute path.
         * When endpoints have overlapping clusters, this searches for endpoints
         * that match the driver's supported device types to disambiguate.
         */
        bool GetAttributePath(const MatterDevice &device,
                              const SbmdAttribute &attributeInfo,
                              app::ConcreteAttributePath &outPath);

        /**
         * Create and configure a script engine with all mappers from the spec
         * @param deviceId The device ID for the script instance
         * @return A configured SbmdScript instance
         */
        std::unique_ptr<SbmdScript> CreateConfiguredScript(const std::string &deviceId);

        uint8_t ConvertModesToBitmask(const std::vector<std::string> &modes);
    };
} // namespace barton
