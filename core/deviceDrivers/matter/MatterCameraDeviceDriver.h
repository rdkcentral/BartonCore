//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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
 * Created by Raiyan Chowdhury on 3/3/25.
 */

#pragma once

#include "MatterDeviceDriver.h"
#include "matter/clusters/WebRTCTransportProvider.h"

namespace barton
{
    class MatterCameraDeviceDriver : public MatterDeviceDriver
    {
    public:
        MatterCameraDeviceDriver();

        bool ClaimDevice(DiscoveredDeviceDetails *details) override;

    protected:
        std::vector<MatterCluster *> GetClustersToSubscribeTo(const std::string &deviceId) override;

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

        void ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                             const std::string &deviceId,
                             icDeviceResource *resource,
                             const char *arg,
                             char **response,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle) override;

    private:
        static bool registeredWithFactory;

        std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId) override;

        class WebRTCTransportProviderClusterEventHandler : public WebRTCTransportProvider::EventHandler
        {
        public:
            void CommandCompleted(void *context, bool success) override
            {
                deviceDriver->OnDeviceWorkCompleted(context, success);
            }

            void HandleProvideOfferResponse(uint16_t webRTCSessionID, std::string &deviceId) override;

            MatterCameraDeviceDriver *deviceDriver;
        } webRtcTransportProviderClusterEventHandler;

        void ProvideOffer(std::forward_list<std::promise<bool>> &promises,
                          const std::string &deviceId,
                          const char *sdp,
                          chip::Messaging::ExchangeManager &exchangeMgr,
                          const chip::SessionHandle &sessionHandle);

        void ProvideICECandidates(std::forward_list<std::promise<bool>> &promises,
                                  const std::string &deviceId,
                                  const char *arg,
                                  chip::Messaging::ExchangeManager &exchangeMgr,
                                  const chip::SessionHandle &sessionHandle);

        void SendEndSession(std::forward_list<std::promise<bool>> &promises,
                            const std::string &deviceId,
                            const char *arg,
                            chip::Messaging::ExchangeManager &exchangeMgr,
                            const chip::SessionHandle &sessionHandle);
    };
} // namespace barton
