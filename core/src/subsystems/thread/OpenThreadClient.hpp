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
 * Created by Christian Leithner on 2/8/24.
 */

#pragma once

#include "otbr/dbus/common/types.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <otbr/dbus/client/thread_api_dbus.hpp>
#include <stdbool.h>

namespace barton
{
    class OpenThreadClient
    {
    public:
        OpenThreadClient(const std::function<void(void)> &configChangedCallback) :
            ready(false), configChangedCallback(configChangedCallback) { Connect(); }

        enum DeviceRole
        {
            DEVICE_ROLE_UNKNOWN,
            DEVICE_ROLE_DISABLED,
            DEVICE_ROLE_DETACHED,
            DEVICE_ROLE_CHILD,
            DEVICE_ROLE_ROUTER,
            DEVICE_ROLE_LEADER
        };

        /**
         * @brief Command OTBR to create a new thread network.
         *
         * @note This is a potentially long-running, synchronous operation
         *
         * @param[in] networkName - The name of the network the caller wishes to create. Must be 16 characters or less.
         * @param[in] desiredChannel - The channel to create the network on. Must be in the interval [11-26], or any
         * other value to allow OTBR to choose.
         * @return The created network data tlvs or empty on error
         */
        std::vector<uint8_t> CreateNetwork(const std::string &networkName, uint8_t desiredChannel);

        /**
         * @brief Command OTBR to create an active network from the provided dataset tlvs.
         *
         * @param[in] networkDataTlvs - The network tlvs to restore when this method returns true
         * @return true - On successful network restoration
         * @return false - When an error occurs
         */
        bool RestoreNetwork(const std::vector<uint8_t> &networkDataTlvs);

        /**
         * @brief Get the active dataset tlvs.
         *
         * @return The active network dataset tlvs or empty on error
         */
        std::vector<uint8_t> GetActiveDatasetTlvs();

        /**
         * @brief Get the pending dataset tlvs.
         *
         * @return The pending network dataset tlvs or empty on error
         */
        std::vector<uint8_t> GetPendingDatasetTlvs();

        /**
         * @brief Get the current channel
         *
         * @return The current channel of the interval [11-26] or UINT16_MAX on error
         */
        uint16_t GetChannel();

        /**
         * @brief Get the current pan id.
         *
         * @return The current pan id or UINT16_MAX on error
         */
        uint16_t GetPanId();

        /**
         * @brief Get the extended pan id.
         *
         * @return The current extended pan id or UINT64_MAX on error
         */
        uint64_t GetExtPanId();

        /**
         * @brief Get the current active network key.
         *
         * @return The current network key or empty on error
         */
        std::vector<uint8_t> GetNetworkKey();

        /**
         * @brief Get the current active network name
         *
         * @return The current network name or empty string on error
         */
        std::string GetNetworkName();

        /**
         * @brief Get the current device role in the active network
         *
         * @return The current device role or DEVICE_ROLE_UNKNOWN on error
         */
        DeviceRole GetDeviceRole();

        /**
         * @brief Get the border router's agent id.
         *
         * @return The border router agent id or empty on error
         */
        std::vector<uint8_t> GetBorderAgentId();

        /**
         * @brief Get the thread stack protocol version.
         *
         * @return The thread stack protocol version or 0 on error
         */
        uint16_t GetThreadVersion();

        /**
         * Get the Thread network interface up/down status.
         *
         * @return true if the interface is up, false otherwise
         */
        bool IsThreadInterfaceUp();

        /**
         * @brief Check if NAT64 is enabled in the active network
         *
         * @return true - NAT64 is enabled
         * @return false - NAT64 is disabled
         */
        bool IsNat64Enabled();

        /**
         * @brief Enable or disable NAT64 in the active network
         *
         * @param enable - true to enable NAT64, false to disable
         * @return true - On success
         * @return false - On error
         */
        bool SetNat64Enabled(bool enable);

        /**
         * @brief Activate ephemeral key mode.
         *
         * @param[out] ephemeralKey - The generated ephemeral key.
         * @return true on success, false on failure.
         */
        bool ActivateEphemeralKeyMode(std::string &ephemeralKey);

    private:
        std::mutex dbusConnectionGuard;
        std::unique_ptr<DBusConnection, std::function<void(DBusConnection *)>> dbusConnection;
        std::unique_ptr<otbr::DBus::ThreadApiDBus> threadApiBus;
        std::atomic_bool ready;
        std::function<void(void)>  configChangedCallback;

        void Connect();

        static void ReleaseDBusConnection(DBusConnection *connection);
        static DeviceRole TranslateOTBRDeviceRole(otbr::DBus::DeviceRole role);
        static DBusHandlerResult
        DBusMessageFilterRedirector(DBusConnection *aConnection, DBusMessage *aMessage, void *aData);
        DBusHandlerResult DBusMessageFilter(DBusMessage *aMessage);
    };
} // namespace barton
