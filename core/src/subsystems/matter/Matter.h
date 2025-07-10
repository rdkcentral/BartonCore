//------------------------------ tabstop = 4 ----------------------------------
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
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 10/6/21.
 */

#pragma once

#include "AccessRestrictionProvider.h"
#include "BartonOperationalCredentialsDelegate.hpp"
#include "BartonCommissionableDataProvider.hpp"
#include "DeviceInfoProviderImpl.h"
#include "IcLogger.hpp"
#include "OTAProviderImpl.h"
#include "PersistentStorageDelegate.h"
#include "SessionMessageHandler.hpp"
#include "credentials/attestation_verifier/FileAttestationTrustStore.h"
#include "crypto/CHIPCryptoPAL.h"
#include "lib/core/CHIPVendorIdentifiers.hpp"
#include <app/server/Server.h>
#include <controller/CHIPDeviceController.h>
#include <controller/ExampleOperationalCredentialsIssuer.h>
#include <credentials/GroupDataProviderImpl.h>
#include <credentials/attestation_verifier/DeviceAttestationDelegate.h>
#include <platform/Linux/CHIPLinuxStorage.h>
#include <thread>

#include <lib/support/PersistedCounter.h>

#define CHIP_NUM_EVENT_LOGGING_BUFFERS 3

namespace barton
{
    /**
     * The Matter class is a purely asynchronous interface to the Matter stack.  None of the calls should block for
     * work.
     */
    class Matter : public chip::Credentials::DeviceAttestationDelegate
    {
    public:
        static Matter &GetInstance()
        {
            static Matter instance;
            return instance;
        }

        /**
         * Initialize the Matter stack interface.  Must be called before other APIs.
         * Note: the account ID must be unique under the related CA as it is used as
         *       the fabric ID.
         *
         * @param accountId the active account ID
         * @return true upon success
         */
        bool Init(uint64_t accountId, std::string &&attestationTrustStorePath);

        /**
         * Start the Matter interface.
         * @return true upon success
         */
        bool Start();

        /**
         * Stop the Matter interface.
         * @return true upon success
         */
        bool Stop();

        static CHIP_ERROR ParseSetupPayload(const std::string &codeString, chip::SetupPayload &payload);

        // TODO : yuck, we shouldnt hand this out publicly
        chip::Controller::DeviceCommissioner &GetCommissioner() { return *commissionerController; }

        chip::NodeId GetNodeId() { return myNodeId; }

        static chip::NodeId GetRandomOperationalNodeId();

        CHIP_ERROR GetCommissioningParams(chip::Controller::CommissioningParameters &params);

        /**
         * @brief Have Matter fetch and retain a new authorization token. Intended to be called before a
         *        matter operation that requires a new operational certificate (i.e. commissioning).
         *        Tokens are short-lived, so a new one should be used each operation.
         *
         * @note This is a potentially long-running operation.
         * @note This method is NOT thread-safe.
         *
         * @return true when a new token was successfully fetched and registered with our operational credentials
         * issuer.
         */
        bool PrimeNewAuthorizationToken();

        ////////////// DeviceAttestationDelegate overrides ////////////////////
        Optional<uint16_t> FailSafeExpiryTimeoutSecs() const override { return {}; }

        /*
         * Only used in development mode. SDK follows standard procedure in production mode.
         * Only called on attestation failure.
         */
        void
        OnDeviceAttestationCompleted(Controller::DeviceCommissioner *deviceCommissioner,
                                     DeviceProxy *device,
                                     const chip::Credentials::DeviceAttestationVerifier::AttestationDeviceInfo &info,
                                     chip::Credentials::AttestationVerificationResult attestationResult) override;
        ////////////// DeviceAttestationDelegate overrides ////////////////////

        /**
         * @brief Get the current IPK for the fabric owned by this controller
         *
         * @return chip::Crypto::AesCcm128Key A valid IPK or (unlikely) nullptr on failure
         */
        std::unique_ptr<chip::Crypto::IdentityProtectionKey> GetCurrentIPK();

        /**
         * Describe a version as a hexadecimal string, with zero padding
         * @param version
         * @return std::string
         */
        static inline std::string VersionToString(uint32_t version)
        {
            std::ostringstream tmp;

            tmp << "0x" << std::setfill('0') << std::setw(sizeof(uint32_t) * 2) << std::hex << version;
            return tmp.str();
        }

        CHIP_ERROR AccessControlDump(const chip::Access::AccessControl::Entry &entry);

        /**
         * @brief Open a commissioning window locally or for a specific device. When successful, the
         *        generated setup code and QR code are returned.  The caller is responsible for
         *        freeing the setupCode and qrCode.
         *
         * @param nodeId the nodeId of the device to open the commissioning window for, or NULL for local
         * @param timeoutSeconds the number of seconds to perform discovery before automatically stopping or 0 for default
         * @param setupCode receives the setup code if successful
         * @param qrCode receives the QR code if successful
         * @return true on success
         */
        bool OpenCommissioningWindow(chip::NodeId nodeId, uint16_t timeoutSecs, std::string &setupCode, std::string &qrCode);

        /**
         * @brief Clear the AccessRestrictionList for certification testing
         *
         * @return true on success
         */
        bool ClearAccessRestrictionList();

        /**
         * @brief Get the endpoint id for "this" node that hosts utility servers (Basic Information, OTA provider, etc)
         *
         * @return chip::EndpointId
         */
        inline chip::EndpointId GetLocalEndpointId() { return localEndpointId; }

        /**
         * @brief Get our fabric index
         *
         * @return chip::FabricIndex
         */
        inline chip::FabricIndex GetFabricIndex() { return myFabricIndex; }

        inline bool IsRunning() { return state == State::running; }

    private:
        static Matter *instance;
        IcLogger logger;

        Matter();
        ~Matter() override;
        CHIP_ERROR InitCommissioner();
        chip::DeviceProxy *ConnectDevice(chip::NodeId nodeId);
        static bool isQRCode(const std::string &codeString);
        void StackThreadProc();
        static chip::NodeId LoadOrGenerateLocalNodeId();
        CHIP_ERROR ConfigureOTAProviderNode();
        bool IsAccessibleByOTARequestors();
        CHIP_ERROR AppendOTARequestorsACLEntry(chip::FabricId fabricId, chip::FabricIndex fabricIndex);

        bool IsDevelopmentMode()
        {
            // TODO: fill in with something like a call to deviceService
            return true;
        }

        const chip::Credentials::AttestationTrustStore &GetAttestationTrustStore()
        {
            static chip::Credentials::FileAttestationTrustStore productionAttestationRootStore {
                paaTrustStorePath.c_str()};

            return productionAttestationRootStore;
        }

        /**
         * Commissioner bootstrap certificates. Load the real operational certificates
         * from storage, if available. The private keys are owned by Matter persistent storage, and
         * though the operational certificate store also has this chain, the SDK uses information
         * in the certificates to locate the fabric, the index of which is the locator for all keys/cert chains.
         * @param rCA The root CA. CHIP_ERROR_NOT_FOUND is returned if this is not in storage,
         *            and means a new chain should be generated.
         * @param iCA The intermediate CA. Optional, and may be empty (size == 0) if not in storage.
         * @param NOC The Node Operational Certificate. CHIP_ERROR_NOT_FOUND is returned if not in storage,
         *            and means a new chain should be generated.
         * @return CHIP_ERROR_NOT_FOUND the chain isn't in storage. Check for ChipError::IsSuccess otherwise.
         */
        CHIP_ERROR LoadNOCChain(MutableByteSpan &rCA, MutableByteSpan &iCA, MutableByteSpan &NOC);

        /**
         * Save a NOC chain for resuming the commissioner later.
         * @param rCA The root CA, to store in Distinguished Encoding Rules form
         * @param iCA Optional. Provide zero-length span if no ICA is to be stored
         * @param NOC The Node Operational Certificate in Distinguished Encoding Rules form
         * @return Check for errors with ChipError::IsSuccess
         */
        CHIP_ERROR SaveNOCChain(ByteSpan rCA, ByteSpan iCA, ByteSpan NOC);

        /**
         * Get the fabric index that matches a Node Operational Certificate
         * signed by a given root certificate authority
         * @param fabricTable The table to search
         * @param mutliControllerAllowed Set to true if the fabric supports multiple controllers
         * @param rCA The X.509 root certificate (Certificate Authority),
         *            in Distinguished Encoding Rules form
         *
         * @param NOC The X.509 Node Operational Certificate that holds the fabric ID to search
         *            for, in Distinguished Encoding Rules form
         *
         * @param [Out] idxOut will receive chip::kUndefinedFabricIndex when not found, or a valid fabric index
         * @return CHP_NO_ERROR, or an SDK error (e.g., bad certificate data).
         * @note idxOut is undefined when an error is indicated.
         */
        CHIP_ERROR GetFabricIndex(FabricTable *fabricTable,
                                  bool mutliControllerAllowed,
                                  ByteSpan rCA,
                                  ByteSpan NOC,
                                  FabricIndex &idxOut);

        /**
         * @brief Idemptotently set the single epoch Identity Protection Key
         *        for the fabric.
         *
         * @ref Matter R1.0 4.13.2.6, 4.15.3
         * @return CHIP_ERROR either CHIP_NO_ERROR or an SDK error on failure
         */
        CHIP_ERROR SetIPKOnce();

        /**
         * Configure our access restrictions.  This includes setting the CommissioningARL attribute as
         * well as setting the current ARL entries for each fabric.
         *
         * @return true on success
         */
        bool SetAccessRestrictionList();

        bool OpenLocalCommissioningWindow(uint16_t discriminator,
                                          uint16_t timeoutSecs,
                                          SetupPayload &setupPayload);

        bool serverIsInitialized = false;

        std::thread *stackThread {};
        chip::NodeId myNodeId = kUndefinedNodeId;
        chip::FabricId myFabricId {};
        chip::FabricIndex myFabricIndex = kUndefinedFabricIndex;
        // defined by barton-library.zap.
        static constexpr chip::EndpointId localEndpointId = 0;
        std::unique_ptr<std::vector<uint8_t>> threadOperationalDataset;
        std::unique_ptr<chip::Controller::DeviceCommissioner> commissionerController;

        PersistentStorageDelegate storageDelegate;

        std::shared_ptr<BartonOperationalCredentialsDelegate> operationalCredentialsIssuer;

        std::shared_ptr<BartonCommissionableDataProvider> commissionableDataProvider;

        std::unique_ptr<chip::PersistentStorageOperationalKeystore> operationalKeystore;
        std::unique_ptr<chip::Credentials::PersistentStorageOpCertStore> opCertStore;
        chip::Crypto::DefaultSessionKeystore sessionKeystore;
        chip::CommonCaseDeviceServerInitParams serverInitParams;
        DeviceInfoProviderImpl deviceInfoProvider;
        chip::Credentials::GroupDataProviderImpl groupDataProvider;
        char *wifiSsid {};
        char *wifiPass {};

        uint8_t infoEventBuffer[CHIP_DEVICE_CONFIG_EVENT_LOGGING_INFO_BUFFER_SIZE];
        uint8_t debugEventBuffer[CHIP_DEVICE_CONFIG_EVENT_LOGGING_DEBUG_BUFFER_SIZE];
        uint8_t critEventBuffer[CHIP_DEVICE_CONFIG_EVENT_LOGGING_CRIT_BUFFER_SIZE];
        chip::PersistedCounter<chip::EventNumber> globalEventIdCounter;
        chip::app::CircularEventBuffer loggingBuffer[CHIP_NUM_EVENT_LOGGING_BUFFERS];

        static constexpr chip::EndpointId otaProviderEndpointId = 0;
        OTAProviderImpl otaProvider;

        std::string paaTrustStorePath;

        enum State
        {
            running,
            stopping,
            stopped
        };

        std::atomic<State> state = {State::stopped};

        SessionMessageHandler sessionMessageHandler;
        AccessRestrictionProvider accessRestrictionProvider;
    };
} // namespace barton
