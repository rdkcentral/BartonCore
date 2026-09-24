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

#include "OnvifSoapClient.h"

#include <gtest/gtest.h>

using namespace barton::onvif;

TEST(OnvifSoapClient, PasswordDigestMatchesKnownVector)
{
    // Base64(SHA1("0123456789abcdef" + "2026-08-10T00:00:00Z" + "testpass"))
    std::string digest = OnvifComputePasswordDigest("0123456789abcdef", "2026-08-10T00:00:00Z", "testpass");
    EXPECT_EQ(digest, "BydukU/0C6mM9gS4egWZVNu1Sao=");
}

TEST(OnvifSoapClient, PasswordDigestIsDeterministicAndDiffersByInput)
{
    std::string a = OnvifComputePasswordDigest("nonce", "created", "pw");
    std::string b = OnvifComputePasswordDigest("nonce", "created", "pw");
    std::string c = OnvifComputePasswordDigest("nonce", "created", "different");

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(OnvifSoapClient, EnvelopeOmitsSecurityHeaderWithoutCredentials)
{
    std::string env = OnvifSoapClient::BuildEnvelope("<x/>", OnvifCredentials {});

    EXPECT_EQ(env.find("UsernameToken"), std::string::npos);
    EXPECT_NE(env.find("<s:Body><x/></s:Body>"), std::string::npos);
}

TEST(OnvifSoapClient, EnvelopeIncludesUsernameTokenWithCredentials)
{
    OnvifCredentials creds {"admin", "secret"};
    std::string env = OnvifSoapClient::BuildEnvelope("<x/>", creds);

    EXPECT_NE(env.find("<wsse:UsernameToken>"), std::string::npos);
    EXPECT_NE(env.find("<wsse:Username>admin</wsse:Username>"), std::string::npos);
    EXPECT_NE(env.find("PasswordDigest"), std::string::npos);
    EXPECT_NE(env.find("<wsse:Nonce"), std::string::npos);
    EXPECT_NE(env.find("<wsu:Created>"), std::string::npos);
    // The cleartext password must never appear in the envelope.
    EXPECT_EQ(env.find("secret"), std::string::npos);
}

TEST(OnvifSoapClient, ParsesDeviceInformation)
{
    std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                      "<s:Body><tds:GetDeviceInformationResponse xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
                      "<tds:Manufacturer>ACME</tds:Manufacturer><tds:Model>Cam9000</tds:Model>"
                      "<tds:FirmwareVersion>1.2.3</tds:FirmwareVersion><tds:SerialNumber>SN42</tds:SerialNumber>"
                      "<tds:HardwareId>HW1</tds:HardwareId></tds:GetDeviceInformationResponse></s:Body></s:Envelope>";

    OnvifDeviceInfo info;
    ASSERT_TRUE(OnvifParseDeviceInformation(xml, info));
    EXPECT_EQ(info.manufacturer, "ACME");
    EXPECT_EQ(info.model, "Cam9000");
    EXPECT_EQ(info.firmwareVersion, "1.2.3");
    EXPECT_EQ(info.serialNumber, "SN42");
}

TEST(OnvifSoapClient, RejectsDeviceInformationWithoutExpectedFields)
{
    // A SOAP Fault returned with HTTP 200 parses as valid XML but carries none of the device fields.
    std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                      "<s:Body><s:Fault><s:Code><s:Value>s:Sender</s:Value></s:Code>"
                      "<s:Reason><s:Text>not authorized</s:Text></s:Reason></s:Fault></s:Body></s:Envelope>";

    OnvifDeviceInfo info;
    EXPECT_FALSE(OnvifParseDeviceInformation(xml, info));
}

TEST(OnvifSoapClient, ParsesProfileTokens)
{
    std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                      "<s:Body><trt:GetProfilesResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
                      "<trt:Profiles token=\"MainStream\"/><trt:Profiles token=\"SubStream\"/>"
                      "</trt:GetProfilesResponse></s:Body></s:Envelope>";

    std::vector<std::string> tokens;
    ASSERT_TRUE(OnvifParseProfiles(xml, tokens));
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], "MainStream");
    EXPECT_EQ(tokens[1], "SubStream");
}

TEST(OnvifSoapClient, ParsesCredentialFreeStreamUri)
{
    std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                      "<s:Body><trt:GetStreamUriResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                      "xmlns:tt=\"http://www.onvif.org/ver10/schema\"><trt:MediaUri>"
                      "<tt:Uri>rtsp://192.168.1.50:554/stream1</tt:Uri></trt:MediaUri>"
                      "</trt:GetStreamUriResponse></s:Body></s:Envelope>";

    std::string uri = OnvifParseMediaUri(xml);
    EXPECT_EQ(uri, "rtsp://192.168.1.50:554/stream1");
    // The URI must not carry embedded credentials.
    EXPECT_EQ(uri.find('@'), std::string::npos);
}

TEST(OnvifSoapClient, StripsEmbeddedCredentialsFromStreamUri)
{
    std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                      "<s:Body><trt:GetStreamUriResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                      "xmlns:tt=\"http://www.onvif.org/ver10/schema\"><trt:MediaUri>"
                      "<tt:Uri>rtsp://admin:secret@192.168.1.50:554/stream1</tt:Uri></trt:MediaUri>"
                      "</trt:GetStreamUriResponse></s:Body></s:Envelope>";

    std::string uri = OnvifParseMediaUri(xml);
    // Embedded userinfo must be stripped so credentials are not leaked to callers/logs.
    EXPECT_EQ(uri, "rtsp://192.168.1.50:554/stream1");
    EXPECT_EQ(uri.find('@'), std::string::npos);
    EXPECT_EQ(uri.find("secret"), std::string::npos);
}

TEST(OnvifSoapClient, ParsesSnapshotUri)
{
    std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                      "<s:Body><trt:GetSnapshotUriResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                      "xmlns:tt=\"http://www.onvif.org/ver10/schema\"><trt:MediaUri>"
                      "<tt:Uri>http://192.168.1.50/snapshot.jpg</tt:Uri></trt:MediaUri>"
                      "</trt:GetSnapshotUriResponse></s:Body></s:Envelope>";

    EXPECT_EQ(OnvifParseMediaUri(xml), "http://192.168.1.50/snapshot.jpg");
}

TEST(OnvifSoapClient, ParseProfilesFailsWhenNoneReturned)
{
    std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                      "<s:Body><trt:GetProfilesResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\"/>"
                      "</s:Body></s:Envelope>";

    std::vector<std::string> tokens;
    EXPECT_FALSE(OnvifParseProfiles(xml, tokens));
    EXPECT_TRUE(tokens.empty());
}
