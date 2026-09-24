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

//
// A small, self-contained ONVIF SOAP client. It has no dependency on the Barton
// DeviceDriver framework so it can be unit tested in isolation against canned
// responses or a mock ONVIF device. It issues the handful of ONVIF operations the
// camera driver needs (device information, media profiles, stream/snapshot URIs)
// over HTTP via libcurl and parses the SOAP responses with libxml2.
//
// AUTH NOTE: the credential model here is intentionally primitive/interim (a single
// static username/password per request; WS-UsernameToken digest auth). It is expected
// to be reworked once Barton gains a configuration-driven credential model. See the
// ONVIF driver module documentation and the change design (D5a) for details.
//

#pragma once

#include <string>
#include <vector>

namespace barton
{
    namespace onvif
    {

        struct OnvifCredentials
        {
            std::string username;
            std::string password;
        };

        struct OnvifDeviceInfo
        {
            std::string manufacturer;
            std::string model;
            std::string firmwareVersion;
            std::string serialNumber;
            std::string hardwareId;
        };

        // Compute the WS-Security UsernameToken password digest: Base64(SHA1(nonce + created + password)),
        // where nonceRaw is the raw (un-encoded) nonce bytes. Exposed for unit testing.
        std::string OnvifComputePasswordDigest(const std::string &nonceRaw,
                                               const std::string &created,
                                               const std::string &password);

        // Response parsers, exposed for unit testing against canned fixtures.
        bool OnvifParseDeviceInformation(const std::string &xml, OnvifDeviceInfo &out);
        bool OnvifParseProfiles(const std::string &xml, std::vector<std::string> &tokensOut);
        // Extract the media URI ("Uri") from a GetStreamUri or GetSnapshotUri response ("" if absent).
        std::string OnvifParseMediaUri(const std::string &xml);

        class OnvifSoapClient
        {
        public:
            // serviceUrl is the absolute ONVIF service endpoint (e.g. "http://<ip>/onvif/device_service").
            explicit OnvifSoapClient(std::string serviceUrl);

            // Optional override of the network timeout (seconds) for each request.
            void SetTimeoutSeconds(long seconds) { timeoutSeconds = seconds; }

            // Anonymous operation — most cameras allow GetDeviceInformation without auth, but a UsernameToken
            // is included when credentials are non-empty for cameras that require it.
            bool
            GetDeviceInformation(const OnvifCredentials &creds, OnvifDeviceInfo &out, std::string *error = nullptr);

            // Returns the media profile tokens advertised by the camera.
            bool GetProfiles(const OnvifCredentials &creds,
                             std::vector<std::string> &profileTokensOut,
                             std::string *error = nullptr);

            // Returns the (credential-free) RTSP stream URI for the given media profile.
            bool GetStreamUri(const OnvifCredentials &creds,
                              const std::string &profileToken,
                              std::string &rtspUriOut,
                              std::string *error = nullptr);

            // Returns the (credential-free) HTTP JPEG snapshot URI for the given media profile.
            bool GetSnapshotUri(const OnvifCredentials &creds,
                                const std::string &profileToken,
                                std::string &jpegUriOut,
                                std::string *error = nullptr);

            // Build a SOAP envelope for a body, injecting a WS-Security UsernameToken when creds are set.
            // Exposed for unit testing of the security header.
            static std::string BuildEnvelope(const std::string &bodyXml, const OnvifCredentials &creds);

        private:
            // POST the envelope to serviceUrl and return the raw response body. Returns false on transport error.
            bool Post(const std::string &envelope, std::string &responseOut, std::string *error);

            std::string serviceUrl;
            long timeoutSeconds = 10;
        };

    } // namespace onvif
} // namespace barton
