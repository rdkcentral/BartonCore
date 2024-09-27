//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2023 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
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

        ~DeviceDiscoverer() = default;

        /**
         * @brief Start discovery asynchronously.
         *
         * @return std::future<bool> a future that will be set with the success/fail result (true/false)
         */
        std::future<bool> Start();

        void SetCompleted(bool isSuccess) { promise.set_value(isSuccess); }
        void ResultsUpdated();
        std::shared_ptr<DiscoveredDeviceDetails> GetDetails() { return details; };
        CHIP_ERROR DiscoverEndpoint(chip::Messaging::ExchangeManager &exchangeMgr, chip::OperationalDeviceProxy *device, chip::EndpointId endpointId);

        void OnDone(chip::app::ReadClient * apReadClient) override;
        void OnAttributeData(const chip::app::ConcreteDataAttributePath & path, chip::TLV::TLVReader * data,
                         const chip::app::StatusIB & status) override;


    private:
        std::promise<bool> promise;
        bool primaryAttributesRead;
        chip::app::BufferedReadCallback bufferedReadCallback;
        chip::NodeId nodeId;
        std::shared_ptr<DiscoveredDeviceDetails> details;
        std::unordered_set<chip::EndpointId> discoveringEndpoints;
        std::unique_ptr<chip::app::ReadClient> readClient;
        chip::OperationalDeviceProxy *device;

        static void DiscoverWorkFuncCb(intptr_t arg);
        static void OnDeviceConnectedFn(void *context,
                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                        const chip::SessionHandle &sessionHandle);
        static void OnDeviceConnectionFailureFn(void *context, const chip::ScopedNodeId &peerId, CHIP_ERROR error);

        chip::Callback::Callback<chip::OnDeviceConnected> onDeviceConnectedCallback;
        chip::Callback::Callback<chip::OnDeviceConnectionFailure> onDeviceConnectionFailureCallback;
    };
}
