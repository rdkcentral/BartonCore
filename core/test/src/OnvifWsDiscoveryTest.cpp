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

#include "OnvifWsDiscovery.h"

#include <gtest/gtest.h>

using namespace barton::onvif;

TEST(OnvifWsDiscovery, ProbeMessageContainsRequiredElements)
{
    std::string probe = OnvifBuildProbeMessage("uuid:abc-123");

    EXPECT_NE(probe.find("<w:MessageID>uuid:abc-123</w:MessageID>"), std::string::npos);
    EXPECT_NE(probe.find("Probe"), std::string::npos);
    EXPECT_NE(probe.find("NetworkVideoTransmitter"), std::string::npos);
}

TEST(OnvifWsDiscovery, ParsesProbeMatch)
{
    std::string xml = "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\" "
                      "xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
                      "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><e:Body>"
                      "<d:ProbeMatches><d:ProbeMatch>"
                      "<w:EndpointReference><w:Address>urn:uuid:aabbccdd-1122-3344-5566-778899aabbcc</w:Address>"
                      "</w:EndpointReference>"
                      "<d:Types>dn:NetworkVideoTransmitter</d:Types>"
                      "<d:Scopes>onvif://www.onvif.org/name/ACME</d:Scopes>"
                      "<d:XAddrs>http://192.168.1.50/onvif/device_service</d:XAddrs>"
                      "</d:ProbeMatch></d:ProbeMatches></e:Body></e:Envelope>";

    std::vector<OnvifProbeMatch> matches = OnvifParseProbeMatches(xml);
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].endpointReference, "urn:uuid:aabbccdd-1122-3344-5566-778899aabbcc");
    ASSERT_EQ(matches[0].xaddrs.size(), 1u);
    EXPECT_EQ(matches[0].xaddrs[0], "http://192.168.1.50/onvif/device_service");
}

TEST(OnvifWsDiscovery, ParsesMultipleXAddrs)
{
    std::string xml =
        "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><e:Body>"
        "<d:ProbeMatches><d:ProbeMatch>"
        "<d:XAddrs>http://192.168.1.50/onvif/device_service http://10.0.0.5/onvif/device_service</d:XAddrs>"
        "</d:ProbeMatch></d:ProbeMatches></e:Body></e:Envelope>";

    std::vector<OnvifProbeMatch> matches = OnvifParseProbeMatches(xml);
    ASSERT_EQ(matches.size(), 1u);
    ASSERT_EQ(matches[0].xaddrs.size(), 2u);
    EXPECT_EQ(matches[0].xaddrs[1], "http://10.0.0.5/onvif/device_service");
}

TEST(OnvifWsDiscovery, DeviceUuidStripsUrnPrefixAndLowercases)
{
    EXPECT_EQ(OnvifDeviceUuidFromEndpointReference("urn:uuid:AABBCCDD-1122-3344-5566-778899AABBCC"),
              "aabbccdd-1122-3344-5566-778899aabbcc");
    EXPECT_EQ(OnvifDeviceUuidFromEndpointReference("URN:UUID:abc"), "abc");
    // Falls back to the trimmed input when there is no urn:uuid prefix.
    EXPECT_EQ(OnvifDeviceUuidFromEndpointReference("  plainId  "), "plainid");
}

TEST(OnvifWsDiscovery, EmptyResponseYieldsNoMatches)
{
    EXPECT_TRUE(OnvifParseProbeMatches("").empty());
    EXPECT_TRUE(OnvifParseProbeMatches("<not-soap/>").empty());
}
