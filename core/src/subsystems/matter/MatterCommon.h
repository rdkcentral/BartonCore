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
// Created by tlea200 on 11/16/21.
//

#ifndef ZILKER_MATTERCOMMON_H
#define ZILKER_MATTERCOMMON_H

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

namespace zilker
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
} // namespace zilker

#endif // ZILKER_MATTERCOMMON_H
