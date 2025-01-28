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
// Created by tlea200 on 11/16/21.
//

#pragma once

#ifndef LOG_TAG
#define LOG_TAG "Matter"
#endif

#include "app-common/zap-generated/cluster-objects.h"
#include "transport/SecureSession.h"
#include <arpa/inet.h>
#include <cinttypes>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <lib/core/NodeId.h>
#include <lib/core/Optional.h>
#include <sstream>
#include <string>

extern "C" {
#include <commonDeviceDefs.h>
#include <icUtil/stringUtils.h>
}

namespace barton
{
    namespace Subsystem
    {
        namespace Matter
        {
            /**
             * Convert a Matter NodeId value to a subsystem uuid
             * @param
             * @return an allocated string that represents the nodeId
             */
            inline std::string NodeIdToUuid(chip::NodeId nodeId)
            {
                std::ostringstream tmp;

                tmp << std::setfill('0') << std::setw(sizeof(chip::NodeId) * 2) << std::hex << nodeId;
                return tmp.str();
            }

            /**
             * Convert a subsystem uuid to a Matter NodeId
             * @param deviceUuid
             * @return a valid nodeId, or chip::kUndefinedNodeId on conversion error
             */
            inline chip::NodeId UuidToNodeId(const std::string &deviceUuid)
            {
                chip::NodeId nodeId;

                if (!deviceUuid.empty() &&
                    stringToUnsignedNumberWithinRange(deviceUuid.c_str(), &nodeId, 16, 0, UINT64_MAX))
                {
                    return nodeId;
                }

                return chip::kUndefinedNodeId;
            }
        } // namespace Matter
    } // namespace Subsystem

    namespace NetworkUtils
    {
        using TypeInfo = chip::app::Clusters::GeneralDiagnostics::Attributes::NetworkInterfaces::TypeInfo;
        struct NetworkInterfaceInfo
        {
            std::string macAddress;
            std::string networkType;
        };

        /**
         * Extracts network interface information for the operational interface.
         *
         * This function searches for the operational network interface and extracts its
         * MAC address and network type. It returns an chip::optional containing the
         * `NetworkInterfaceInfo` if the operational interface is found and contains the
         * specified IPv6 address. If no operational interface is found, or if the
         * operational interface does not contain the specified IPv6 address, it returns
         * `chip::NullOptional`.
         * If the network type does not match any of the defined interface types, the
         * network type in the `NetworkInterfaceInfo` will be an empty string.
         *
         * @param value A reference to the NetworkInterfaces::DecodableType containing network interface details.
         * @param nodeIpv6Addr A C string representing the IPv6 address of the node.
         * @return chip::Optional<NetworkInterfaceInfo>
         *         - An optional containing the `NetworkInterfaceInfo` if the operational
         *           interface is found and contains the specified IPv6 address.
         *         - `chip::NullOptional` if no operational interface is found or if the
         *           operational interface does not contain the specified IPv6 address.
         */
        chip::Optional<NetworkInterfaceInfo> ExtractOperationalInterfaceInfo(const TypeInfo::DecodableType &value,
                                                                             const char *nodeIpv6Addr);
    } // namespace NetworkUtils
} // namespace barton
