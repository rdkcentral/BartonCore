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

#include "BartonOperationalCredentialsDelegate.hpp"
#include <lib/core/CHIPPersistentStorageDelegate.h>

namespace barton
{

class SelfSignedCertifierOperationalCredentialsIssuer : public BartonOperationalCredentialsDelegate
{
  public:
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

    SmartEvpPKey mKeyPair = {nullptr, EVP_PKEY_free}; // Used to back mRootCACert
    SmartX509 mRootCACert = {nullptr, X509_free};     // Used to sign client CSRs

    CHIP_ERROR AddExtensionToX509(X509 *cert, X509V3_CTX &extensionContext, int nid, std::string &data);

    CHIP_ERROR GenerateKeyPair(PersistentStorageDelegate &storageDelegate);
    CHIP_ERROR LoadOrGenKeyPair(PersistentStorageDelegate &storageDelegate);
    void SavePrivateKey(PersistentStorageDelegate &storageDelegate);

    CHIP_ERROR GenerateRootCACert(PersistentStorageDelegate &storageDelegate);
    CHIP_ERROR LoadOrGenRootCACert(PersistentStorageDelegate &storageDelegate);
    void SaveRootCACert(PersistentStorageDelegate &storageDelegate);

    CHIP_ERROR CreateX509FromRequest(X509 *&certificate, X509_REQ *request, NodeId nodeId, FabricId fabricId);
};

} // namespace barton
