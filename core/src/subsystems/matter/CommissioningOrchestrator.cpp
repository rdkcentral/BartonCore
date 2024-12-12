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

#define LOG_TAG     "MatterCommishOrch"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "CommissioningOrchestrator.h"
#include "DeviceDiscoverer.h"
#include "DiscoveredDeviceDetails.h"
#include "DiscoveredDeviceDetailsStore.h"
#include "Matter.h"
#include "matter/MatterDeviceDriver.h"
#include "matter/MatterDriverFactory.h"
#include "matterSubsystem.h"
#include "platform/PlatformManager.h"

extern "C" {
#include <device-driver/device-driver.h>
#include <device/icDevice.h>
#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
}

#include <chrono>

#define CONNECT_DEVICE_TIMEOUT_SECONDS 15

namespace zilker
{
    // This is synchronous up to timeoutSeconds
    bool CommissioningOrchestrator::Commission(const char *setupPayloadStr, uint16_t timeoutSeconds)
    {
        icDebug();

        std::unique_lock<std::mutex> lock_guard(mtx);
        Reset();
        setupPayload = setupPayloadStr;
        SetCommissioningStatus(Pending);

        // TODO: this only works if we are doing one commissioning at a time... probably want Matter to dispatch to
        // multiple
        Matter::GetInstance().GetCommissioner().RegisterDeviceDiscoveryDelegate(this);
        Matter::GetInstance().GetCommissioner().RegisterPairingDelegate(this);

#ifdef BARTON_CONFIG_MATTER_SELF_SIGNED_OP_CREDS_ISSUER
        // We don't need to get an auth token for the self-signed ca operational credentials issuer, so don't bother.
        bool worked = true;
#else
        bool worked = Matter::GetInstance().PrimeNewAuthorizationToken();
#endif
        if (worked)
        {
            chip::DeviceLayer::PlatformMgr().ScheduleWork(CommissionWorkFuncCb, reinterpret_cast<intptr_t>(this));
        }
        else
        {
            icError("Failed to prime Matter with a new token. Cannot continue commissioning");
            SetCommissioningStatus(CommissioningFailed);
            return false;
        }

        std::chrono::duration<uint16_t> duration(timeoutSeconds);
        auto waitingUntil =
            std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::seconds>(duration);

        // wait until timeout or until commissioningStatus reaches one of our two exit values
        if (!cond.wait_until(lock_guard, waitingUntil, [this]() {
                return (this->commissioningStatus == CommissionedSuccessfully ||
                        this->commissioningStatus == CommissioningFailed);
            }))
        {
            icWarn("Timed out waiting for commissioning to complete");
            // cancel and clean up anything that is still running so it doesnt crash if it continues to do something
        }
        else
        {
            icDebug("Done waiting for commissioning");
        }

        // TODO: this only works if we are doing one commissioning at a time... probably want Matter to dispatch to
        // multiple
        Matter::GetInstance().GetCommissioner().RegisterDeviceDiscoveryDelegate(nullptr);
        Matter::GetInstance().GetCommissioner().RegisterPairingDelegate(nullptr);

        auto endStatus = this->commissioningStatus;

        lock_guard.unlock();

        if (endStatus == CommissionedSuccessfully)
        {
            return this->Pair(finalNodeId, timeoutSeconds);
        }

        return false;
    }

    bool CommissioningOrchestrator::Pair(chip::NodeId nodeId, uint16_t timeoutSeconds)
    {
        if (nodeId == 0)
        {
            return false;
        }

        icDebug("Pairing device with node id %" PRIx64, nodeId);

        bool result = false;

        // First, we need to complete the commissioning process from the admin perspective (steps 13-15 from Matter 1.0
        // section 5.5)
        if (!SendCommissioningComplete(nodeId))
        {
            icError("Failed to complete commissioning");
            SetCommissioningStatus(CommissioningCompleteFailed);
            return false;
        }

        std::unique_lock<std::mutex> lock_guard(mtx);

        this->finalNodeId = nodeId;
        std::chrono::duration<uint16_t> duration(timeoutSeconds);
        auto waitingUntil =
            std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::seconds>(duration);

        SetCommissioningStatus(DiscoveryPending);

        DeviceDiscoverer discoverer(finalNodeId);
        std::future<bool> success = discoverer.Start();

        if (success.wait_until(waitingUntil) == std::future_status::ready)
        {
            if (success.get())
            {
                icDebug("Done waiting for discovery");
                SetCommissioningStatus(DiscoveryCompleted);
            }
            else
            {
                icDebug("Device discovery failed");
                SetCommissioningStatus(DiscoveryFailed);
            }
        }
        else
        {
            icDebug("Device discovery did not complete");
            SetCommissioningStatus(DiscoveryFailed);
        }

        if (commissioningStatus == DiscoveryCompleted)
        {
            auto matterDetails = discoverer.GetDetails();

            MatterDeviceDriver *driver = MatterDriverFactory::Instance().GetDriver(matterDetails.get());
            if (driver != nullptr)
            {
                scoped_icStringHashMap *endpointProfileMap = driver->GetEndpointToProfileMap(matterDetails.get());

                scoped_generic char *uuid = stringBuilder("%016" PRIx64, finalNodeId);
                // these are zilker details (technology neutral)
                DeviceFoundDetails details {
                    .deviceDriver = driver->GetDriver(),
                    .subsystem = MATTER_SUBSYSTEM_NAME,
                    .deviceClass = driver->GetDeviceClass(),
                    .deviceClassVersion = driver->GetDeviceClassVersion(),
                    .deviceUuid = uuid,
                    .manufacturer = matterDetails->vendorName->c_str(),
                    .model = matterDetails->productName->c_str(),
                    .hardwareVersion = matterDetails->hardwareVersion->c_str(),
                    .firmwareVersion = matterDetails->softwareVersion->c_str(),
                    .endpointProfileMap = endpointProfileMap,
                };

                // save off the details so the pairing functions can look them up if they need
                DiscoveredDeviceDetailsStore::Instance().PutTemporarily(uuid, matterDetails);

                if (deviceServiceDeviceFound(&details, true))
                {
                    result = true;
                }
            }
        }

        if (result)
        {
            SetCommissioningStatus(CommissionedSuccessfully);
        }
        else
        {
            SetCommissioningStatus(CommissioningFailed);
        }

        return result;
    }

    bool CommissioningOrchestrator::SendCommissioningComplete(chip::NodeId nodeId)
    {
        icDebug("nodeId %" PRIx64, nodeId);

        std::promise<bool> success;
        bool ret;

        chip::Callback::Callback<chip::OnDeviceConnected> onDeviceConnectedCallback(
            [](void *context, chip::Messaging::ExchangeManager &exchangeMgr, const chip::SessionHandle &sessionHandle) {
                chip::app::CommandSender commandSender(nullptr, &exchangeMgr);

                chip::app::CommandSender::PrepareCommandParameters prepareParams;
                chip::app::CommandSender::FinishCommandParameters finishParams;
                // TODO what are these command refs (passed to SetCommandRef)?
                prepareParams.SetStartDataStruct(true).SetCommandRef(0);
                finishParams.SetEndDataStruct(true).SetCommandRef(0);

                chip::app::CommandPathParams commandPath(
                    0, /* endpoint 0 holds the GeneralCommissioning cluster*/
                    0, /* group not used */
                    chip::app::Clusters::GeneralCommissioning::Id,
                    chip::app::Clusters::GeneralCommissioning::Commands::CommissioningComplete::Id,
                    (chip::app::CommandPathFlags::kEndpointIdValid));

                commandSender.PrepareCommand(commandPath, prepareParams);
                commandSender.FinishCommand(finishParams);
                static_cast<std::promise<bool> *>(context)->set_value(commandSender.SendCommandRequest(sessionHandle) ==
                                                                      CHIP_NO_ERROR);
            },
            &success);

        chip::Callback::Callback<chip::OnDeviceConnectionFailure> onDeviceConnectionFailureCallback(
            [](void *context, const chip::ScopedNodeId &peerId, CHIP_ERROR error) {
                icError("Failed to connect to device to send commissioning complete");
                static_cast<std::promise<bool> *>(context)->set_value(false);
            },
            &success);

        chip::DeviceLayer::PlatformMgr().LockChipStack();

        Matter &matter = Matter::GetInstance();
        if (matter.GetCommissioner().GetConnectedDevice(
                nodeId, &onDeviceConnectedCallback, &onDeviceConnectionFailureCallback) != CHIP_NO_ERROR)
        {
            icError("Failed to get device connection");
            success.set_value(false);
        }

        chip::DeviceLayer::PlatformMgr().UnlockChipStack();
        ret = success.get_future().get();

        return ret;
    }

    void CommissioningOrchestrator::CommissionWorkFunc()
    {
        icDebug();

        CHIP_ERROR err = CHIP_NO_ERROR;
        Matter &matter = Matter::GetInstance(); // for readability
        chip::NodeId randomId = Matter::GetRandomOperationalNodeId();

        std::unique_lock<std::mutex> lock_guard(mtx);

        SetupPayload parsedPayload;
        if ((err = Matter::ParseSetupPayload(setupPayload, parsedPayload)) != CHIP_NO_ERROR)
        {
            icError("Failed to parse setup payload: %s", err.AsString());
            SetCommissioningStatus(InvalidSetupPayload);
            return;
        }

        chip::Transport::PeerAddress peerAddress;
        chip::Controller::DiscoveryType discoveryType = chip::Controller::DiscoveryType::kAll;

        // if we got rendezvous information that says its on the network, lets prefer that on UDP over anything else
        if (parsedPayload.rendezvousInformation.HasValue())
        {
            if (parsedPayload.rendezvousInformation.Value().Has(chip::RendezvousInformationFlag::kOnNetwork))
            {
                peerAddress.SetTransportType(chip::Transport::Type::kUdp);
                discoveryType = chip::Controller::DiscoveryType::kDiscoveryNetworkOnly;
            }
            else if (parsedPayload.rendezvousInformation.Value().Has(chip::RendezvousInformationFlag::kBLE))
            {
                peerAddress.SetTransportType(chip::Transport::Type::kBle);
            }
        }

        chip::Controller::CommissioningParameters commissioningParams;
        err = matter.GetCommissioningParams(commissioningParams);

        CommissioningStatus status = InternalError;

        if (err != CHIP_NO_ERROR)
        {
            icError("GetCommissioningParams failed with error: %s", err.AsString());
            status = InternalError;
        }
        else
        {
            icDebug("Calling PairDevice()");
            err =
                matter.GetCommissioner().PairDevice(randomId, setupPayload.c_str(), commissioningParams, discoveryType);
            if (err != CHIP_NO_ERROR)
            {
                icError("PairDevice failed with error: %s", err.AsString());
                status = CommissioningFailed;
            }
            else
            {
                status = Started;
            }
        }

        SetCommissioningStatus(status);
    }

    void CommissioningOrchestrator::CommissionWorkFuncCb(intptr_t arg)
    {
        auto *orchestrator = reinterpret_cast<CommissioningOrchestrator *>(arg);
        orchestrator->CommissionWorkFunc();
    }

    void CommissioningOrchestrator::OnCommissioningSuccess(chip::PeerId peerId)
    {
        icDebug("Node %016" PRIx64 " commissioned successfully", peerId.GetNodeId());

        std::unique_lock<std::mutex> lock_guard(mtx);
        finalNodeId = peerId.GetNodeId();
        SetCommissioningStatus(CommissionedSuccessfully);
    }

    void CommissioningOrchestrator::OnCommissioningFailure(
        chip::PeerId peerId,
        CHIP_ERROR error,
        chip::Controller::CommissioningStage stageFailed,
        chip::Optional<chip::Credentials::AttestationVerificationResult> additionalErrorInfo)
    {
        icDebug("Node %016" PRIx64 " commissioning failure", peerId.GetNodeId());

        std::unique_lock<std::mutex> lock_guard(mtx);
        finalNodeId = peerId.GetNodeId();
        SetCommissioningStatus(CommissioningFailed);
    }

    void CommissioningOrchestrator::OnCommissioningStatusUpdate(chip::PeerId peerId,
                                                                chip::Controller::CommissioningStage stageCompleted,
                                                                CHIP_ERROR error)
    {
        icDebug("Node %016" PRIx64 " stageCompleted='%s' error='%s'",
                peerId.GetNodeId(),
                StageToString(stageCompleted),
                ErrorStr(error));
    }

    void CommissioningOrchestrator::OnReadCommissioningInfo(const chip::Controller::ReadCommissioningInfo &info)
    {
        icDebug("Node %016" PRIx64, info.remoteNodeId);
    }

    void CommissioningOrchestrator::OnDiscoveredDevice(const chip::Dnssd::CommissionNodeData &nodeData)
    {
        std::unique_lock<std::mutex> lock_guard(mtx);

        // only process the first response
        if (discoveredNodeData == nullptr)
        {
            char buf[chip::Inet::IPAddress::kMaxStringLength];
            nodeData.ipAddress[0].ToString(buf);

            icLogDebug(LOG_TAG, "%s: %s", __func__, buf);
            nodeData.LogDetail();

            discoveredNodeData = new chip::Dnssd::CommissionNodeData;
            *discoveredNodeData = nodeData;

            SetCommissioningStatus(DeviceFound);
        }
    }

    void CommissioningOrchestrator::Reset()
    {
        delete discoveredNodeData;
        discoveredNodeData = nullptr;
    }

    // KEEP THIS IN SYNC WITH THE ENUM!
    std::ostream &operator<<(std::ostream &strm, zilker::CommissioningStatus status)
    {
        const std::string name[] = {"Pending",
                                    "InvalidSetupPayload",
                                    "Started",
                                    "MatterCommissioningCompleted",
                                    "MatterCommissioningFailed",
                                    "DeviceFound",
                                    "DeviceConnected",
                                    "DeviceNotFound",
                                    "DiscoveryPending",
                                    "DiscoveryStarted",
                                    "DiscoveryCompleted",
                                    "DiscoveryFailed",
                                    "InternalError",
                                    "CommissionedSuccessfully",
                                    "CommissioningFailed",
                                    "CommissioningCompleteFailed"};
        return strm << name[status];
    }

} // namespace zilker
