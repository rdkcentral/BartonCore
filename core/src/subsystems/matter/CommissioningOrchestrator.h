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
// Created by tlea on 10/31/22.
//

#pragma once

#include "DiscoveredDeviceDetails.h"
#include "Matter.h"
#include "app/OperationalSessionSetup.h"
#include "lib/core/NodeId.h"
#include "lib/core/Optional.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace zilker
{
    // keep this in sync with the ostream operator
    enum CommissioningStatus
    {
        // TODO fix these up to whatever makes sense
        Pending,
        InvalidSetupPayload,
        Started,
        MatterCommissioningCompleted,
        MatterCommissioningFailed,
        DeviceFound,
        DeviceConnected,
        DeviceNotFound,
        DiscoveryPending,
        DiscoveryStarted,
        DiscoveryCompleted,
        DiscoveryFailed,
        InternalError,
        CommissionedSuccessfully,
        CommissioningFailed,
        CommissioningCompleteFailed,
    };

    std::ostream &operator<<(std::ostream &strm, zilker::CommissioningStatus status);

    /**
     * Callback invoked when the status of the commissioning process changes
     *
     * nodeId is optional as it might not be valid for some of the events
     */
    typedef void (*OnDeviceCommissioningStatusChanged)(void *context,
                                                       chip::Optional<chip::NodeId> nodeId,
                                                       CommissioningStatus status);

    /**
     * CommissioningOrchestrator instances are created to handle a single device's commissioning process.  Progress
     * events as well as final success/failure are delivered asynchronously through the registered callback.
     */
    class CommissioningOrchestrator : public chip::Controller::DeviceDiscoveryDelegate,
                                      chip::Controller::DevicePairingDelegate
    {
    public:
        CommissioningOrchestrator(OnDeviceCommissioningStatusChanged onDeviceCommissioningStatusChangedCb,
                                  void *callbackContext) :
            finalNodeId(kUndefinedNodeId), commissioningStatus(Pending), callbackContext(callbackContext),
            discoveredNodeData(nullptr), onDeviceCommissioningStatusChanged(onDeviceCommissioningStatusChangedCb)
        {
        }

        ~CommissioningOrchestrator() override { Reset(); }

        /**
         * Attempt to commission a device matching the setup payload
         * This is synchronous.  The callback provided may get notified about status updates before
         * ultimately returning true or false to indicate the overall operational result.
         *
         * @param setupPayloadStr the string value of the setup payload (QR code or 11/21 digit manual code)
         * @param timeoutSeconds the maximum number of seconds allowed for performing the commissioning
         *
         * @return true upon successful commissioning
         */
        bool Commission(const char *setupPayloadStr, uint16_t timeoutSeconds);

        /**
         * Attempt to pair a device matching the nodeId
         * This is synchronous.  The callback provided may get notified about status updates before
         * ultimately returning true or false to indicate the overall operational result.
         *
         * @param nodeId node id of device to be paired
         * @param timeoutSeconds the maximum number of seconds allowed for performing the pairing
         *
         * @return true upon successful commissioning
         */
        bool Pair(chip::NodeId nodeId, uint16_t timeoutSeconds);

        /**
         * Resets the state of the orchestrator instance for cleanup / reuse.
         */
        void Reset();

        ////////////// DevicePairingDelegate overrides ////////////////////
        void OnCommissioningSuccess(chip::PeerId peerId) override;
        void OnCommissioningFailure(
            chip::PeerId peerId,
            CHIP_ERROR error,
            chip::Controller::CommissioningStage stageFailed,
            chip::Optional<chip::Credentials::AttestationVerificationResult> additionalErrorInfo) override;
        void OnCommissioningStatusUpdate(chip::PeerId peerId,
                                         chip::Controller::CommissioningStage stageCompleted,
                                         CHIP_ERROR error) override;
        void OnReadCommissioningInfo(const chip::Controller::ReadCommissioningInfo &info) override;
        ////////////// DevicePairingDelegate overrides ////////////////////

        ////////////// DeviceDiscoveryDelegate overrides ////////////////////
        void OnDiscoveredDevice(const chip::Dnssd::CommissionNodeData &nodeData) override;
        ////////////// DeviceDiscoveryDelegate overrides ////////////////////

    private:
        chip::NodeId finalNodeId;
        std::string setupPayload;
        CommissioningStatus commissioningStatus;
        void *callbackContext;
        std::mutex mtx;
        std::condition_variable cond;
        chip::Dnssd::CommissionNodeData *discoveredNodeData;

        /**
         * Set the status and invoke callback.
         *
         * mtx must be held.
         *
         * @param newStatus
         */
        void SetCommissioningStatus(CommissioningStatus newStatus)
        {
            commissioningStatus = newStatus;
            void *localCallbackContext = callbackContext;
            chip::NodeId localNodeId = finalNodeId;
            mtx.unlock();
            onDeviceCommissioningStatusChanged(
                localCallbackContext, chip::Optional<chip::NodeId>(localNodeId), newStatus);
            cond.notify_all();
            mtx.lock();
        }

        /**
         * @brief Per Matter 1.0 Section 5.5 (Commissioning Flows), handle steps 13-15 by connecting over CASE and
         * sending the CommissioningComplete message.
         *
         * @param nodeId
         * @return true on success
         */
        bool SendCommissioningComplete(chip::NodeId nodeId);

        OnDeviceCommissioningStatusChanged onDeviceCommissioningStatusChanged;

        void CommissionWorkFunc();
        static void CommissionWorkFuncCb(intptr_t arg);
    };
} // namespace zilker
