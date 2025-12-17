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

#define MATTER_LINK_QUALITY_GOOD_THRESHOLD   46
#define MATTER_LINK_QUALITY_FAIR_THRESHOLD   30

namespace barton
{
    namespace Subsystem
    {
        namespace Matter
        {
            uint8_t calculateLinkScore(int8_t rssi)
            {
                static double DBM_FLOOR = -90.0;
                static double DBM_CEIL = -20.0;

                // Clamp rssi to reasonable boundaries
                rssi = (rssi < DBM_FLOOR) ? DBM_FLOOR : (rssi > DBM_CEIL) ? DBM_CEIL : rssi;

                // This will be a number between 0 and 1 where closer to 0 is close to the ceiling (good) and closer to 1 is close
                // to the floor (bad).
                double normalizedRSSI = (DBM_CEIL - rssi) / (DBM_CEIL - DBM_FLOOR);

                // A normalizedRSSI close to 0 (good) should cause the second subtraction operand to be small, leading to a
                // percentage close to 100. A normalizedRSSI close to 1 (bad) should cause the second subtraction operand being
                // close to 100, leading to a percentage close to 0.
                uint8_t percentage = 100 - (100 * normalizedRSSI);

                return percentage;
            }

            /*
             * Caller frees result
             */
            char *determineLinkQuality(uint8_t linkScore)
            {
                if (linkScore >= MATTER_LINK_QUALITY_GOOD_THRESHOLD)
                {
                    return strdup(LINK_QUALITY_GOOD);
                }
                else if (linkScore >= MATTER_LINK_QUALITY_FAIR_THRESHOLD)
                {
                    return strdup(LINK_QUALITY_FAIR);
                }
                else
                {
                    return strdup(LINK_QUALITY_POOR);
                }
            }
        } // namespace Matter
    } // namespace Subsystem
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

                barton::NetworkUtils::NetworkInterfaceInfo info;
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
} // namespace barton
