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

namespace barton
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
        Matter::GetInstance().GetCommissioner()->RegisterDeviceDiscoveryDelegate(this);
        Matter::GetInstance().GetCommissioner()->RegisterPairingDelegate(this);

        chip::DeviceLayer::PlatformMgr().ScheduleWork(CommissionWorkFuncCb, reinterpret_cast<intptr_t>(this));

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
        Matter::GetInstance().GetCommissioner()->RegisterDeviceDiscoveryDelegate(nullptr);
        Matter::GetInstance().GetCommissioner()->RegisterPairingDelegate(nullptr);

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

        std::string uuid = Subsystem::Matter::NodeIdToUuid(nodeId);
        auto deviceDataCache = std::make_shared<DeviceDataCache>(uuid, Matter::GetInstance().GetCommissioner());
        std::future<bool> success = deviceDataCache->Start();

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
            // GetDriver needs to inspect the cache to determine driver type
            MatterDeviceDriver *driver = MatterDriverFactory::Instance().GetDriver(deviceDataCache.get());
            if (driver != nullptr)
            {
                // Extract values before transferring ownership
                std::string manufacturer, model, hardwareVersion, firmwareVersion;
                deviceDataCache->GetVendorName(manufacturer);
                deviceDataCache->GetProductName(model);
                deviceDataCache->GetHardwareVersionString(hardwareVersion);
                deviceDataCache->GetSoftwareVersionString(firmwareVersion);

                std::unique_ptr<MatterDevice> device =
                    std::make_unique<MatterDevice>(uuid, deviceDataCache);

                // Transfer ownership to driver
                driver->AddDevice(std::move(device));

                // these are device service details (technology neutral)
                DeviceFoundDetails details {
                    .deviceDriver = driver->GetDriver(),
                    .subsystem = MATTER_SUBSYSTEM_NAME,
                    .deviceClass = driver->GetDeviceClass(),
                    .deviceClassVersion = driver->GetDeviceClassVersion(),
                    .deviceUuid = uuid.c_str(),
                    .manufacturer = manufacturer.c_str(),
                    .model = model.c_str(),
                    .hardwareVersion = hardwareVersion.c_str(),
                    .firmwareVersion = firmwareVersion.c_str(),
                    .endpointProfileMap = nullptr
                };

                if (deviceServiceDeviceFound(&details, true))
                {
                    // Retrieve device from driver after ownership transfer
                    auto retrievedDevice = driver->GetDevice(uuid);
                    if (retrievedDevice != nullptr)
                    {
                        // reprocessing the attributes in the cache will trigger the callbacks from registered clusters which
                        //  can update resources
                        retrievedDevice->GetDeviceDataCache()->RegenerateAttributeReport();
                        result = true;
                    }
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
        if (matter.GetCommissioner()->GetConnectedDevice(
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
                matter.GetCommissioner()->PairDevice(randomId, setupPayload.c_str(), commissioningParams, discoveryType);
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
    std::ostream &operator<<(std::ostream &strm, barton::CommissioningStatus status)
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

} // namespace barton
