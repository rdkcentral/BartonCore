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
// Created by tlea on 11/7/22.
//

#pragma once

#include "app/WriteClient.h"
#include "lib/core/DataModelTypes.h"
#include "subsystems/matter/DeviceDataCache.h"
#include "subsystems/matter/MatterCommon.h"
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace barton
{
    class MatterCluster : public chip::app::CommandSender::Callback,
                          public chip::app::WriteClient::Callback,
                          public chip::app::ClusterStateCache::Callback
    {
    public:
        class EventHandler
        {
        public:
            virtual void CommandCompleted(void *context, bool success) {};
            virtual void WriteRequestCompleted(void *context, bool success) {};
        };

        MatterCluster(EventHandler *handler,
                      std::string deviceId,
                      chip::EndpointId endpointId,
                      chip::ClusterId clusterId,
                      std::shared_ptr<DeviceDataCache> deviceDataCache) :
            eventHandler(handler), deviceId(std::move(deviceId)), endpointId(endpointId),
            clusterId(clusterId), deviceDataCache(std::move(deviceDataCache))
        {
            nodeId = Subsystem::Matter::UuidToNodeId(this->deviceId.c_str());
        };

        virtual ~MatterCluster() = default;

        // ReadClient callbacks
        virtual void OnDone(chip::app::ReadClient * apReadClient) override;

        // CommandSender callbacks
        virtual void OnResponse(chip::app::CommandSender *apCommandSender,
                                const chip::app::ConcreteCommandPath &aPath,
                                const chip::app::StatusIB &aStatusIB,
                                chip::TLV::TLVReader *apData) override;
        virtual void OnError(const chip::app::CommandSender *apCommandSender, CHIP_ERROR aError) override;
        virtual void OnDone(chip::app::CommandSender *apCommandSender) override { delete apCommandSender; }

        // WriteClient callbacks
        virtual void OnResponse(const chip::app::WriteClient *apWriteClient,
                                const chip::app::ConcreteDataAttributePath &path,
                                chip::app::StatusIB status) override;
        virtual void OnError(const chip::app::WriteClient *apWriteClient, CHIP_ERROR aError) override;
        virtual void OnDone(chip::app::WriteClient *apWriteClient) override { delete apWriteClient; }

        /**
         * @brief Get the remote device endpoint ID for this cluster instance.
         *        Note this is not to be confused with deviceService Device's endpoint
         *        ID property; drivers are responsible for translation.
         *
         * @return chip::EndpointId
         */
        inline chip::EndpointId GetEndpointId() { return endpointId; }

        /**
         * @brief Get the remote device's node id for this cluster instance.
         *
         * @return chip::NodeId
         */
        inline chip::NodeId GetNodeId() { return nodeId; }

        /**
         * @brief Get the remote device's node ID as a deviceUuid for this cluster instance
         *
         * @return std::string
         */
        inline std::string GetDeviceId() { return deviceId; }

    protected:
        std::mutex mtx;
        EventHandler *eventHandler;
        std::string deviceId;
        chip::NodeId nodeId;
        chip::EndpointId endpointId;
        chip::ClusterId clusterId;
        std::shared_ptr<DeviceDataCache> deviceDataCache;
        void *commandContext {};      // only one command operation can be in flight at a time
        void *writeRequestContext {}; // only one write request operation can be in flight at a time

        /**
         * Context for on-demand attribute reads that send a read request to the device rather than query the cluster
         * state cache for the attribute value.
         */
        struct OnDemandReadContext
        {
            /**
             * @brief Construct a new OnDemandReadContext object
             *
             * @param context The caller's context pointer to be passed back in the read complete callback
             * @param eventHandler The cluster's event handler to invoke the read complete callback on
             * @param deviceId The device UUID of the device being red from
            */
            OnDemandReadContext(void *context,
                                MatterCluster::EventHandler *eventHandler,
                                std::string &deviceId) :
                baseReadContext(context), eventHandler(eventHandler), deviceId(deviceId)
            {
            }
            void *baseReadContext;
            MatterCluster::EventHandler *eventHandler;
            std::string &deviceId;
        };

        bool
        SendCommand(chip::app::CommandSender *commandSender, const chip::SessionHandle &sessionHandle, void *context);

        bool
        SendWriteRequest(chip::app::WriteClient *writeClient, const chip::SessionHandle &sessionHandle, void *context);

    private:
    };
} // namespace barton
