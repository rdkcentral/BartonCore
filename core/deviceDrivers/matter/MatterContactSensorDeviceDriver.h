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
// Created by Raiyan Chowdhury on 8/9/2024.
//

#pragma once

#include "MatterDeviceDriver.h"
#include "matter/clusters/BooleanState.h"

namespace barton

{
    class MatterContactSensorDeviceDriver : public MatterDeviceDriver
    {
    public:
        MatterContactSensorDeviceDriver();

        bool ClaimDevice(DiscoveredDeviceDetails *details) override;

    protected:
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

        void SetTamperedEndpointResource(const std::string &deviceId, bool tampered) override;

    private:
        static bool registeredWithFactory;

        std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId) override;

        class BooleanStateClusterEventHandler : public BooleanState::EventHandler
        {
        public:
            void StateValueChanged(std::string &deviceUuid, bool state) override;
            void StateValueReadComplete(std::string &deviceUuid, bool state, void *asyncContext) override;
            MatterContactSensorDeviceDriver *deviceDriver;
        } booleanStateClusterEventHandler;
    };
} // namespace barton
