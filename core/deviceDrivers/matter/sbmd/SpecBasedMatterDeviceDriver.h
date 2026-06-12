//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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

#include "../MatterDevice.h"
#include "../MatterDeviceDriver.h"
#include "SbmdDriver.h"
#include "mquickjs/SbmdResultExecutor.h"
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace barton
{
    class SpecBasedMatterDeviceDriver : public MatterDeviceDriver
    {
    public:
        SpecBasedMatterDeviceDriver(SbmdDriver *driver);

        std::vector<uint16_t> GetSupportedDeviceTypes() override;

        uint16_t GetSupportedVendorId() const override;
        uint16_t GetSupportedProductId() const override;
        bool IsVendorSpecificDriver() const override;

        bool AddDevice(std::unique_ptr<MatterDevice> device) override;

    protected:
        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override;

        bool DoRegisterResources(icDevice *device) override;

        void DoSynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                 const std::string &deviceId,
                                 chip::Messaging::ExchangeManager &exchangeMgr,
                                 const chip::SessionHandle &sessionHandle) override;

        void DoReadResource(std::forward_list<std::promise<bool>> &promises,
                            const std::string &deviceId,
                            icDeviceResource *resource,
                            char **value,
                            chip::Messaging::ExchangeManager &exchangeMgr,
                            const chip::SessionHandle &sessionHandle) override;

        bool DoWriteResource(std::forward_list<std::promise<bool>> &promises,
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

        SbmdDriver *driver = nullptr; // Non-owning. Owned by SbmdFactory.

        // Driver-based internal methods
        bool DoRegisterDriverResources(icDevice *device);
        void SeedInitialResourceValues(const std::string &deviceId);

        /**
         * Prerequisite check — evaluates prerequisites from registration data
         * against the device's data cache.
         */
        static bool CheckPrerequisites(const SbmdResource &resource, const MatterDevice &device);

        /**
         * Invoke a seed handler for a resource. Returns the seed value or empty string.
         */
        std::string InvokeSeedHandler(const std::string &deviceId,
                                        const std::string &endpointId,
                                        const SbmdResource &resource);

        /**
         * Find a resource by endpoint ID and resource ID.
         */
        const SbmdResource *FindDriverResource(const char *endpointId, const char *resourceId) const;

        /**
         * Handle a read/write/execute resource operation through the handler system.
         */
        void HandleResourceOp(std::forward_list<std::promise<bool>> &promises,
                                MatterDevice &device,
                                icDeviceResource *resource,
                                const char *input,
                                char **readValue,
                                char **executeResponse,
                                chip::Messaging::ExchangeManager &exchangeMgr,
                                const chip::SessionHandle &sessionHandle,
                                const char *opType);

        /**
         * Execute a result chain terminal — success, error, sendCommand, or writeAttribute.
         */
        void ExecuteTerminal(std::forward_list<std::promise<bool>> &promises,
                              MatterDevice &device,
                              const ResultTerminal &terminal,
                              const char *uri,
                              char **readValue,
                              char **executeResponse,
                              chip::Messaging::ExchangeManager &exchangeMgr,
                              const chip::SessionHandle &sessionHandle);

        /**
         * Handle a attribute report via the dispatch tables.
         * Called from MatterDevice::CacheCallback via the AttributeCallback.
         */
        void HandleAttributeReport(const std::string &deviceId,
                                     chip::EndpointId endpointId,
                                     chip::ClusterId clusterId,
                                     chip::AttributeId attributeId,
                                     chip::TLV::TLVReader &reader);

        uint8_t ConvertModesToBitmask(const std::vector<std::string> &modes);

        /** Map of device ID to set of resource keys (endpointId:resourceId) for optional resources that failed
         * configuration */
        std::map<std::string, std::set<std::string>> skippedOptionalResources;

        friend class TestableSpecBasedMatterDeviceDriver;
    };
} // namespace barton
