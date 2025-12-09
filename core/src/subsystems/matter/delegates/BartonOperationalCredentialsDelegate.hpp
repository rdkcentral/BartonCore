// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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

//
// Created by Christian Leithner on 3/31/2025.
//

#pragma once

#include "credentials/CHIPCert.h"
#include "lib/core/CHIPError.h"
#include <controller/OperationalCredentialsDelegate.h>
#include <iomanip>
#include <lib/core/TLV.h>
#include <platform/PlatformManager.h>

#include <string>
#include <thread>

extern "C" {
    #include <openssl/x509.h>
}

using namespace chip;
using namespace Controller;
using namespace TLV;
using namespace Platform;

namespace barton
{

    /**
     * This class is the common parent for implementors of the OperationalCredentialsDelegate.
     * It provides some common code for operational credentials implementations. Subclasses should
     * implement the GenerateNOCChainAfterValidation method to perform the actual NOC generation.
     */
    class BartonOperationalCredentialsDelegate : public chip::Controller::OperationalCredentialsDelegate
    {
        public:
            CHIP_ERROR GenerateNOCChain(const ByteSpan & csrElements, const ByteSpan & csrNonce,
                                                            const ByteSpan & attestationSignature,
                                                            const ByteSpan & attestationChallenge, const ByteSpan & DAC,
                                                            const ByteSpan & PAI,
                                                            Callback::Callback<OnNOCChainGeneration> * onCompletion) override
            {
                ChipLogProgress(Controller, "Verifying Certificate Signing Request");

                if (!mIPK.HasValue())
                {
                    ChipLogError(NotSpecified, "No IPK set! SetIPKForNextNOCRequest prior to requesting a NOC");
                    return CHIP_ERROR_INCORRECT_STATE;
                }

                TLVReader reader;
                reader.Init(csrElements);

                if (reader.GetType() == kTLVType_NotSpecified)
                {
                    ReturnErrorOnFailure(reader.Next());
                }

                VerifyOrReturnError(reader.GetType() == kTLVType_Structure, CHIP_ERROR_WRONG_TLV_TYPE);
                VerifyOrReturnError(reader.GetTag() == AnonymousTag(), CHIP_ERROR_UNEXPECTED_TLV_ELEMENT);

                TLVType containerType;
                ReturnErrorOnFailure(reader.EnterContainer(containerType));
                ReturnErrorOnFailure(reader.Next(kTLVType_ByteString, TLV::ContextTag(1)));

                ByteSpan csrFromElements;
                ReturnErrorOnFailure(reader.Get(csrFromElements));

                ReturnErrorOnFailure(reader.Next(kTLVType_ByteString, TLV::ContextTag(2)));

                ByteSpan nonceFromElements;
                ReturnErrorOnFailure(reader.Get(nonceFromElements));

                VerifyOrReturnError(nonceFromElements.size() == kCSRNonceLength, CHIP_ERROR_BUFFER_TOO_SMALL);
                VerifyOrReturnError(memcmp(nonceFromElements.data(), csrNonce.data(), kCSRNonceLength) == 0, CHIP_ERROR_INTERNAL);

                reader.ExitContainer(containerType);

                ScopedMemoryBufferWithSize<uint8_t> csrClone;
                ScopedMemoryBufferWithSize<uint8_t> nonceClone;

                VerifyOrReturnError(csrClone.Alloc(csrFromElements.size()), CHIP_ERROR_NO_MEMORY);
                VerifyOrReturnError(nonceClone.Alloc(nonceFromElements.size()), CHIP_ERROR_NO_MEMORY);

                memcpy(csrClone.Get(), csrFromElements.data(), csrFromElements.size());
                memcpy(nonceClone.Get(), nonceFromElements.data(), nonceFromElements.size());

                std::thread fetcher(
                    [this, csrBuf = std::move(csrClone), nonceBuf = std::move(nonceClone), onCompletion](NodeId nodeId, FabricId fabricId) {
                        // FIXME: forward errors to the callback
                        Platform::ScopedMemoryBuffer<uint8_t> noc;
                        VerifyOrReturnError(noc.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
                        MutableByteSpan nocSpan(noc.Get(), kMaxCHIPDERCertLength);

                        Platform::ScopedMemoryBuffer<uint8_t> icac;
                        VerifyOrReturnError(icac.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
                        MutableByteSpan icacSpan(icac.Get(), kMaxCHIPDERCertLength);

                        Platform::ScopedMemoryBuffer<uint8_t> rcac;
                        VerifyOrReturnError(rcac.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
                        MutableByteSpan rcacSpan(rcac.Get(), kMaxCHIPDERCertLength);

                        ByteSpan csr(csrBuf.Get(), csrBuf.AllocatedSize());
                        ByteSpan nonce(nonceBuf.Get(), csrBuf.AllocatedSize());

                        CHIP_ERROR fetchErr = GenerateNOCChainAfterValidation(nodeId, fabricId, csr, nonce, rcacSpan, icacSpan, nocSpan);

                        if (fetchErr == CHIP_NO_ERROR && nocSpan.size() > 0)
                        {
                            chip::Platform::ScopedMemoryBuffer<uint8_t> chipNoc;
                            VerifyOrReturnError(chipNoc.Alloc(chip::Credentials::kMaxCHIPCertLength),
                                                CHIP_ERROR_NO_MEMORY);
                            chip::MutableByteSpan chipNocSpan(chipNoc.Get(), chip::Credentials::kMaxCHIPCertLength);

                            // Do the same verification checks on the received NOC that Matter is going to do.
                            fetchErr = Credentials::ConvertX509CertToChipCert(nocSpan, chipNocSpan);
                        }

                        {
                            DeviceLayer::StackLock lock;

                            onCompletion->mCall(onCompletion->mContext, fetchErr, nocSpan, icacSpan, rcacSpan,
                                                MakeOptional(mIPK.Value().Span()), Optional<NodeId>());
                        }

                        return CHIP_NO_ERROR;
                    },
                    mNodeId, mFabricId);

                fetcher.detach();

                return CHIP_NO_ERROR;
            }

            /**
             * @brief Perform NOC chain generation. Intended to be called by GenerateNOCChain
             *        which will do validation on input before calling this method.
             *
             * @param[in] nodeId The Node ID of the device receiving node operational credentials.
             * @param[in] fabricId The Fabric ID that the node is being commissioned into.
             * @param[in] csr The Certificate Signing Request (CSR)
             * @param[in] nonce The CSR nonce
             * @param[out] rcac The root CA certificate
             * @param[out] icac The intermediate CA certificate (optional)
             * @param[out] noc The node operational certificate
             */
            virtual CHIP_ERROR GenerateNOCChainAfterValidation(NodeId nodeId,
                FabricId fabricId,
                const ByteSpan &csr,
                const ByteSpan &nonce,
                MutableByteSpan &rcac,
                MutableByteSpan &icac,
                MutableByteSpan &noc) = 0;

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

            void SetNodeIdForNextNOCRequest(NodeId nodeId) override { mNodeId = nodeId; }

            void SetFabricIdForNextNOCRequest(FabricId fabricId) override { mFabricId = fabricId; }

        protected:
            CHIP_ERROR PutX509(const X509 * cert, MutableByteSpan & outCert)
            {
                CHIP_ERROR err = CHIP_NO_ERROR;

                unsigned char * tmp = NULL;
                size_t cert_len     = i2d_X509((X509 *) cert, &tmp);

                if (outCert.size() >= cert_len)
                {
                    err = CopySpanToMutableSpan(ByteSpan(tmp, cert_len), outCert);
                }
                else
                {
                    err = CHIP_ERROR_BUFFER_TOO_SMALL;
                }

                OPENSSL_free(tmp);

                return err;
            }

            /**
             * @brief Makes a string that represents a number in hex, with leading zeroes.
             *
             * @tparam T A numeric type (int, uint64_t, ...)
             * @param number
             * @return std::string
             * TODO: Use c++20 'concept' to constrain T to numeric.
             */
            template <typename T> inline std::string ToHexString(T number)
            {
                std::stringstream os;

                // Matter requires uppercase hex for ID attributes; xPKI wants no radix indicator (0x).
                os << std::uppercase << std::hex << std::setfill('0') << std::setw(sizeof(T) * 2) << std::right << number;

                return os.str();
            }

        private:
            NodeId mNodeId;
            FabricId mFabricId;
            chip::Optional<chip::Crypto::IdentityProtectionKey> mIPK;

    };

}
