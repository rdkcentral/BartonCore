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
// Created by nkhan599 on 3/29/24.
//

#include "MatterCommon.h"
#include <iostream>

namespace zilker
{
    namespace NetworkUtils
    {
        chip::Optional<NetworkInterfaceInfo> ExtractOperationalInterfaceInfo(const TypeInfo::DecodableType &value,
                                                                             const char *nodeIpv6Addr)
        {
            auto interfaceIter = value.begin();
            while (interfaceIter.Next())
            {
                auto &interface = interfaceIter.GetValue();
                if (!interface.isOperational)
                {
                    continue;
                }

                size_t size = 0;
                if (interface.IPv6Addresses.ComputeSize(&size) != CHIP_NO_ERROR || size == 0)
                {
                    continue;
                }

                bool found = false;
                auto interfaceIpv6Iter = interface.IPv6Addresses.begin();
                while (interfaceIpv6Iter.Next())
                {
                    // Convert the current interface IPv6 address from binary to string format.
                    // This conversion is necessary because `nodeIpv6Addr` contains the IPv6 address in a shortened
                    // string form, while the addresses we're iterating over are in full binary format.
                    char interfaceIpv6Addr[INET6_ADDRSTRLEN] = {};
                    if (inet_ntop(AF_INET6, interfaceIpv6Iter.GetValue().data(), interfaceIpv6Addr, INET6_ADDRSTRLEN) ==
                        nullptr)
                    {
                        continue;
                    }

                    if (std::string(nodeIpv6Addr) == interfaceIpv6Addr)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    continue;
                }

                std::ostringstream hexStream;
                for (auto byte : interface.hardwareAddress)
                {
                    hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                }

                zilker::NetworkUtils::NetworkInterfaceInfo info;
                info.macAddress = hexStream.str();
                info.networkType = [&]() -> std::string {
                    switch (interface.type)
                    {
                    case chip::app::Clusters::GeneralDiagnostics::InterfaceTypeEnum::kWiFi:
                            return NETWORK_TYPE_WIFI;
                    case chip::app::Clusters::GeneralDiagnostics::InterfaceTypeEnum::kEthernet:
                            return NETWORK_TYPE_ETHERNET;
                    case chip::app::Clusters::GeneralDiagnostics::InterfaceTypeEnum::kCellular:
                            return NETWORK_TYPE_CELLULAR;
                    case chip::app::Clusters::GeneralDiagnostics::InterfaceTypeEnum::kThread:
                            return NETWORK_TYPE_THREAD;
                    case chip::app::Clusters::GeneralDiagnostics::InterfaceTypeEnum::kUnspecified:
                        default:
                            return "";
                    }
                }();

                return chip::MakeOptional<NetworkInterfaceInfo>(info);
            }

            return chip::NullOptional; // No matching interface found
        }
    } // namespace NetworkUtils
} // namespace zilker
