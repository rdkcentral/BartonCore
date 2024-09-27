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
// Created by nkhan599 on 3/29/24.
//

#include "MatterCommon.h"
#include <iostream>

namespace zilker
{
namespace NetworkUtils
{
    chip::Optional <NetworkInterfaceInfo>
    ExtractOperationalInterfaceInfo(const TypeInfo::DecodableType &value,
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
                // This conversion is necessary because `nodeIpv6Addr` contains the IPv6 address in a shortened string form,
                // while the addresses we're iterating over are in full binary format.
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
            for (auto byte: interface.hardwareAddress)
            {
                hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            }

            zilker::NetworkUtils::NetworkInterfaceInfo info;
            info.macAddress = hexStream.str();
            info.networkType = [&]() -> std::string {
                switch (interface.type)
                {
                    case EMBER_ZCL_INTERFACE_TYPE_ENUM_WI_FI:
                        return NETWORK_TYPE_WIFI;
                    case EMBER_ZCL_INTERFACE_TYPE_ENUM_ETHERNET:
                        return NETWORK_TYPE_ETHERNET;
                    case EMBER_ZCL_INTERFACE_TYPE_ENUM_CELLULAR:
                        return NETWORK_TYPE_CELLULAR;
                    case EMBER_ZCL_INTERFACE_TYPE_ENUM_THREAD:
                        return NETWORK_TYPE_THREAD;
                    case EMBER_ZCL_INTERFACE_TYPE_ENUM_UNSPECIFIED:
                    default:
                        return "";
                }
            }();

            return chip::MakeOptional<NetworkInterfaceInfo>(info);
        }

        return chip::NullOptional; // No matching interface found
    }
}// end NetworkUtils namespace
} //end zilker namespace