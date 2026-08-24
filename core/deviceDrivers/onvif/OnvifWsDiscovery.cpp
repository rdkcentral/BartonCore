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

#include "OnvifXml.h"

#include <arpa/inet.h>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace barton
{
    namespace onvif
    {

        namespace
        {

            const char *const NS_SOAP = "http://www.w3.org/2003/05/soap-envelope";
            const char *const NS_WSA = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
            const char *const NS_WSD = "http://schemas.xmlsoap.org/ws/2005/04/discovery";
            const char *const NS_ONVIF_NET = "http://www.onvif.org/ver10/network/wsdl";
            const char *const WSD_TO = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
            const char *const WSD_PROBE_ACTION = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";

            // Split whitespace-separated tokens (used for the XAddrs list).
            std::vector<std::string> SplitWhitespace(const std::string &in)
            {
                std::vector<std::string> tokens;
                size_t i = 0;

                while (i < in.size())
                {
                    while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i])))
                    {
                        i++;
                    }
                    size_t start = i;

                    while (i < in.size() && !std::isspace(static_cast<unsigned char>(in[i])))
                    {
                        i++;
                    }

                    if (i > start)
                    {
                        tokens.push_back(in.substr(start, i - start));
                    }
                }

                return tokens;
            }

        } // namespace

        std::string OnvifBuildProbeMessage(const std::string &messageId)
        {
            return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") + "<e:Envelope xmlns:e=\"" + NS_SOAP +
                   "\" xmlns:w=\"" + NS_WSA + "\" xmlns:d=\"" + NS_WSD + "\" xmlns:dn=\"" + NS_ONVIF_NET +
                   "\"><e:Header><w:MessageID>" + OnvifXmlEscape(messageId) +
                   "</w:MessageID><w:To e:mustUnderstand=\"true\">" + WSD_TO +
                   "</w:To><w:Action e:mustUnderstand=\"true\">" + WSD_PROBE_ACTION +
                   "</w:Action></e:Header><e:Body><d:Probe><d:Types>dn:NetworkVideoTransmitter</d:Types></d:Probe>"
                   "</e:Body></e:Envelope>";
        }

        std::vector<OnvifProbeMatch> OnvifParseProbeMatches(const std::string &xml)
        {
            std::vector<OnvifProbeMatch> matches;

            xmlDoc *doc = xmlReadMemory(xml.data(),
                                        static_cast<int>(xml.size()),
                                        nullptr,
                                        nullptr,
                                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER | XML_PARSE_NONET);

            if (doc == nullptr)
            {
                return matches;
            }

            std::vector<xmlNode *> matchNodes;
            OnvifXmlCollectByLocalName(xmlDocGetRootElement(doc), "ProbeMatch", matchNodes);

            for (xmlNode *matchNode : matchNodes)
            {
                OnvifProbeMatch match;

                // EndpointReference/Address carries the urn:uuid identity.
                xmlNode *epr = OnvifXmlFindFirstByLocalName(matchNode, "EndpointReference");

                if (epr != nullptr)
                {
                    match.endpointReference = OnvifXmlFindText(epr, "Address");
                }

                match.xaddrs = SplitWhitespace(OnvifXmlFindText(matchNode, "XAddrs"));
                match.scopes = OnvifXmlFindText(matchNode, "Scopes");
                match.types = OnvifXmlFindText(matchNode, "Types");

                if (!match.endpointReference.empty() || !match.xaddrs.empty())
                {
                    matches.push_back(std::move(match));
                }
            }

            xmlFreeDoc(doc);

            return matches;
        }

        std::string OnvifDeviceUuidFromEndpointReference(const std::string &endpointReference)
        {
            std::string ref = endpointReference;

            // Trim surrounding whitespace.
            size_t start = ref.find_first_not_of(" \t\r\n");
            size_t end = ref.find_last_not_of(" \t\r\n");

            if (start == std::string::npos)
            {
                return "";
            }
            ref = ref.substr(start, end - start + 1);

            const std::string prefix = "urn:uuid:";

            if (ref.size() >= prefix.size())
            {
                std::string head = ref.substr(0, prefix.size());
                std::string headLower;

                for (char c : head)
                {
                    headLower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }

                if (headLower == prefix)
                {
                    ref = ref.substr(prefix.size());
                }
            }

            std::string lower;

            for (char c : ref)
            {
                lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            return lower;
        }

        std::vector<OnvifProbeMatch> OnvifWsDiscovery::Probe(int timeoutMs, std::string *error)
        {
            std::vector<OnvifProbeMatch> results;

            int sock = socket(AF_INET, SOCK_DGRAM, 0);

            if (sock < 0)
            {
                if (error != nullptr)
                {
                    *error = std::string("failed to create socket: ") + std::strerror(errno);
                }

                return results;
            }

            // Bounded blocking receive so the gather loop cannot hang.
            struct timeval tv;
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;

            if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
            {
                if (error != nullptr)
                {
                    *error = std::string("failed to set receive timeout: ") + std::strerror(errno);
                }

                close(sock);

                return results;
            }

            // Best-effort: keep the probe on the local link. A failure here is non-fatal to
            // discovery (the OS default TTL is still usable), so the result is deliberately ignored.
            unsigned char ttl = 1;
            (void) setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

            if (destPort < 1 || destPort > 65535)
            {
                if (error != nullptr)
                {
                    *error = "invalid discovery destination port: " + std::to_string(destPort);
                }

                close(sock);

                return results;
            }

            struct sockaddr_in dest;
            std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(static_cast<uint16_t>(destPort));

            if (inet_pton(AF_INET, destAddress.c_str(), &dest.sin_addr) != 1)
            {
                if (error != nullptr)
                {
                    *error = "invalid discovery destination address: " + destAddress;
                }

                close(sock);

                return results;
            }

            // A per-probe message id; format is not significant to the camera beyond uniqueness.
            // WS-Addressing MessageIDs must be unique; a per-process atomic counter makes repeated
            // probes (even with the same timeout) distinct so devices/proxies do not de-duplicate them.
            static std::atomic<uint64_t> probeSequence {0};
            uint64_t sequence = probeSequence.fetch_add(1, std::memory_order_relaxed);
            std::string messageId = std::string("uuid:") + std::to_string(getpid()) + "-" + std::to_string(timeoutMs) +
                                    "-" + std::to_string(sequence);
            std::string probe = OnvifBuildProbeMessage(messageId);

            ssize_t sent =
                sendto(sock, probe.data(), probe.size(), 0, reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest));

            if (sent < 0)
            {
                if (error != nullptr)
                {
                    *error = std::string("failed to send probe: ") + std::strerror(errno);
                }

                close(sock);

                return results;
            }

            // Gather responses until the receive timeout elapses, bounding the total so a noisy or
            // hostile LAN cannot grow results without limit.
            static const size_t maxMatches = 256;
            char buffer[8192];

            while (results.size() < maxMatches)
            {
                ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);

                if (received < 0)
                {
                    if (errno == EINTR)
                    {
                        continue; // interrupted by a signal; keep gathering rather than ending early
                    }

                    // SO_RCVTIMEO expiry (EAGAIN/EWOULDBLOCK) is the normal end of the gather window;
                    // anything else is a real error worth surfacing (we still return what we gathered).
                    if (errno != EAGAIN && errno != EWOULDBLOCK && error != nullptr)
                    {
                        *error = std::string("failed to receive probe response: ") + std::strerror(errno);
                    }

                    break;
                }

                if (received == 0)
                {
                    break;
                }
                buffer[received] = '\0';

                std::vector<OnvifProbeMatch> parsed =
                    OnvifParseProbeMatches(std::string(buffer, static_cast<size_t>(received)));

                for (OnvifProbeMatch &match : parsed)
                {
                    if (results.size() >= maxMatches)
                    {
                        break;
                    }

                    results.push_back(std::move(match));
                }
            }

            close(sock);

            return results;
        }

    } // namespace onvif
} // namespace barton
