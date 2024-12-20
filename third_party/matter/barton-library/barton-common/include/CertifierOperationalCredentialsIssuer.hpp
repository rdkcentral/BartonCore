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

#include <controller/OperationalCredentialsDelegate.h>
#include <credentials/CHIPCert.h>
#include <crypto/CHIPCryptoPAL.h>
#include <iomanip>
#include <iostream>
#include <lib/core/PeerId.h>
#include <lib/support/DLLUtil.h>
#include <string>

extern "C"
{
#include <curl/curl.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
}

struct CERTIFIER;

namespace chip
{
namespace Controller
{

class DLL_EXPORT CertifierOperationalCredentialsIssuer : public OperationalCredentialsDelegate
{
  public:
    virtual ~CertifierOperationalCredentialsIssuer() {}

    CHIP_ERROR GenerateNOCChain(const ByteSpan &csrElements,
                                const ByteSpan &csrNonce,
                                const ByteSpan &attestationSignature,
                                const ByteSpan &attestationChallenge,
                                const ByteSpan &DAC,
                                const ByteSpan &PAI,
                                Callback::Callback<OnNOCChainGeneration> *onCompletion) override;

    void SetNodeIdForNextNOCRequest(NodeId nodeId) override { mNodeId = nodeId; }

    void SetFabricIdForNextNOCRequest(FabricId fabricId) override { mFabricId = fabricId; }

    virtual CHIP_ERROR GenerateNOCChainAfterValidation(NodeId nodeId,
                                                       FabricId fabricId,
                                                       const ByteSpan &csr,
                                                       const ByteSpan &nonce,
                                                       MutableByteSpan &rcac,
                                                       MutableByteSpan &icac,
                                                       MutableByteSpan &noc);

    /**
     * @brief Set the xPKI CA name
     *
     * @param caName
     */
    void SetCAProfile(const std::string &caName) { mCertifierProfile = caName; }

    /**
     * @brief Set the xPKI authorization token for NOC chain validation
     *
     * @param token A valid bearer token (e.g., SAT)
     */
    void SetAuthToken(const std::string &token) { mAuthorizationToken = token; }

    /**
     * @brief Set the fabric IPK for the next NOC chain generation
     *
     * @param active_ipk_span The current IPK for the fabric that is commissioning
     * @note Be sure to invoke this to synchronize the commissioning fabric's active IPK
     *       when starting a commissioning session. If the IPK doesn't match the commissioner's
     *       active IPK, CASE will fail to establish.
     * @return CHIP_ERROR CHIP_ERROR_INVALID_ARGUMENT if input IPK span is empty
     */
    inline CHIP_ERROR SetIPKForNextNOCRequest(const chip::Crypto::IdentityProtectionKeySpan &active_ipk_span)
    {
        mIPK = MakeOptional(chip::Crypto::IdentityProtectionKey(active_ipk_span));

        return CHIP_NO_ERROR;
    }

    enum ApiEnv
    {
        prod,
        stage
    };

    inline void SetApiEnv(ApiEnv env) { mApiEnv = env; }

  private:
    NodeId mNodeId;
    FabricId mFabricId;

    using CurlEasy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    using Pkcs7 = std::unique_ptr<PKCS7, decltype(&PKCS7_free)>;

    /**
     * @brief The Matter CA to request certificates from
     */
    std::string mCertifierProfile = "XFN_Matter_OP_ICA";

    std::string mAuthorizationToken;

    ApiEnv mApiEnv = ApiEnv::prod;

    chip::Optional<chip::Crypto::IdentityProtectionKey> mIPK;

    std::string CreateNOCRequest(const ByteSpan &csr, FabricId fabricId, NodeId nodeId);
    std::string CreateCRTNonce();
    std::string CreateSATCRT(const std::string &sat);
    CHIP_ERROR FetchNOC(const ByteSpan &csr, FabricId fabricId, NodeId nodeId, std::string &pkcs7NOCOut);

  protected:
    CHIP_ERROR PutX509(const X509 *cert, MutableByteSpan &outCert);

    /**
     * @brief Makes a string that represents a number in hex, with leading zeroes.
     *
     * @tparam T A numeric type (int, uint64_t, ...)
     * @param number
     * @return std::string
     * //TODO: Use c++20 'concept' to constrain T to numeric.
     */
    template <typename T> inline std::string ToHexString(T number)
    {
        std::stringstream os;

        // Matter requires uppercase hex for ID attributes; xPKI wants no radix indicator (0x).
        os << std::uppercase << std::hex << std::setfill('0') << std::setw(sizeof(T) * 2) << std::right << number;

        return os.str();
    }
};

} // namespace Controller
} // namespace chip
