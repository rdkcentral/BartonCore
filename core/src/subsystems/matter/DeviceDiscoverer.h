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

//
// Created by tlea on 11/4/22.
//

#pragma once

#include "DiscoveredDeviceDetails.h"
#include "app/BufferedReadCallback.h"
#include "app/OperationalSessionSetup.h"
#include "app/ReadClient.h"
#include "lib/core/CHIPCallback.h"
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace zilker
{
    class DeviceDiscoverer : public chip::app::ReadClient::Callback
    {
    public:
        DeviceDiscoverer(chip::NodeId nodeId) :
            primaryAttributesRead(false), bufferedReadCallback(*this), nodeId(nodeId), readClient(nullptr),
            device(nullptr), onDeviceConnectedCallback(OnDeviceConnectedFn, this),
            onDeviceConnectionFailureCallback(OnDeviceConnectionFailureFn, this) {};

        ~DeviceDiscoverer();

        /**
         * @brief Start discovery asynchronously.
         *
         * @return std::future<bool> a future that will be set with the success/fail result (true/false)
         */
        std::future<bool> Start();

        void SetCompleted(bool isSuccess) { promise.set_value(isSuccess); }
        void ResultsUpdated();
        std::shared_ptr<DiscoveredDeviceDetails> GetDetails() { return details; };
        CHIP_ERROR DiscoverEndpoint(chip::Messaging::ExchangeManager &exchangeMgr,
                                    chip::OperationalDeviceProxy *device,
                                    chip::EndpointId endpointId);

        void OnDone(chip::app::ReadClient *apReadClient) override;
        void OnAttributeData(const chip::app::ConcreteDataAttributePath &path,
                             chip::TLV::TLVReader *data,
                             const chip::app::StatusIB &status) override;


    private:
        std::promise<bool> promise;
        bool primaryAttributesRead;
        chip::app::BufferedReadCallback bufferedReadCallback;
        chip::NodeId nodeId;
        std::shared_ptr<DiscoveredDeviceDetails> details;
        std::unordered_set<chip::EndpointId> discoveringEndpoints;
        std::unique_ptr<chip::app::ReadClient> readClient;
        chip::OperationalDeviceProxy *device;

        static void CleanupReader(intptr_t arg);
        static void CleanupDevice(intptr_t arg);
        static void DiscoverWorkFuncCb(intptr_t arg);
        static void OnDeviceConnectedFn(void *context,
                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                        const chip::SessionHandle &sessionHandle);
        static void OnDeviceConnectionFailureFn(void *context, const chip::ScopedNodeId &peerId, CHIP_ERROR error);

        chip::Callback::Callback<chip::OnDeviceConnected> onDeviceConnectedCallback;
        chip::Callback::Callback<chip::OnDeviceConnectionFailure> onDeviceConnectionFailureCallback;
    };
} // namespace zilker
