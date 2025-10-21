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
#include "matter/clusters/MatterCluster.h"
#include "matter/sbmd/SbmdCluster.h"
#include "SbmdSpec.h"
#include <memory>

namespace barton
{
    class SpecBasedMatterDeviceDriver : public MatterDeviceDriver,
                                        public SbmdCluster::EventHandler
    {
    public:
        SpecBasedMatterDeviceDriver(std::unique_ptr<SbmdSpec> spec);

        bool ClaimDevice(DiscoveredDeviceDetails *details) override;

    protected:
        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override;

        void SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                               const std::string &deviceId,
                               chip::Messaging::ExchangeManager &exchangeMgr,
                               const chip::SessionHandle &sessionHandle) override;

        void FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                        const std::string &deviceId,
                                        icInitialResourceValues *initialResourceValues,
                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                        const chip::SessionHandle &sessionHandle) override;

        bool RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues) override;

        void ReadResource(std::forward_list<std::promise<bool>> &promises,
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

    private:
        static bool registeredWithFactory;

        std::unique_ptr<SbmdSpec> spec;

        void AttributeReadComplete(std::string &deviceUuid,
                                   uint16_t clusterId,
                                   uint16_t attributeId,
                                   chip::TLV::TLVReader &reader,
                                   void *asyncContext) override;

        bool ExecuteMapperScript(const std::string &script,
                                 const std::string &argumentName,
                                 const char *argumentValue,
                                 char **outputValue);

        uint8_t ConvertModesToBitmask(const std::vector<std::string> &modes);
        ResourceCachingPolicy ConvertCachingPolicy(const std::string &policy);

        std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId) override;
    };
} // namespace barton
