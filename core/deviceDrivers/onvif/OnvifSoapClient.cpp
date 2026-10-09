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

#include "OnvifXml.h"

#include <curl/curl.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/random.h>

#include <cerrno>
#include <cstring>
#include <mutex>
#include <utility>

namespace barton
{
    namespace onvif
    {

        namespace
        {

            // XML namespace (NS_*) URIs used to build SOAP envelopes and WS-Security headers.
            const char *const NS_SOAP = "http://www.w3.org/2003/05/soap-envelope";
            const char *const NS_TDS = "http://www.onvif.org/ver10/device/wsdl";
            const char *const NS_TRT = "http://www.onvif.org/ver10/media/wsdl";
            const char *const NS_TT = "http://www.onvif.org/ver10/schema";
            const char *const NS_WSSE =
                "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd";
            const char *const NS_WSU =
                "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd";
            const char *const PASSWORD_DIGEST_TYPE =
                "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest";
            const char *const BASE64_ENCODING_TYPE =
                "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary";

            // Cap the accumulated response so a reachable or hostile camera cannot stream an
            // unbounded (e.g. chunked) body and drive the process out of memory. ONVIF SOAP
            // responses are small; 8 MiB is far above any legitimate reply.
            constexpr size_t MAX_RESPONSE_BYTES = 8 * 1024 * 1024;

            size_t WriteCb(char *ptr, size_t size, size_t nmemb, void *userdata)
            {
                std::string *body = static_cast<std::string *>(userdata);
                size_t total = size * nmemb;

                // Returning a short count aborts the transfer (CURLE_WRITE_ERROR) rather than
                // appending past the limit. Guard the subtraction so a single oversized chunk cannot
                // underflow the size_t and slip past the cap.
                if (total > MAX_RESPONSE_BYTES || body->size() > MAX_RESPONSE_BYTES - total)
                {
                    return 0;
                }
                body->append(ptr, total);

                return total;
            }

            // Fill buf with cryptographically strong random bytes, retrying on EINTR and short reads
            // so the caller never falls back to a non-cryptographic PRNG for the WS-UsernameToken nonce.
            bool FillSecureRandom(guint8 *buf, size_t len)
            {
                size_t filled = 0;

                while (filled < len)
                {
                    ssize_t got = getrandom(buf + filled, len - filled, 0);

                    if (got < 0)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }

                        return false;
                    }

                    filled += static_cast<size_t>(got);
                }

                return true;
            }

        } // namespace

        std::string
        OnvifComputePasswordDigest(const std::string &nonceRaw, const std::string &created, const std::string &password)
        {
            GChecksum *sha1 = g_checksum_new(G_CHECKSUM_SHA1);
            g_checksum_update(sha1, reinterpret_cast<const guchar *>(nonceRaw.data()), nonceRaw.size());
            g_checksum_update(sha1, reinterpret_cast<const guchar *>(created.data()), created.size());
            g_checksum_update(sha1, reinterpret_cast<const guchar *>(password.data()), password.size());

            static constexpr gsize SHA1_DIGEST_BYTES = 20; // WS-UsernameToken PasswordDigest is SHA-1
            guint8 digest[SHA1_DIGEST_BYTES];
            gsize digestLen = sizeof(digest);
            g_checksum_get_digest(sha1, digest, &digestLen);

            gchar *b64 = g_base64_encode(digest, digestLen);
            std::string result(b64);

            g_free(b64);
            g_checksum_free(sha1);

            return result;
        }

        OnvifSoapClient::OnvifSoapClient(std::string serviceUrl) : serviceUrl(std::move(serviceUrl)) {}

        bool OnvifParseDeviceInformation(const std::string &xml, OnvifDeviceInfo &out)
        {
            out = OnvifDeviceInfo {};

            xmlDoc *doc = xmlReadMemory(xml.data(),
                                        static_cast<int>(xml.size()),
                                        nullptr,
                                        nullptr,
                                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER | XML_PARSE_NONET);

            if (doc == nullptr)
            {
                return false;
            }

            xmlNode *root = xmlDocGetRootElement(doc);
            out.manufacturer = OnvifXmlFindText(root, "Manufacturer");
            out.model = OnvifXmlFindText(root, "Model");
            out.firmwareVersion = OnvifXmlFindText(root, "FirmwareVersion");
            out.serialNumber = OnvifXmlFindText(root, "SerialNumber");
            out.hardwareId = OnvifXmlFindText(root, "HardwareId");

            xmlFreeDoc(doc);

            // A SOAP Fault or unrelated payload parses fine but yields none of the expected fields;
            // treat that as a failure so discovery does not report a bogus device.
            return !(out.manufacturer.empty() && out.model.empty() && out.firmwareVersion.empty() &&
                     out.serialNumber.empty() && out.hardwareId.empty());
        }

        bool OnvifParseProfiles(const std::string &xml, std::vector<std::string> &tokensOut)
        {
            tokensOut.clear();

            xmlDoc *doc = xmlReadMemory(xml.data(),
                                        static_cast<int>(xml.size()),
                                        nullptr,
                                        nullptr,
                                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER | XML_PARSE_NONET);

            if (doc == nullptr)
            {
                return false;
            }

            std::vector<xmlNode *> profiles;
            OnvifXmlCollectByLocalName(xmlDocGetRootElement(doc), "Profiles", profiles);

            for (xmlNode *profile : profiles)
            {
                std::string token = OnvifXmlElementAttr(profile, "token");

                if (!token.empty())
                {
                    tokensOut.push_back(token);
                }
            }

            xmlFreeDoc(doc);

            return !tokensOut.empty();
        }

        // Strip any "userinfo@" from a URI's authority so embedded credentials are not leaked to
        // callers/logs; the driver supplies credentials separately (username/password resources).
        // File-local (static) so it is not exported from this translation unit.
        static std::string StripUriCredentials(const std::string &uri)
        {
            std::string::size_type schemeEnd = uri.find("://");

            if (schemeEnd == std::string::npos)
            {
                return uri;
            }

            std::string::size_type authStart = schemeEnd + 3;
            // The authority ends at the first '/', '?' or '#'.
            std::string::size_type authEnd = uri.find_first_of("/?#", authStart);
            std::string authority =
                uri.substr(authStart, authEnd == std::string::npos ? std::string::npos : authEnd - authStart);

            // Userinfo is everything up to the LAST '@' in the authority ('@' may appear in a password),
            // so using the first '@' would leave part of the credential behind.
            std::string::size_type at = authority.rfind('@');

            if (at == std::string::npos)
            {
                return uri;
            }

            return uri.substr(0, authStart) + authority.substr(at + 1) +
                   (authEnd == std::string::npos ? std::string() : uri.substr(authEnd));
        }

        std::string OnvifParseMediaUri(const std::string &xml)
        {
            xmlDoc *doc = xmlReadMemory(xml.data(),
                                        static_cast<int>(xml.size()),
                                        nullptr,
                                        nullptr,
                                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER | XML_PARSE_NONET);

            if (doc == nullptr)
            {
                return "";
            }

            std::string uri = OnvifXmlFindText(xmlDocGetRootElement(doc), "Uri");
            xmlFreeDoc(doc);

            return StripUriCredentials(uri);
        }

        std::string OnvifSoapClient::BuildEnvelope(const std::string &bodyXml, const OnvifCredentials &creds)
        {
            std::string header;

            if (!creds.username.empty() && !creds.password.empty())
            {
                // Raw 16-byte nonce; the Nonce element carries its Base64, the digest hashes the raw bytes.
                guint8 nonceBytes[16];

                // Use a CSPRNG so the nonce is unpredictable (WS-UsernameToken replay protection);
                // glib's g_random_* is non-cryptographic. If the CSPRNG cannot be read, fail envelope
                // construction (return empty) rather than downgrade an authenticated request to an
                // anonymous one or emit a predictable nonce; Post treats an empty envelope as an error.
                if (!FillSecureRandom(nonceBytes, sizeof(nonceBytes)))
                {
                    return std::string();
                }
                std::string nonceRaw(reinterpret_cast<const char *>(nonceBytes), sizeof(nonceBytes));

                gchar *nonceB64 = g_base64_encode(nonceBytes, sizeof(nonceBytes));

                GDateTime *now = g_date_time_new_now_utc();
                gchar *created = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");

                std::string digest = OnvifComputePasswordDigest(nonceRaw, created, creds.password);

                header = std::string("<s:Header><wsse:Security s:mustUnderstand=\"1\" xmlns:wsse=\"") + NS_WSSE +
                         "\" xmlns:wsu=\"" + NS_WSU + "\"><wsse:UsernameToken><wsse:Username>" +
                         OnvifXmlEscape(creds.username) + "</wsse:Username><wsse:Password Type=\"" +
                         PASSWORD_DIGEST_TYPE + "\">" + digest + "</wsse:Password><wsse:Nonce EncodingType=\"" +
                         BASE64_ENCODING_TYPE + "\">" + nonceB64 + "</wsse:Nonce><wsu:Created>" + created +
                         "</wsu:Created></wsse:UsernameToken></wsse:Security></s:Header>";

                g_free(nonceB64);
                g_free(created);
                g_date_time_unref(now);
            }

            return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><s:Envelope xmlns:s=\"") + NS_SOAP + "\">" +
                   header + "<s:Body>" + bodyXml + "</s:Body></s:Envelope>";
        }

        bool OnvifSoapClient::Post(const std::string &envelope, std::string &responseOut, std::string *error)
        {
            // Start from a clean slate so a reused output string (or a retry) cannot concatenate the
            // new response onto stale bytes; the curl write callback only appends.
            responseOut.clear();

            // An empty envelope means BuildEnvelope could not generate a secure nonce for an
            // authenticated request; fail rather than send an unauthenticated/malformed request.
            if (envelope.empty())
            {
                if (error != nullptr)
                {
                    *error = "failed to generate authentication nonce";
                }

                return false;
            }

            // libcurl requires a one-time, process-wide init before any easy handle is created; do it
            // exactly once in a thread-safe way since drivers may issue SOAP calls from worker threads.
            static std::once_flag curlInitFlag;
            std::call_once(curlInitFlag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

            CURL *curl = curl_easy_init();

            if (curl == nullptr)
            {
                if (error != nullptr)
                {
                    *error = "failed to initialize curl";
                }

                return false;
            }

            struct curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/soap+xml; charset=utf-8");

            // ONVIF service URLs are typically plain HTTP; default libcurl TLS verification applies to
            // any https:// endpoint (a self-signed camera cert would fail closed -- HTTPS is out of scope).
            // Restrict to HTTP(S) so a forged discovery XAddr cannot make libcurl POST via another scheme.
            curl_easy_setopt(curl, CURLOPT_URL, serviceUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long) (CURLPROTO_HTTP | CURLPROTO_HTTPS));
            curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long) (CURLPROTO_HTTP | CURLPROTO_HTTPS));
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, envelope.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(envelope.size()));
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseOut);
            // CURLOPT_TIMEOUT bounds the whole exchange; CONNECTTIMEOUT bounds just the connect phase
            // with a shorter budget so an unreachable camera on the LAN fails fast instead of burning
            // the full transfer timeout before the next candidate is tried.
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
            const long connectTimeoutSeconds = 5;
            curl_easy_setopt(curl,
                             CURLOPT_CONNECTTIMEOUT,
                             timeoutSeconds < connectTimeoutSeconds ? timeoutSeconds : connectTimeoutSeconds);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            CURLcode code = curl_easy_perform(curl);

            long httpStatus = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            if (code != CURLE_OK)
            {
                if (error != nullptr)
                {
                    *error = std::string("transport error: ") + curl_easy_strerror(code);
                }

                return false;
            }

            // ONVIF returns a SOAP Fault with an HTTP 4xx/5xx (commonly 400/401) when auth fails.
            if (httpStatus >= 400)
            {
                if (error != nullptr)
                {
                    *error = "ONVIF request rejected with HTTP status " + std::to_string(httpStatus) +
                             " (check credentials)";
                }

                return false;
            }

            return true;
        }

        std::string OnvifParseHwAddress(const std::string &xml)
        {
            xmlDoc *doc = xmlReadMemory(xml.data(),
                                        static_cast<int>(xml.size()),
                                        nullptr,
                                        nullptr,
                                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER | XML_PARSE_NONET);

            if (doc == nullptr)
            {
                return "";
            }

            // GetNetworkInterfacesResponse/NetworkInterfaces/Info/HwAddress. OnvifXmlFindText returns the
            // first HwAddress in document order (the first interface), which is the device's MAC.
            std::string hwAddress = OnvifXmlFindText(xmlDocGetRootElement(doc), "HwAddress");
            xmlFreeDoc(doc);

            return hwAddress;
        }

        bool OnvifSoapClient::GetHwAddress(const OnvifCredentials &creds, std::string &macOut, std::string *error)
        {
            std::string body = std::string("<tds:GetNetworkInterfaces xmlns:tds=\"") + NS_TDS + "\"/>";
            std::string response;

            if (!Post(BuildEnvelope(body, creds), response, error))
            {
                return false;
            }

            macOut = OnvifParseHwAddress(response);

            if (macOut.empty())
            {
                if (error != nullptr)
                {
                    *error = "GetNetworkInterfaces returned no HwAddress";
                }

                return false;
            }

            return true;
        }

        bool
        OnvifSoapClient::GetDeviceInformation(const OnvifCredentials &creds, OnvifDeviceInfo &out, std::string *error)
        {
            std::string body = std::string("<tds:GetDeviceInformation xmlns:tds=\"") + NS_TDS + "\"/>";
            std::string response;

            if (!Post(BuildEnvelope(body, creds), response, error))
            {
                return false;
            }

            if (!OnvifParseDeviceInformation(response, out))
            {
                if (error != nullptr)
                {
                    *error = "failed to parse GetDeviceInformation response";
                }

                return false;
            }

            return true;
        }

        bool OnvifSoapClient::GetProfiles(const OnvifCredentials &creds,
                                          std::vector<std::string> &profileTokensOut,
                                          std::string *error)
        {
            std::string body = std::string("<trt:GetProfiles xmlns:trt=\"") + NS_TRT + "\"/>";
            std::string response;

            if (!Post(BuildEnvelope(body, creds), response, error))
            {
                return false;
            }

            if (!OnvifParseProfiles(response, profileTokensOut))
            {
                if (error != nullptr)
                {
                    *error = "no media profiles returned by camera";
                }

                return false;
            }

            return true;
        }

        bool OnvifSoapClient::GetStreamUri(const OnvifCredentials &creds,
                                           const std::string &profileToken,
                                           std::string &rtspUriOut,
                                           std::string *error)
        {
            std::string body = std::string("<trt:GetStreamUri xmlns:trt=\"") + NS_TRT + "\" xmlns:tt=\"" + NS_TT +
                               "\"><trt:StreamSetup><tt:Stream>RTP-Unicast</tt:Stream><tt:Transport><tt:Protocol>RTSP"
                               "</tt:Protocol></tt:Transport></trt:StreamSetup><trt:ProfileToken>" +
                               OnvifXmlEscape(profileToken) + "</trt:ProfileToken></trt:GetStreamUri>";
            std::string response;

            if (!Post(BuildEnvelope(body, creds), response, error))
            {
                return false;
            }

            rtspUriOut = OnvifParseMediaUri(response);

            if (rtspUriOut.empty())
            {
                if (error != nullptr)
                {
                    *error = "GetStreamUri returned no URI";
                }

                return false;
            }

            return true;
        }

        bool OnvifSoapClient::GetSnapshotUri(const OnvifCredentials &creds,
                                             const std::string &profileToken,
                                             std::string &jpegUriOut,
                                             std::string *error)
        {
            std::string body = std::string("<trt:GetSnapshotUri xmlns:trt=\"") + NS_TRT + "\"><trt:ProfileToken>" +
                               OnvifXmlEscape(profileToken) + "</trt:ProfileToken></trt:GetSnapshotUri>";
            std::string response;

            if (!Post(BuildEnvelope(body, creds), response, error))
            {
                return false;
            }

            jpegUriOut = OnvifParseMediaUri(response);

            if (jpegUriOut.empty())
            {
                if (error != nullptr)
                {
                    *error = "GetSnapshotUri returned no URI";
                }

                return false;
            }

            return true;
        }

    } // namespace onvif
} // namespace barton
