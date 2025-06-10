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

#include "CertifierDACProvider.hpp"

#include <crypto/CHIPCryptoPAL.h>

#include <lib/core/CHIPError.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>

#include <certifier/security.h>
#include <memory>

#include "BartonMatterProviderRegistry.hpp"

extern "C" {
    #include "deviceServiceConfiguration.h"
    #include "deviceServiceProps.h"
    #include "provider/device-service-property-provider.h"
}

using namespace barton;

namespace {
    CHIP_ERROR LoadKeypairFromRaw(ByteSpan private_key, ByteSpan public_key, Crypto::P256Keypair & keypair)
    {
        Crypto::P256SerializedKeypair serialized_keypair;
        ReturnErrorOnFailure(serialized_keypair.SetLength(private_key.size() + public_key.size()));
        memcpy(serialized_keypair.Bytes(), public_key.data(), public_key.size());
        memcpy(serialized_keypair.Bytes() + public_key.size(), private_key.data(), private_key.size());
        return keypair.Deserialize(serialized_keypair);
    }

    std::shared_ptr<CertifierDACProvider> dacProvider = std::make_shared<CertifierDACProvider>();
    auto providerRegistered = BartonMatterProviderRegistry::Instance().RegisterBartonDACProvider(dacProvider);
}

CHIP_ERROR CertifierDACProvider::GetDeviceAttestationCert(MutableByteSpan & out_dac_buffer)
{
    X509_CERT * cert     = nullptr;
    CertifierError error = CERTIFIER_ERROR_INITIALIZER;

    std::string dacFilePath = GetDACFilepath();
    std::string dacPassword = GetDACPassword();
    error = security_get_X509_PKCS12_file(dacFilePath.c_str(), dacPassword.c_str(), nullptr, &cert, nullptr);
    VerifyOrReturnError(error.application_error_code == 0 && error.library_error_code == 0, CHIP_ERROR_INTERNAL);

    size_t der_len      = 0;
    unsigned char * der = security_X509_to_DER(cert, &der_len);
    VerifyOrReturnError(der != nullptr, CHIP_ERROR_INTERNAL);

    CopySpanToMutableSpan(ByteSpan(der, der_len), out_dac_buffer);

    XFREE(der);
    security_free_cert(cert);

    return CHIP_NO_ERROR;
}

CHIP_ERROR CertifierDACProvider::GetProductAttestationIntermediateCert(MutableByteSpan & out_pai_buffer)
{
    X509_LIST * certs    = nullptr;
    X509_CERT * cert     = nullptr;
    CertifierError error = CERTIFIER_ERROR_INITIALIZER;

    certs = security_new_cert_list();
    VerifyOrReturnError(certs != nullptr, CHIP_ERROR_INTERNAL);

    std::string dacFilePath = GetDACFilepath();
    std::string dacPassword = GetDACPassword();
    error = security_get_X509_PKCS12_file(dacFilePath.c_str(), dacPassword.c_str(), certs, nullptr, nullptr);
    VerifyOrReturnError(error.application_error_code == 0 && error.library_error_code == 0, CHIP_ERROR_INTERNAL);

    cert = security_cert_list_get(certs, 1);

    size_t der_len      = 0;
    unsigned char * der = security_X509_to_DER(cert, &der_len);
    VerifyOrReturnError(der != nullptr, CHIP_ERROR_INTERNAL);

    CopySpanToMutableSpan(ByteSpan(der, der_len), out_pai_buffer);

    XFREE(der);
    security_free_cert(cert);
    security_free_cert_list(certs);

    return CHIP_NO_ERROR;
}

CHIP_ERROR CertifierDACProvider::GetCertificationDeclaration(MutableByteSpan & out_cd_buffer)
{
    // TODO: We don't yet have CMS for CDs (issued by matter upon certification - Matter spec 1.4 section 6.3)
    out_cd_buffer.reduce_size(0);

    return CHIP_NO_ERROR;
}

CHIP_ERROR CertifierDACProvider::GetFirmwareInformation(MutableByteSpan & out_firmware_info_buffer)
{
    // TODO: We need a real example FirmwareInformation to be populated.
    out_firmware_info_buffer.reduce_size(0);

    return CHIP_NO_ERROR;
}

CHIP_ERROR CertifierDACProvider::SignWithDeviceAttestationKey(const ByteSpan & message_to_sign,
                                                              MutableByteSpan & out_signature_buffer)
{
    Crypto::P256ECDSASignature signature;
    Crypto::P256Keypair keypair;

    VerifyOrReturnError(!out_signature_buffer.empty(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(!message_to_sign.empty(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(out_signature_buffer.size() >= signature.Capacity(), CHIP_ERROR_BUFFER_TOO_SMALL);

    X509_CERT * cert     = nullptr;
    ECC_KEY * key        = nullptr;
    CertifierError error = CERTIFIER_ERROR_INITIALIZER;

    std::string dacFilePath = GetDACFilepath();
    std::string dacPassword = GetDACPassword();
    error = security_get_X509_PKCS12_file(dacFilePath.c_str(), dacPassword.c_str(), nullptr, &cert, &key);
    VerifyOrReturnError(error.application_error_code == 0 && error.library_error_code == 0, CHIP_ERROR_INTERNAL);

    uint8_t raw_public_key[65]  = { 0 };
    uint8_t raw_private_key[32] = { 0 };
    size_t raw_public_key_len   = security_serialize_raw_public_key(key, raw_public_key, sizeof(raw_public_key));
    size_t raw_private_key_len  = security_serialize_raw_private_key(key, raw_private_key, sizeof(raw_private_key));
    VerifyOrReturnError(raw_public_key_len == sizeof(raw_public_key), CHIP_ERROR_INTERNAL);
    VerifyOrReturnError(raw_private_key_len == sizeof(raw_private_key), CHIP_ERROR_INTERNAL);

    ReturnErrorOnFailure(LoadKeypairFromRaw(ByteSpan{ raw_private_key, raw_private_key_len },
                                            ByteSpan{ raw_public_key, raw_public_key_len }, keypair));

    security_free_eckey(key);
    security_free_cert(cert);

    ReturnErrorOnFailure(keypair.ECDSA_sign_msg(message_to_sign.data(), message_to_sign.size(), signature));

    return CopySpanToMutableSpan(ByteSpan{ signature.ConstBytes(), signature.Length() }, out_signature_buffer);
}

std::string CertifierDACProvider::GetDACFilepath()
{
    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    g_autofree char *dacFilePath = b_device_service_property_provider_get_property_as_string(
        propertyProvider, DEVICE_PROP_MATTER_DAC_P12_PATH, "");

    return std::string(dacFilePath);
}

std::string CertifierDACProvider::GetDACPassword()
{
    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    g_autofree char *dacPassword = b_device_service_property_provider_get_property_as_string(
        propertyProvider, DEVICE_PROP_MATTER_DAC_P12_PASSWORD, "");

    return std::string(dacPassword);
}
