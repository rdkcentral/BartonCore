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

#include "lib/core/TLVWriter.h"
#include "subsystems/matter/MatterCommon.h"
#include <app/data-model/Decode.h>
#include <app/data-model/Encode.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <inet/InetInterface.h>
#include <lib/core/TLV.h>
#include <optional>

using namespace chip;
using namespace chip::app::DataModel;
using namespace chip::TLV;
using namespace chip::app::Clusters::GeneralDiagnostics::Structs;
using namespace chip::app::Clusters::GeneralDiagnostics;
using namespace barton::NetworkUtils;
using namespace chip::app::Clusters::GeneralDiagnostics::Structs::NetworkInterface;

namespace
{
#define ARRAY_SIZE 2048

    using NetworkInterfaceType = chip::app::Clusters::GeneralDiagnostics::Structs::NetworkInterface::DecodableType;
    using DecodableType = chip::app::DataModel::DecodableList<NetworkInterfaceType>;
    class ExtractOperationalInterfaceInfoTest : public ::testing::Test
    {
    protected:
        Type CreateInterface(const char *name,
                             bool isOperational,
                             const uint8_t *macAddress,
                             size_t macLen,
                             InterfaceTypeEnum type,
                             const std::vector<ByteSpan> &ipv6Addresses)
        {
            Type iface;
            iface.name = CharSpan(name, strlen(name));
            iface.offPremiseServicesReachableIPv4.SetNull();
            iface.offPremiseServicesReachableIPv6.SetNull();
            iface.isOperational = isOperational;
            iface.hardwareAddress = ByteSpan(macAddress, macLen);
            iface.type = type;
            iface.IPv4Addresses = app::DataModel::List<const ByteSpan>(ipv6Addresses.data(), ipv6Addresses.size());
            iface.IPv6Addresses = app::DataModel::List<const ByteSpan>(ipv6Addresses.data(), ipv6Addresses.size());
            return iface;
        }

        CHIP_ERROR GetDecodableList(const Type &iface, DecodableType &list, uint8_t *decodableListBuffer)
        {
            uint8_t interfaceData[ARRAY_SIZE];
            TLVWriter interfaceWriter;
            interfaceWriter.Init(interfaceData, ARRAY_SIZE);

            CHIP_ERROR err = iface.Encode(interfaceWriter, AnonymousTag());
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            err = interfaceWriter.Finalize();
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            TLVReader reader;
            reader.Init(interfaceData, interfaceWriter.GetLengthWritten());

            err = reader.Next(TLV::kTLVType_Structure, AnonymousTag());
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            TLVWriter decodableWriter;
            decodableWriter.Init(decodableListBuffer, ARRAY_SIZE);

            TLVType outerContainerType;
            err = decodableWriter.StartContainer(AnonymousTag(), kTLVType_Array, outerContainerType);
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            err = decodableWriter.CopyContainer(reader);
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            err = decodableWriter.EndContainer(outerContainerType);
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            err = decodableWriter.Finalize();
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            TLVReader listReader;
            listReader.Init(decodableListBuffer, decodableWriter.GetLengthWritten());
            err = listReader.Next(TLV::kTLVType_Array, AnonymousTag());
            if (err != CHIP_NO_ERROR)
            {
                return err;
            }

            err = list.Decode(listReader);
            return err;
        }
    };

    TEST_F(ExtractOperationalInterfaceInfoTest, NoOperationalInterface)
    {
        uint8_t ipv6Addr1[16] = {
            0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
        std::vector<ByteSpan> ipv6Addresses = {ByteSpan(ipv6Addr1, sizeof(ipv6Addr1))};
        Type iface = CreateInterface("eth0",
                                     false,
                                     (const uint8_t *) "\x00\x1A\x2B\x3C\x4D\x5E",
                                     6,
                                     InterfaceTypeEnum::kEthernet,
                                     ipv6Addresses);

        uint8_t decodableListBuffer[ARRAY_SIZE];
        DecodableType list;
        ASSERT_EQ(GetDecodableList(iface, list, decodableListBuffer), CHIP_NO_ERROR);

        auto result = ExtractOperationalInterfaceInfo(list, "2001:db8::1");

        ASSERT_FALSE(result.HasValue());
    }

    TEST_F(ExtractOperationalInterfaceInfoTest, NoIPv6Addresses)
    {
        Type iface = CreateInterface(
            "eth0", true, (const uint8_t *) "\x00\x1A\x2B\x3C\x4D\x5E", 6, InterfaceTypeEnum::kEthernet, {});

        uint8_t decodableListBuffer[ARRAY_SIZE];
        DecodableType list;
        ASSERT_EQ(GetDecodableList(iface, list, decodableListBuffer), CHIP_NO_ERROR);
        auto result = ExtractOperationalInterfaceInfo(list, "2001:db8::1");

        ASSERT_FALSE(result.HasValue());
    }

    TEST_F(ExtractOperationalInterfaceInfoTest, MatchingIPv6Address)
    {
        uint8_t ipv6Addr1[16] = {
            0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
        std::vector<ByteSpan> ipv6Addresses = {ByteSpan(ipv6Addr1, sizeof(ipv6Addr1))};
        Type iface = CreateInterface(
            "eth0", true, (const uint8_t *) "\x00\x1A\x2B\x3C\x4D\x5E", 6, InterfaceTypeEnum::kEthernet, ipv6Addresses);

        uint8_t decodableListBuffer[ARRAY_SIZE];
        DecodableType list;
        ASSERT_EQ(GetDecodableList(iface, list, decodableListBuffer), CHIP_NO_ERROR);
        auto result = ExtractOperationalInterfaceInfo(list, "2001:db8::1");

        ASSERT_TRUE(result.HasValue());
        EXPECT_EQ(result.Value().macAddress, "001a2b3c4d5e");
        EXPECT_EQ(result.Value().networkType, NETWORK_TYPE_ETHERNET);
    }
} // namespace
