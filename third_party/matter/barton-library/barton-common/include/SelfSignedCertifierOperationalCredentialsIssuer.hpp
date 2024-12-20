// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2023 Comcast Cable Communications Management, LLC
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

#include <CertifierOperationalCredentialsIssuer.hpp>
#include <lib/core/CHIPPersistentStorageDelegate.h>

#define CERT_EXPIRATION_TIME_SECONDS (60 * 60 * 24 * 365 * 20) // 20 years...ish
#define COMMON_NAME_DEFAULT          "My Test ECC Class I"

namespace chip
{
namespace Controller
{

class DLL_EXPORT SelfSignedCertifierOperationalCredentialsIssuer : public CertifierOperationalCredentialsIssuer
{
  public:
    SelfSignedCertifierOperationalCredentialsIssuer(PersistentStorageDelegate &storageDelegate)
        : mStorageDelegate(storageDelegate)
    {
    }

    /**
     * @brief Set the certificate authority common name prefix. The root CA Subject will be this string
     *        + " Root" concatenated to it.
     * @note optional, will use the value of the macro COMMON_NAME_DEFAULT + " Root" when not provided
     *
     * @param commonName
     */
    void SetRootCommonName(const std::string &commonName) { mRootCommonName = commonName; }

    /**
     * @brief Set the client certificate common name.
     * @note optional, will use the value of the macro COMMON_NAME_DEFAULT when not provided
     *
     * @param commonName
     */
    void SetClientCommonName(const std::string &commonName) { mClientCommonName = commonName; }

    CHIP_ERROR GenerateNOCChainAfterValidation(NodeId nodeId,
                                               FabricId fabricId,
                                               const ByteSpan &csr,
                                               const ByteSpan &nonce,
                                               MutableByteSpan &rcac,
                                               MutableByteSpan &icac,
                                               MutableByteSpan &noc) override;

  private:
    using SmartEvpPKey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using SmartEcKey = std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)>;
    using SmartX509 = std::unique_ptr<X509, decltype(&X509_free)>;
    using SmartX509Req = std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)>;
    using SmartX509Extension = std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)>;

    PersistentStorageDelegate &mStorageDelegate;
    SmartEvpPKey mKeyPair = {nullptr, EVP_PKEY_free}; // Used to back mRootCACert
    SmartX509 mRootCACert = {nullptr, X509_free};     // Used to sign client CSRs
    std::string mRootCommonName;
    std::string mClientCommonName;

    CHIP_ERROR AddExtensionToX509(X509 *cert, X509V3_CTX &extensionContext, int nid, std::string &data);

    CHIP_ERROR GenerateKeyPair();
    CHIP_ERROR LoadOrGenKeyPair();
    void SavePrivateKey();

    CHIP_ERROR GenerateRootCACert();
    CHIP_ERROR LoadOrGenRootCACert();
    void SaveRootCACert();

    CHIP_ERROR CreateX509FromRequest(X509 *&certificate, X509_REQ *request, NodeId nodeId, FabricId fabricId);
};

} // namespace Controller
} // namespace chip
