// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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

#include <SelfSignedCertifierOperationalCredentialsIssuer.hpp>
extern "C"
{
#include <openssl/asn1.h>
#include <openssl/x509v3.h>
}

#define ISSUER_COUNTRY_CODE_US "US"
#define ISSUER_ORGNAIZATION     "Test Issuer"
#define ISSUER_ROOT_ID_DN "0000000000000001"

#define ROOT "Root"

#define STORAGE_KEY_PREFIX      "selfSignedRootCA_"
#define STORAGE_KEY_PRIVATE_KEY STORAGE_KEY_PREFIX "privateKey"
#define STORAGE_KEY_CERTIFICATE STORAGE_KEY_PREFIX "certificate"

// Must fit within uint16_t
#define STORAGE_BUFFER_SIZE UINT16_MAX

// Keep it simple
#define ROOT_CA_SERIAL_NUMBER 1
#define CLIENT_SERIAL_NUMBER 2

namespace chip
{
namespace Controller
{

CHIP_ERROR
SelfSignedCertifierOperationalCredentialsIssuer::AddExtensionToX509(X509 *cert,
                                                                    X509V3_CTX &extensionContext,
                                                                    int nid,
                                                                    std::string &data)
{
    SmartX509Extension extension =
        SmartX509Extension(X509V3_EXT_conf_nid(NULL, &extensionContext, nid, data.c_str()), X509_EXTENSION_free);

    VerifyOrReturnError(extension != nullptr, CHIP_ERROR_INTERNAL);

    VerifyOrReturnError(X509_add_ext(cert, extension.get(), -1) == 1, CHIP_ERROR_INTERNAL);

    return CHIP_NO_ERROR;
}

CHIP_ERROR SelfSignedCertifierOperationalCredentialsIssuer::GenerateKeyPair()
{
    if (!mKeyPair)
    {
        SmartEvpPKey localKeyPair = SmartEvpPKey(EVP_PKEY_new(), EVP_PKEY_free);
        VerifyOrReturnError(localKeyPair != nullptr, CHIP_ERROR_INTERNAL);

        SmartEcKey ecKeyPair = SmartEcKey(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), EC_KEY_free);
        VerifyOrReturnError(ecKeyPair != nullptr, CHIP_ERROR_INTERNAL);

        VerifyOrReturnError(EC_KEY_generate_key(ecKeyPair.get()) == 1, CHIP_ERROR_INTERNAL);

        if (EVP_PKEY_assign_EC_KEY(localKeyPair.get(), ecKeyPair.get()) == 1)
        {
            ecKeyPair.release();
        }
        else
        {
            return CHIP_ERROR_INTERNAL;
        }

        mKeyPair = std::move(localKeyPair);

        SavePrivateKey();
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR SelfSignedCertifierOperationalCredentialsIssuer::LoadOrGenKeyPair()
{
    if (!mKeyPair)
    {
        // First attempt to load the keypair from storage.
        unsigned char buffer[STORAGE_BUFFER_SIZE];
        // See d2i_TYPE and i2d_TYPE documentation on ppin argument and the WARNINGS section
        const unsigned char *tempBufferPointer = buffer;
        uint16_t bufferSize = STORAGE_BUFFER_SIZE;

        CHIP_ERROR err =
            mStorageDelegate.SyncGetKeyValue(STORAGE_KEY_PRIVATE_KEY, static_cast<void *>(buffer), bufferSize);

        if (err == CHIP_NO_ERROR)
        {
            EVP_PKEY *localKey = nullptr;
            localKey = d2i_PrivateKey(EVP_PKEY_EC, &localKey, &tempBufferPointer, static_cast<long>(bufferSize));

            VerifyOrReturnError(localKey != nullptr, CHIP_ERROR_INTERNAL);

            mKeyPair = SmartEvpPKey(localKey, EVP_PKEY_free);
            localKey = nullptr;
        }
    }

    if (!mKeyPair)
    {
        // Didn't find the key, generate a new key pair.
        ReturnErrorOnFailure(GenerateKeyPair());
    }

    return CHIP_NO_ERROR;
}

void SelfSignedCertifierOperationalCredentialsIssuer::SavePrivateKey()
{
    if (mKeyPair)
    {
        unsigned char *buffer = nullptr;
        int bufferSize;

        bufferSize = i2d_PrivateKey(mKeyPair.get(), &buffer);

        if (bufferSize >= 0 && bufferSize <= UINT16_MAX)
        {
            mStorageDelegate.SyncSetKeyValue(STORAGE_KEY_PRIVATE_KEY,
                                             static_cast<void *>(buffer),
                                             static_cast<uint16_t>(bufferSize));
        }

        free(buffer);
    }
}

CHIP_ERROR SelfSignedCertifierOperationalCredentialsIssuer::GenerateRootCACert()
{
    if (!mRootCACert)
    {
        SmartX509 localRootCertificate = SmartX509(X509_new(), X509_free);
        VerifyOrReturnError(localRootCertificate != nullptr, CHIP_ERROR_INTERNAL);

        // Openssl defines qualifier "X509_VERSION_3" for this in openssl 3.0+. However,
        // the actual values are well defined by the x509 standard (actual version minus 1).
        VerifyOrReturnError(X509_set_version(localRootCertificate.get(), 2) == 1, CHIP_ERROR_INTERNAL);

        VerifyOrReturnError(
            ASN1_INTEGER_set(X509_get_serialNumber(localRootCertificate.get()), ROOT_CA_SERIAL_NUMBER) == 1,
            CHIP_ERROR_INTERNAL);

        VerifyOrReturnError(X509_gmtime_adj(X509_get_notBefore(localRootCertificate.get()), 0) != nullptr,
                            CHIP_ERROR_INTERNAL);
        VerifyOrReturnError(
            X509_gmtime_adj(X509_get_notAfter(localRootCertificate.get()), CERT_EXPIRATION_TIME_SECONDS) != nullptr,
            CHIP_ERROR_INTERNAL);

        VerifyOrReturnError(X509_set_pubkey(localRootCertificate.get(), mKeyPair.get()) == 1, CHIP_ERROR_INTERNAL);

        X509_NAME *subjectName = X509_get_subject_name(localRootCertificate.get());
        VerifyOrReturnError(subjectName != nullptr, CHIP_ERROR_INTERNAL);

        std::string commonName = mRootCommonName.empty() ? COMMON_NAME_DEFAULT : mRootCommonName;
        commonName = commonName + " " ROOT;

        VerifyOrReturnError(
            X509_NAME_add_entry_by_txt(subjectName,
                                       "C",
                                       MBSTRING_ASC,
                                       reinterpret_cast<unsigned char *>(const_cast<char *>(ISSUER_COUNTRY_CODE_US)),
                                       -1,
                                       -1,
                                       0) == 1,
            CHIP_ERROR_INTERNAL);

        VerifyOrReturnError(
            X509_NAME_add_entry_by_txt(subjectName,
                                       "O",
                                       MBSTRING_ASC,
                                       reinterpret_cast<unsigned char *>(const_cast<char *>(ISSUER_ORGNAIZATION)),
                                       -1,
                                       -1,
                                       0),
            CHIP_ERROR_INTERNAL);

        VerifyOrReturnError(
            X509_NAME_add_entry_by_txt(subjectName,
                                       "CN",
                                       MBSTRING_ASC,
                                       reinterpret_cast<unsigned char *>(const_cast<char *>(commonName.c_str())),
                                       -1,
                                       -1,
                                       0),
            CHIP_ERROR_INTERNAL);

        // Matter root id as defined by Appendix E in the matter spec
        VerifyOrReturnError(
            X509_NAME_add_entry_by_txt(subjectName,
                                       "1.3.6.1.4.1.37244.1.4",
                                       MBSTRING_ASC,
                                       reinterpret_cast<unsigned char *>(const_cast<char *>(ISSUER_ROOT_ID_DN)),
                                       -1,
                                       -1,
                                       0),
            CHIP_ERROR_INTERNAL);

        VerifyOrReturnError(X509_set_issuer_name(localRootCertificate.get(), subjectName) == 1, CHIP_ERROR_INTERNAL);

        std::string basicConstraints = "critical, CA:TRUE";
        std::string keyUsage = "critical, digitalSignature, keyCertSign, cRLSign";
        std::string subjectKeyIdentifier = "hash";
        std::string authorityKeyIdentifier = "keyid:always";

        X509V3_CTX extensionContext;
        X509V3_set_ctx_nodb(&extensionContext);
        X509V3_set_ctx(&extensionContext, localRootCertificate.get(), localRootCertificate.get(), NULL, NULL, 0);

        ReturnErrorOnFailure(
            AddExtensionToX509(localRootCertificate.get(), extensionContext, NID_basic_constraints, basicConstraints));

        ReturnErrorOnFailure(AddExtensionToX509(localRootCertificate.get(), extensionContext, NID_key_usage, keyUsage));

        ReturnErrorOnFailure(AddExtensionToX509(localRootCertificate.get(),
                                                extensionContext,
                                                NID_subject_key_identifier,
                                                subjectKeyIdentifier));

        ReturnErrorOnFailure(AddExtensionToX509(localRootCertificate.get(),
                                                extensionContext,
                                                NID_authority_key_identifier,
                                                authorityKeyIdentifier));

        VerifyOrReturnError(X509_sign(localRootCertificate.get(), mKeyPair.get(), EVP_sha256()) > 0,
                            CHIP_ERROR_INTERNAL);

        mRootCACert = std::move(localRootCertificate);

        SaveRootCACert();
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR SelfSignedCertifierOperationalCredentialsIssuer::LoadOrGenRootCACert()
{
    ReturnErrorOnFailure(LoadOrGenKeyPair());

    if (!mRootCACert)
    {
        // First attempt to load the certificate from storage.
        unsigned char buffer[STORAGE_BUFFER_SIZE];
        // See d2i_TYPE and i2d_TYPE info on ppin argument and WARNINGS section
        const unsigned char *tempBufferPointer = buffer;
        uint16_t bufferSize = STORAGE_BUFFER_SIZE;

        CHIP_ERROR err =
            mStorageDelegate.SyncGetKeyValue(STORAGE_KEY_CERTIFICATE, static_cast<void *>(buffer), bufferSize);
        if (err == CHIP_NO_ERROR)
        {
            X509 *localCert = nullptr;
            localCert = d2i_X509(&localCert, &tempBufferPointer, static_cast<long>(bufferSize));

            VerifyOrReturnError(localCert != nullptr, CHIP_ERROR_INTERNAL);

            mRootCACert = SmartX509(localCert, X509_free);
            localCert = nullptr;
        }
    }

    if (!mRootCACert)
    {
        // Failed to load it or didn't find one, generate a new certificate.
        ReturnErrorOnFailure(GenerateRootCACert());
    }

    return CHIP_NO_ERROR;
}

void SelfSignedCertifierOperationalCredentialsIssuer::SaveRootCACert()
{
    if (mRootCACert)
    {
        unsigned char *buffer = nullptr;
        int bufferSize;

        bufferSize = i2d_X509(mRootCACert.get(), &buffer);

        if (bufferSize >= 0 && bufferSize <= UINT16_MAX)
        {
            mStorageDelegate.SyncSetKeyValue(STORAGE_KEY_CERTIFICATE,
                                             static_cast<void *>(buffer),
                                             static_cast<uint16_t>(bufferSize));
        }

        free(buffer);
    }
}

CHIP_ERROR SelfSignedCertifierOperationalCredentialsIssuer::CreateX509FromRequest(X509 *&certificate,
                                                                                  X509_REQ *request,
                                                                                  NodeId nodeId,
                                                                                  FabricId fabricId)
{
    VerifyOrReturnError(request != nullptr, CHIP_ERROR_INTERNAL);

    SmartX509 localCertificate = SmartX509(X509_new(), X509_free);

    VerifyOrReturnError(localCertificate != nullptr, CHIP_ERROR_INTERNAL);
    VerifyOrReturnError(X509_set_version(localCertificate.get(), 2) == 1, CHIP_ERROR_INTERNAL);

    VerifyOrReturnError(ASN1_INTEGER_set(X509_get_serialNumber(localCertificate.get()), CLIENT_SERIAL_NUMBER) == 1,
                        CHIP_ERROR_INTERNAL);

    VerifyOrReturnError(X509_set_issuer_name(localCertificate.get(),
                                             X509_get_subject_name(const_cast<const X509 *>(mRootCACert.get()))) == 1,
                        CHIP_ERROR_INTERNAL);

    // Discard CSR subject, it's useless.
    X509_NAME *subjectName = X509_get_subject_name(const_cast<const X509 *>(localCertificate.get()));
    VerifyOrReturnError(subjectName != nullptr, CHIP_ERROR_INTERNAL);

    std::string commonName = mClientCommonName.empty() ? COMMON_NAME_DEFAULT : mClientCommonName;
    std::string nodeIdStr = ToHexString(nodeId);
    std::string fabricIdStr = ToHexString(fabricId);

    // TODO: Matter CSR doesn't bother with a CN so we add one. It would be better if we tested to see if it already
    // exists in the request, though.
    VerifyOrReturnError(
        X509_NAME_add_entry_by_txt(subjectName,
                                   "CN",
                                   MBSTRING_ASC,
                                   reinterpret_cast<unsigned char *>(const_cast<char *>(commonName.c_str())),
                                   -1,
                                   -1,
                                   0),
        CHIP_ERROR_INTERNAL);

    // Matter node id as defined by Appendix E in the matter spec
    VerifyOrReturnError(
        X509_NAME_add_entry_by_txt(subjectName,
                                   "1.3.6.1.4.1.37244.1.1",
                                   MBSTRING_ASC,
                                   reinterpret_cast<unsigned char *>(const_cast<char *>(nodeIdStr.c_str())),
                                   -1,
                                   -1,
                                   0),
        CHIP_ERROR_INTERNAL);

    // Matter fabric id as defined by Appendix E in the matter spec
    VerifyOrReturnError(
        X509_NAME_add_entry_by_txt(subjectName,
                                   "1.3.6.1.4.1.37244.1.5",
                                   MBSTRING_ASC,
                                   reinterpret_cast<unsigned char *>(const_cast<char *>(fabricIdStr.c_str())),
                                   -1,
                                   -1,
                                   0),
        CHIP_ERROR_INTERNAL);

    VerifyOrReturnError(X509_set_subject_name(localCertificate.get(), subjectName) == 1, CHIP_ERROR_INTERNAL);

    VerifyOrReturnError(X509_gmtime_adj(X509_get_notBefore(localCertificate.get()), 0) != nullptr, CHIP_ERROR_INTERNAL);
    VerifyOrReturnError(X509_gmtime_adj(X509_get_notAfter(localCertificate.get()), CERT_EXPIRATION_TIME_SECONDS) !=
                            nullptr,
                        CHIP_ERROR_INTERNAL);

    VerifyOrReturnError(X509_set_pubkey(localCertificate.get(), X509_REQ_get_pubkey(request)) == 1,
                        CHIP_ERROR_INTERNAL);

    std::string basicConstraints = "critical, CA:FALSE";
    std::string keyUsage = "critical, digitalSignature, keyEncipherment";
    std::string extendedKeyUsage = "critical, serverAuth, clientAuth";
    std::string subjectKeyIdentifier = "hash";
    std::string authorityKeyIdentifier = "keyid:always";

    X509V3_CTX extensionContext;
    X509V3_set_ctx_nodb(&extensionContext);
    X509V3_set_ctx(&extensionContext, mRootCACert.get(), localCertificate.get(), NULL, NULL, 0);

    ReturnErrorOnFailure(
        AddExtensionToX509(localCertificate.get(), extensionContext, NID_basic_constraints, basicConstraints));

    ReturnErrorOnFailure(AddExtensionToX509(localCertificate.get(), extensionContext, NID_key_usage, keyUsage));

    ReturnErrorOnFailure(
        AddExtensionToX509(localCertificate.get(), extensionContext, NID_ext_key_usage, extendedKeyUsage));

    ReturnErrorOnFailure(
        AddExtensionToX509(localCertificate.get(), extensionContext, NID_subject_key_identifier, subjectKeyIdentifier));

    ReturnErrorOnFailure(AddExtensionToX509(localCertificate.get(),
                                            extensionContext,
                                            NID_authority_key_identifier,
                                            authorityKeyIdentifier));

    VerifyOrReturnError(X509_sign(localCertificate.get(), mKeyPair.get(), EVP_sha256()) > 0, CHIP_ERROR_INTERNAL);

    X509_free(certificate);
    certificate = localCertificate.release();

    return CHIP_NO_ERROR;
}

CHIP_ERROR SelfSignedCertifierOperationalCredentialsIssuer::GenerateNOCChainAfterValidation(NodeId nodeId,
                                                                                            FabricId fabricId,
                                                                                            const ByteSpan &csr,
                                                                                            const ByteSpan &nonce,
                                                                                            MutableByteSpan &rcac,
                                                                                            MutableByteSpan &icac,
                                                                                            MutableByteSpan &noc)
{
    ReturnErrorOnFailure(LoadOrGenRootCACert());

    const unsigned char *csrBytes = csr.data();
    SmartX509Req req = SmartX509Req(d2i_X509_REQ(nullptr, &csrBytes, static_cast<long>(csr.size())), X509_REQ_free);

    VerifyOrReturnError(req != nullptr, CHIP_ERROR_INTERNAL);

    X509 *clientCertificate = nullptr;
    ReturnErrorOnFailure(CreateX509FromRequest(clientCertificate, req.get(), nodeId, fabricId));

    SmartX509 clientCert = SmartX509(clientCertificate, X509_free);

    VerifyOrReturnError(clientCert != nullptr, CHIP_ERROR_INTERNAL);

    ReturnErrorOnFailure(PutX509(clientCert.get(), noc));
    ReturnErrorOnFailure(PutX509(mRootCACert.get(), rcac));
    // Make sure we're clear that there's no intermediate
    icac.reduce_size(0);

    return CHIP_NO_ERROR;
}

} // namespace Controller
} // namespace chip
