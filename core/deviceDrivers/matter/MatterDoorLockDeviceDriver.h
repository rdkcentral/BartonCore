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
 * Created by Thomas Lea on 4/4/2022..
 */

#pragma once

#include "MatterDeviceDriver.h"
#include "clusters/DoorLock.h"

namespace barton
{
    class MatterDoorLockDeviceDriver : public MatterDeviceDriver,
                                       DoorLock::EventHandler
    {
    public:
        MatterDoorLockDeviceDriver();

        // DoorLock cluster callbacks
        void CommandCompleted(void *context, bool success) override { OnDeviceWorkCompleted(context, success); };
        void WriteRequestCompleted(void *context, bool success) override { OnDeviceWorkCompleted(context, success); };
        void LockStateChanged(std::string &deviceUuid,
                              chip::app::Clusters::DoorLock::DlLockState lockState,
                              void *asyncContext) override;
        void LockStateReadComplete(std::string &deviceUuid,
                                   chip::app::Clusters::DoorLock::DlLockState lockState,
                                   void *asyncContext) override;

    protected:
        std::vector<uint16_t> GetSupportedDeviceTypes() override;
        void DoSynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                 const std::string &deviceId,
                                 chip::Messaging::ExchangeManager &exchangeMgr,
                                 const chip::SessionHandle &sessionHandle) override;

        bool DoRegisterResources(icDevice *device) override;

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

        std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId,
                    std::shared_ptr<DeviceDataCache> deviceDataCache) override;
    };
} // namespace barton
