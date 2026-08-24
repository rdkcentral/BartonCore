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
// A minimal ONVIF WS-Discovery client: it multicasts a Probe for NetworkVideoTransmitter devices
// and collects ProbeMatch responses. The message construction and response parsing are separated
// from the socket I/O so they can be unit tested against canned payloads.
//

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace barton
{
    namespace onvif
    {

        struct OnvifProbeMatch
        {
            std::string endpointReference;   // e.g. "urn:uuid:<uuid>"
            std::vector<std::string> xaddrs; // ONVIF service URLs advertised by the device
            std::string scopes;
            std::string types;
        };

        // Build a WS-Discovery Probe SOAP message with the given (already-formatted) message id. Testable.
        std::string OnvifBuildProbeMessage(const std::string &messageId);

        // Parse a WS-Discovery ProbeMatches SOAP response into zero or more matches. Testable.
        std::vector<OnvifProbeMatch> OnvifParseProbeMatches(const std::string &xml);

        // Derive a stable Barton device uuid from a ProbeMatch endpoint reference. Strips a leading
        // "urn:uuid:" (case-insensitive) and lower-cases the result; falls back to the trimmed input.
        std::string OnvifDeviceUuidFromEndpointReference(const std::string &endpointReference);

        class OnvifWsDiscovery
        {
        public:
            OnvifWsDiscovery() = default;

            // Override the multicast destination (defaults to 239.255.255.250:3702). Primarily a test seam
            // to target a unicast mock when multicast is unavailable in the environment.
            void SetDestination(std::string address, int port)
            {
                destAddress = std::move(address);
                destPort = port;
            }

            // Send a Probe and gather ProbeMatch responses for up to timeoutMs. Returns the parsed matches;
            // sets error (if provided) only on a hard socket failure (an empty result is not an error).
            std::vector<OnvifProbeMatch> Probe(int timeoutMs, std::string *error = nullptr);

        private:
            std::string destAddress = "239.255.255.250";
            int destPort = 3702;
        };

    } // namespace onvif
} // namespace barton
