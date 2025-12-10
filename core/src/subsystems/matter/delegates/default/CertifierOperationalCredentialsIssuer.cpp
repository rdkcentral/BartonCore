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

// Copyright 2021-2023 Project CHIP Authors
// All rights reserved.
// Licensed under the Apache License, Version 2.0 (the "License")

#include "CertifierOperationalCredentialsIssuer.hpp"
#include "../BartonMatterDelegateRegistry.hpp"

#include <stddef.h>

#include <controller/CommissioneeDeviceProxy.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <lib/asn1/ASN1.h>
#include <lib/asn1/ASN1Macros.h>
#include <lib/core/TLV.h>
#include <lib/support/Base64.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/ScopedBuffer.h>

#include <memory>
#include <sstream>
#include <string>

#include <json/json.h>

extern "C" {
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <icLog/logging.h>

#include "deviceServiceConfiguration.h"
#include "deviceServiceProps.h"
}

extern "C" {
    #define LOG_TAG "CertifierOperationalCredentialsIssuer"
    #define logFmt(fmt) "%s: " fmt, __func__
}

using namespace barton;
using namespace Credentials;
using namespace Crypto;
using namespace ASN1;
using namespace TLV;
using namespace chip::Platform;

namespace {
    const char *OPERATIONAL_ENVIRONMENT_STRING_PROD  = "prod";
    const char *OPERATIONAL_ENVIRONMENT_STRING_STAGE = "stage";
    const char *DEFAULT_OPERATIONAL_ENVIRONMENT = OPERATIONAL_ENVIRONMENT_STRING_PROD;

    std::shared_ptr<CertifierOperationalCredentialsIssuer> operationalCredentialsIssuer =
        std::make_shared<CertifierOperationalCredentialsIssuer>();
    auto delegateRegistered = BartonMatterDelegateRegistry::Instance().RegisterBartonOperationalCredentialDelegate(
        operationalCredentialsIssuer);
} // namespace

CHIP_ERROR CertifierOperationalCredentialsIssuer::GenerateNOCChainAfterValidation(NodeId nodeId, FabricId fabricId,
                                                                                  const ByteSpan & csr, const ByteSpan & nonce,
                                                                                  MutableByteSpan & rcac, MutableByteSpan & icac,
                                                                                  MutableByteSpan & noc)
{
    SetOperationalCredsIssuerApiEnv();

    mAuthorizationToken = GetAuthToken();
    VerifyOrReturnError(!mAuthorizationToken.empty(), CHIP_ERROR_INCORRECT_STATE);

    std::string pkcs7NOC;
    ReturnErrorOnFailure(FetchNOC(csr, fabricId, nodeId, pkcs7NOC));

    const STACK_OF(X509) * certs = NULL;

    BIO * mem      = BIO_new_mem_buf(pkcs7NOC.c_str(), -1);
    Pkcs7 nocChain = Pkcs7(PEM_read_bio_PKCS7(mem, NULL, NULL, NULL), PKCS7_free);

    BIO_free_all(mem);
    mem = nullptr;

    VerifyOrReturnError(nocChain != nullptr, CHIP_ERROR_INTERNAL);

    switch (OBJ_obj2nid(nocChain->type))
    {
    case NID_pkcs7_signed:
        certs = nocChain->d.sign->cert;
        break;

    case NID_pkcs7_signedAndEnveloped:
        certs = nocChain->d.signed_and_enveloped->cert;
        break;

    default:
        break;
    }

    VerifyOrReturnError(sk_X509_num(certs) == 3, CHIP_ERROR_CERT_NOT_FOUND);

    ReturnErrorOnFailure(PutX509(sk_X509_value(certs, 0), noc));
    ReturnErrorOnFailure(PutX509(sk_X509_value(certs, 1), icac));
    ReturnErrorOnFailure(PutX509(sk_X509_value(certs, 2), rcac));

    return CHIP_NO_ERROR;
}

std::string CertifierOperationalCredentialsIssuer::CreateNOCRequest(const ByteSpan & csr, FabricId fabricId, NodeId nodeId)
{
    Json::Value doc;

    chip::Platform::ScopedMemoryBuffer<char> b64Csr;
    b64Csr.Alloc(BASE64_ENCODED_LEN(csr.size()) + 1);
    uint16_t csrLen = chip::Base64Encode(csr.data(), static_cast<uint16_t>(csr.size()), b64Csr.Get());
    b64Csr[csrLen]  = '\0';

    doc["csr"]             = b64Csr.Get();
    doc["profileName"]     = mCertifierProfile;
    doc["fabricId"]        = ToHexString(fabricId);
    doc["nodeId"]          = ToHexString(nodeId);
    doc["certificateOnly"] = false;
    doc["certificateLite"] = true;
    doc["validityDays"]    = 365 * 15;

    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, doc);
}

std::string CertifierOperationalCredentialsIssuer::CreateCRTNonce()
{
    const char * dict                 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    constexpr uint8_t CRT_NONCE_BYTES = 16;

    chip::Platform::ScopedMemoryBuffer<uint8_t> nonce;
    nonce.Alloc(CRT_NONCE_BYTES);
    size_t divisor = strlen(dict) - 1;

    Crypto::DRBG_get_bytes(nonce.Get(), CRT_NONCE_BYTES);

    std::stringstream out;

    for (uint8_t i = 0; i < 16; i++)
    {
        out << dict[nonce[i] % divisor];
    }

    return out.str();
}

std::string CertifierOperationalCredentialsIssuer::CreateSATCRT(const std::string & sat)
{
    Json::Value doc;

    doc["tokenType"] = "SAT";
    doc["nonce"]     = CreateCRTNonce();
    doc["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    doc["token"] = sat;

    Json::StreamWriterBuilder builder;
    auto tmp = Json::writeString(builder, doc);

    chip::Platform::ScopedMemoryBuffer<char> b64Crt;
    b64Crt.Alloc(BASE64_ENCODED_LEN(tmp.size()) + 1);
    uint16_t crtLen = chip::Base64Encode((const uint8_t *) tmp.c_str(), static_cast<uint16_t>(tmp.size()), b64Crt.Get());
    std::string out(b64Crt.Get(), crtLen);

    return out;
}

CHIP_ERROR CertifierOperationalCredentialsIssuer::FetchNOC(const ByteSpan & csr, FabricId fabricId, NodeId nodeId,
                                                           std::string & pkcs7NOCOut)
{
    VerifyOrReturnError(!mAuthorizationToken.empty(), CHIP_ERROR_INCORRECT_STATE);

    CurlEasy curl = CurlEasy(curl_easy_init(), curl_easy_cleanup);

    std::string host;

    switch (mApiEnv)
    {
    case ApiEnv::prod:
        host = "certifier.xpki.io";
        break;

    case ApiEnv::stage:
        host = "certifier-stage.xpki.io";
        break;
    }

    std::stringstream url;
    url << "https://" << host << "/v1/certifier/certificate";

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.str().c_str());

    std::string request = CreateNOCRequest(csr, fabricId, nodeId);
    std::stringbuf responseData;

    VerifyOrReturnError(!request.empty(), CHIP_ERROR_INTERNAL);

    std::string satCRT = CreateSATCRT(mAuthorizationToken);
    std::stringstream trackingId;
    trackingId << "x-xpki-tracking-id: " << CreateCRTNonce();

    struct curl_slist * headers = nullptr;
    headers                     = curl_slist_append(headers, "Content-Type: application/json");
    // FIXME: create mutator for this
    headers = curl_slist_append(headers, "x-xpki-source: matter-commissioner");
    headers = curl_slist_append(headers, trackingId.str().c_str());

    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, request.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, false);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl.get(), CURLOPT_XOAUTH2_BEARER, satCRT.c_str());

    curl_easy_setopt(
        curl.get(), CURLOPT_WRITEFUNCTION, (curl_write_callback)[](char * ptr, size_t size, size_t nmemb, void * userdata)->size_t {
            auto outbuf = static_cast<std::stringbuf *>(userdata);
            return outbuf->sputn(ptr, size * nmemb);
        });

    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, (void *) &responseData);

    ChipLogProgress(Controller, "Sending HTTP request to certifier.");

    icTrace("Certifier Request:\n8<---\n%s\n8<---\n", request.c_str());
    icTrace("SAT CRT:\n8<---\n%s\n8<---\n", satCRT.c_str());
    icTrace("Headers:");
    icTrace("\t%s", trackingId.str().c_str());
    icTrace("\tContent-Type: application/json");
    icTrace("\tx-xpki-source: matter-commissioner");

    CURLcode res = curl_easy_perform(curl.get());
    curl_slist_free_all(headers);
    headers = nullptr;

    auto responseBody = responseData.str();

    if (res != CURLE_OK)
    {
        ChipLogError(Controller, "Failed to request operational certificate. cURL error '%s'", curl_easy_strerror(res));

        return CHIP_ERROR_INTERNAL;
    }

    long httpCode;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode >= 400)
    {
        ChipLogError(Controller, "Certifier: HTTP %ld \n8<---\n%s\n8<---\n", httpCode, responseBody.c_str());

        return CHIP_ERROR_INTERNAL;
    }
    else
    {
        ChipLogProgress(Controller, "Certifier HTTP transaction complete.");
    }

    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value responseDoc;
    Json::String err;
    const char * responseRawJson = responseBody.c_str();

    if (!reader->parse(responseRawJson, responseRawJson + strlen(responseRawJson), &responseDoc, &err))
    {
        ChipLogError(Controller, "JSON parse error: %s", err.c_str());
        return CHIP_ERROR_INTERNAL;
    }

    const Json::Value & chain = responseDoc["certificateChain"];
    VerifyOrReturnError(chain.isString(), CHIP_ERROR_INTERNAL);

    pkcs7NOCOut = chain.asString();

    return CHIP_NO_ERROR;
}

std::string CertifierOperationalCredentialsIssuer::GetAuthToken()
{
    g_autoptr(BCoreTokenProvider) tokenProvider = deviceServiceConfigurationGetTokenProvider();

    std::string retVal = "";
    if (tokenProvider)
    {
        g_autoptr(GError) error = nullptr;
        g_autofree gchar *token =
            b_core_token_provider_get_token(tokenProvider, B_CORE_TOKEN_TYPE_XPKI_MATTER, &error);

        if (error)
        {
            icError("Failed to get auth token: [%d] - %s", error->code, error->message);
        }
        else if (token)
        {
            retVal = token;
        }
    }

    return retVal;
}

CertifierOperationalCredentialsIssuer::ApiEnv
CertifierOperationalCredentialsIssuer::GetIssuerApiEnvFromString(std::string operationalEnv)
{
    CertifierOperationalCredentialsIssuer::ApiEnv apiEnv = CertifierOperationalCredentialsIssuer::ApiEnv::prod;

    if (operationalEnv == OPERATIONAL_ENVIRONMENT_STRING_PROD)
    {
        apiEnv = CertifierOperationalCredentialsIssuer::ApiEnv::prod;
    }
    else if (operationalEnv == OPERATIONAL_ENVIRONMENT_STRING_STAGE)
    {
        apiEnv = CertifierOperationalCredentialsIssuer::ApiEnv::stage;
    }

    return apiEnv;
}

void CertifierOperationalCredentialsIssuer::SetOperationalCredsIssuerApiEnv()
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    g_autofree char *propValue = b_core_property_provider_get_property_as_string(
        propertyProvider, DEVICE_PROP_MATTER_OPERATIONAL_ENVIRONMENT, DEFAULT_OPERATIONAL_ENVIRONMENT);
    std::string envString(propValue);
    mApiEnv = GetIssuerApiEnvFromString(envString);
}
