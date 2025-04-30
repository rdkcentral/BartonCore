// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2021 Comcast Cable Communications Management, LLC
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
// ------------------------------ tabstop = 4 ----------------------------------

#pragma once

#include "../BartonOperationalCredentialsDelegate.hpp"
#include <controller/OperationalCredentialsDelegate.h>
#include <credentials/CHIPCert.h>
#include <crypto/CHIPCryptoPAL.h>
#include <iomanip>
#include <iostream>
#include <lib/core/PeerId.h>
#include <lib/support/DLLUtil.h>
#include <string>

extern "C" {
#include <curl/curl.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
}

namespace barton
{

    class CertifierOperationalCredentialsIssuer final : public BartonOperationalCredentialsDelegate
    {
    public:
        enum ApiEnv
        {
            prod,
            stage
        };

        CHIP_ERROR GenerateNOCChainAfterValidation(NodeId nodeId,
                                                   FabricId fabricId,
                                                   const ByteSpan &csr,
                                                   const ByteSpan &nonce,
                                                   MutableByteSpan &rcac,
                                                   MutableByteSpan &icac,
                                                   MutableByteSpan &noc) override;

    private:
        using CurlEasy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
        using Pkcs7 = std::unique_ptr<PKCS7, decltype(&PKCS7_free)>;

        /**
         * @brief The Matter CA to request certificates from
         */
        std::string mCertifierProfile = "XFN_Matter_OP_ICA";

        std::string mAuthorizationToken;

        ApiEnv mApiEnv = ApiEnv::prod;

        std::string CreateNOCRequest(const ByteSpan &csr, FabricId fabricId, NodeId nodeId);
        std::string CreateCRTNonce();
        std::string CreateSATCRT(const std::string &sat);
        CHIP_ERROR FetchNOC(const ByteSpan &csr, FabricId fabricId, NodeId nodeId, std::string &pkcs7NOCOut);
        std::string GetAuthToken();
        void SetOperationalCredsIssuerApiEnv();
        ApiEnv GetIssuerApiEnvFromString(std::string operationalEnv);
    };

} // namespace barton
