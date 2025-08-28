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

#include "app-common/zap-generated/ids/Clusters.h"
#include "app/server/CommissioningWindowManager.h"
#include "lib/core/Optional.h"
#include "platform/DeviceInstanceInfoProvider.h"
#include "system/SystemClock.h"
#include <cstdint>
#include <vector>
#define LOG_TAG     "Matter"
#define logFmt(fmt) "(%s): " fmt, __func__

#include <glib-object.h>
#include <libxml/parser.h>

extern "C" {
#include "deviceServiceConfiguration.h"
#include "deviceServiceProperties.h"
#include "icUtil/fileUtils.h"
#include "provider/barton-core-property-provider.h"
#include "provider/barton-core-token-provider.h"
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/base64.h>

#ifdef BARTON_CONFIG_THREAD
#include <subsystems/thread/threadSubsystem.h>
#endif
}

#include CHIP_PROJECT_CONFIG_INCLUDE
#include <app/clusters/ota-provider/ota-provider.h>
#include <app/clusters/thread-border-router-management-server/thread-border-router-management-server.h>
#include <app/clusters/thread-network-directory-server/thread-network-directory-server.h>
#include <app/clusters/wifi-network-management-server/wifi-network-management-server.h>
#include <app/server/Dnssd.h>
#include <controller/CHIPDeviceController.h>
#include <controller/CHIPDeviceControllerFactory.h>
#include <controller/CommissioningWindowOpener.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/FabricTable.h>
#include <credentials/attestation_verifier/DefaultDeviceAttestationVerifier.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CodeUtils.h>
#include <messaging/ExchangeMgr.h>
#include <platform/Linux/ConfigurationManagerImpl.h>
#include <platform/OpenThread/GenericThreadBorderRouterDelegate.h>
#include <protocols/secure_channel/RendezvousParameters.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>
#include <transport/SessionMessageDelegate.h>

#include <lib/support/TestGroupData.h>

#include <lib/core/NodeId.h>
#include <matter/MatterDriverFactory.h>

#include "BartonDeviceInstanceInfoProvider.hpp"
#include "BartonMatterDelegateRegistry.hpp"
#include "BartonMatterProviderRegistry.hpp"
#include "Matter.h"
#include "credentials/attestation_verifier/DeviceAttestationVerifier.h"

#include "AccessControlDelegate.h"
#include "ThreadBorderRouterManagementDelegate.h"

#define CONNECT_DEVICE_TIMEOUT_SECONDS          15
#define DISCOVER_ON_NETWORK_DEVICE_TIMEOUT_SECS 1

#define BLE_CONTROLLER_ADAPTER_ID               0
#define BLE_CONTROLLER_DEVICE_NAME              BARTON_CONFIG_MATTER_BLE_CONTROLLER_DEVICE_NAME

#define LOCAL_NODE_ID_SYSTEM_PROPERTY_NAME      "localMatterNodeId"
#define MATTER_RCA_SYSTEM_PROPERTY_NAME         "matterRCA"
#define MATTER_ICA_SYSTEM_PROPERTY_NAME         "matterICA"
#define MATTER_NOC_SYSTEM_PROPERTY_NAME         "matterNOC"

#define KV_STORAGE_NAMESPACE                    "matterkv"
#define MATTER_CONFIG_DIR                       "matter"

#ifdef BARTON_CONFIG_USE_DEFAULT_COMMISSIONABLE_DATA
#define DEVELOPMENT_DISCRIMINATOR CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR
#define DEVELOPMENT_PASSCODE      20202021 // This is the test setup pin used by the Matter SDK
#define DEVELOPMENT_SALT          "TXkgVGVzdCBLZXkgU2FsdA=="
#define DEVELOPMENT_VERIFIER                                                                                           \
    "yyTJb+3cnmKd+GYlvuGaRw6SD0YWRNSZkDOfh5WYUjEEUnFQ6a2d4m7EtQtdw7OomtGlK2FF13Quxm5zS"                                \
    "Tdr/lPQNKez/7vJ+Sh/YBZoFQTdKMS9QUKkVz4Vfc1gjbkkPA=="
#endif

/*
 * The SDK's example apps arbitrarily use +12 as a way to differentiate the commissioner port from the server one.
 * That should be good enough for us at the moment.
 */
#define COMMISSIONER_CHIP_PORT CHIP_PORT + 12

using namespace std;
using namespace ::barton;
using namespace std::chrono_literals;
using namespace chip;
using namespace chip::Crypto;
using namespace chip::app::Clusters;

#include <credentials/attestation_verifier/FileAttestationTrustStore.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>

static constexpr uint16_t kMaxGroupsPerFabric = 5;
static constexpr uint16_t kMaxGroupKeysPerFabric = 8;
static constexpr uint16_t kMaxDiscriminator = 4096;
static constexpr uint16_t kDefaultPAKEIterationCount = chip::Crypto::kSpake2p_Min_PBKDF_Iterations;

namespace
{
    std::optional<DefaultThreadNetworkDirectoryServer> threadNetworkDirectoryServer;
    EndpointId threadNetworkDirectoryServerEndpointId = UINT16_MAX;
    std::optional<WiFiNetworkManagementServer> wifiNetworkManagementServer;
    EndpointId wifiNetworkManagementServerEndpointId = UINT16_MAX;
    std::unique_ptr<ThreadBorderRouterManagementDelegate> otbrDelegate;
    std::optional<ThreadBorderRouterManagement::ServerInstance> threadBorderRouterManagementServer;
    EndpointId threadBorderRouterManagementServerEndpointId = UINT16_MAX;
} // namespace

Matter::Matter() : groupDataProvider(kMaxGroupsPerFabric, kMaxGroupKeysPerFabric)
{
    MatterDriverFactory::Instance();

    commissionerController = std::make_unique<chip::Controller::DeviceCommissioner>();
    operationalCredentialsIssuer = BartonMatterDelegateRegistry::Instance().GetBartonOperationalCredentialDelegate();
    commissionableDataProvider = BartonMatterProviderRegistry::Instance().GetBartonCommissionableDataProvider();
}

Matter::~Matter()
{
    free(wifiSsid);
    free(wifiPass);
}

void EventHandler(const DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
    (void) arg;
    icDebug("EventType=%" PRIx16, event->Type);
}

bool Matter::Init(uint64_t accountId, std::string &&attestationTrustStorePath)
{
    icDebug();

    bool result = true;

    CHIP_ERROR err = CHIP_NO_ERROR;

    myNodeId = LoadOrGenerateLocalNodeId();
    if (!IsOperationalNodeId(myNodeId))
    {
        icError("Failed to load or generate local node ID");
        return false;
    }
    icDebug("Local node ID: 0x%" PRIx64, myNodeId);

    mkdir_p(CHIP_BARTON_CONF_DIR, 0700);

    myFabricId = accountId;

    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    paaTrustStorePath = std::string(std::move(attestationTrustStorePath));

    if ((err = chip::Platform::MemoryInit()) != CHIP_NO_ERROR)
    {
        icError("MemoryInit failed: %s", err.AsString());
        return false;
    }

    if ((err = chip::DeviceLayer::PlatformMgr().InitChipStack()) != CHIP_NO_ERROR)
    {
        icError("InitChipStack failed: %s", err.AsString());
        return false;
    }

    if (!GetBartonDeviceInstanceInfoProvider().ValidateProperties())
    {
        icError("BartonDeviceInstanceInfoProvider properties validation failed");
        return false;
    }
    DeviceLayer::SetDeviceInstanceInfoProvider(&GetBartonDeviceInstanceInfoProvider());

    // We assign new objects here just to guarantee that we're working with uninitialized objects
    opCertStore = std::make_unique<chip::Credentials::PersistentStorageOpCertStore>();
    err = opCertStore->Init(&storageDelegate);

    if (!ChipError::IsSuccess(err))
    {
        icError("Operational cert store init failed: %s", err.AsString());
        return false;
    }

    operationalKeystore = std::make_unique<chip::PersistentStorageOperationalKeystore>();
    err = operationalKeystore->Init(&storageDelegate);

    if (!ChipError::IsSuccess(err))
    {
        icError("Operational keystore init failed: %s", err.AsString());
        return false;
    }

#ifdef BARTON_CONFIG_USE_DEFAULT_COMMISSIONABLE_DATA
    b_core_property_provider_set_property_uint16(
        propertyProvider, B_CORE_BARTON_MATTER_SETUP_DISCRIMINATOR, DEVELOPMENT_DISCRIMINATOR);

    b_core_property_provider_set_property_uint32(
        propertyProvider, B_CORE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT, kDefaultPAKEIterationCount);

    b_core_property_provider_set_property_uint32(
        propertyProvider, B_CORE_BARTON_MATTER_SETUP_PASSCODE, DEVELOPMENT_PASSCODE);

    b_core_property_provider_set_property_string(
        propertyProvider, B_CORE_BARTON_MATTER_SPAKE2P_SALT, DEVELOPMENT_SALT);

    b_core_property_provider_set_property_string(
        propertyProvider, B_CORE_BARTON_MATTER_SPAKE2P_VERIFIER, DEVELOPMENT_VERIFIER);
#endif

    chip::DeviceLayer::SetCommissionableDataProvider(commissionableDataProvider.get());

    chip::DeviceLayer::ConfigurationMgr().LogDeviceConfig();

    if ((err = DeviceLayer::PlatformMgrImpl().AddEventHandler(EventHandler, 0)) != CHIP_NO_ERROR)
    {
        icError("AddEventHandler failed: %s", err.AsString());
        return false;
    }

    chip::DeviceLayer::ConnectivityMgr().SetBLEDeviceName(BLE_CONTROLLER_DEVICE_NAME);
    chip::DeviceLayer::Internal::BLEMgrImpl().ConfigureBle(BLE_CONTROLLER_ADAPTER_ID, true);
    chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false);

#if CHIP_ENABLE_OPENTHREAD
    if ((err = DeviceLayer::ThreadStackMgrImpl().InitThreadStack()) != CHIP_NO_ERROR)
    {
        icError("InitThreadStack failed: %s", err.AsString());
        return false;
    }

    otbrDelegate = std::make_unique<ThreadBorderRouterManagementDelegate>();
#endif // CHIP_ENABLE_OPENTHREAD

    return result;
}

bool Matter::Start()
{
    icDebug();

    CHIP_ERROR err;

    if (state.load() == State::stopping)
    {
        icWarn("QA: stop already in progress");
        return false;
    }

    if (state.load() == State::running)
    {
        icWarn("QA: already running");
        return true;
    }

    // If Server::Init succeeded in a previous attempt, we shouldn't perform these steps again
    if (!serverIsInitialized)
    {
        if ((err = serverInitParams.InitializeStaticResourcesBeforeServerInit()) != CHIP_NO_ERROR)
        {
            icError("InitializeStaticResourcesBeforeServerInit failed: %s", err.AsString());
            return false;
        }

        chip::Access::AccessControl::Delegate *aclDelegate = GetAccessControlDelegate();
        serverInitParams.accessDelegate = aclDelegate;

        // We need to set DeviceInfoProvider before Server::Init to setup the storage of DeviceInfoProvider properly.
        chip::DeviceLayer::SetDeviceInfoProvider(&deviceInfoProvider);

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
        serverInitParams.accessRestrictionProvider = &accessRestrictionProvider;
#endif

        if ((err = Server::GetInstance().Init(serverInitParams)) != CHIP_NO_ERROR)
        {
            icError("Server::Init failed: %s", err.AsString());
            return false;
        }

        serverIsInitialized = true;
    }

    // Now that the server has started and we are done with our startup logging, log our discovery/onboarding
    // information again so it's not lost in the noise.
    ConfigurationMgr().LogDeviceConfig();

    // Initialize device attestation config
    auto dacProvider = BartonMatterProviderRegistry::Instance().GetBartonDACProvider();
    SetDeviceAttestationCredentialsProvider(dacProvider.get());

    if ((err = InitCommissioner()) != CHIP_NO_ERROR)
    {
        icError("InitCommissioner failed: %s", err.AsString());
        return false;
    }

    state.store(State::running);

    SetAccessRestrictionList();

    stackThread = new std::thread(&Matter::StackThreadProc, this);

    //TODO this should also be called if/when the local wifi network is changed
    g_autoptr(GError) error = NULL;
    g_autoptr(BCoreNetworkCredentialsProvider) provider =
        deviceServiceConfigurationGetNetworkCredentialsProvider();
    g_autoptr(BCoreWifiNetworkCredentials) wifiCredentials =
        b_core_network_credentials_provider_get_wifi_network_credentials(provider, &error);

    if (error == NULL && wifiCredentials != NULL)
    {
        g_autofree gchar *ssid = NULL;
        g_autofree gchar *psk = NULL;
        g_object_get(wifiCredentials,
                     B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_SSID],
                     &ssid,
                     B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_PSK],
                     &psk,
                     NULL);

        chip::DeviceLayer::PlatformMgr().LockChipStack();
        wifiNetworkManagementServer->SetNetworkCredentials(ByteSpan(Uint8::from_const_char(ssid), strlen(ssid)),
                                                           ByteSpan(Uint8::from_const_char(psk), strlen(psk)));
        chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    }
    else
    {
        if (error != NULL)
        {
            icError("Failed to get WiFi credentials: [%d] %s", error->code, stringCoalesce(error->message));
        }
        else
        {
            icError("Failed to get WiFi credentials");
        }
    }

    return true;
}

// block until shutdown
void Matter::StackThreadProc()
{
    icDebug();

    // This blocks until Stop
    chip::DeviceLayer::PlatformMgr().RunEventLoop();

    chip::DeviceLayer::PlatformMgr().LockChipStack();

    auto &server = chip::Server::GetInstance();
    server.Shutdown();

    commissionerController->Shutdown();

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    icInfo("Shut down");
}

bool Matter::Stop()
{
    icDebug();

    state.store(State::stopping);

    chip::DeviceLayer::PlatformMgr().StopEventLoopTask();

    if (stackThread != nullptr)
    {
        stackThread->join();
    }

    // If/when Matter starts up again, we want to guarantee that this is uninitialized
    chip::Access::AccessControl &accessControl = chip::Access::GetAccessControl();
    accessControl.Finish();
    chip::Access::SetAccessControl(accessControl);

    state.store(State::stopped);

    return true;
}

CHIP_ERROR Matter::InitCommissioner()
{
    Controller::FactoryInitParams factoryParams;
    chip::Controller::SetupParams params;
    FabricTable *fabricTable = &Server::GetInstance().GetFabricTable();

    factoryParams.fabricIndependentStorage = &storageDelegate;
    factoryParams.fabricTable = fabricTable;
    factoryParams.sessionKeystore = &sessionKeystore;

    groupDataProvider.SetStorageDelegate(&storageDelegate);
    groupDataProvider.SetSessionKeystore(&sessionKeystore);
    factoryParams.groupDataProvider = &groupDataProvider;

#ifdef BARTON_CONFIG_MATTER_USE_RANDOM_PORT
    factoryParams.listenPort = 0;
#else
    factoryParams.listenPort = COMMISSIONER_CHIP_PORT;
#endif

    factoryParams.opCertStore = opCertStore.get();

    params.operationalCredentialsDelegate = operationalCredentialsIssuer.get();
    uint16_t controllerVendorId = 0;
    if(DeviceLayer::GetDeviceInstanceInfoProvider()->GetVendorId(controllerVendorId) == CHIP_NO_ERROR)
    {
        params.controllerVendorId = (VendorId)controllerVendorId;
    }
    params.permitMultiControllerFabrics = false;

    const chip::Credentials::AttestationTrustStore *paaRootStore;

    // The attestation trust store is file backed and populated with appropriate per-product roots.
    // dev build contains test+prod certificates and all others only trust production certs.
    chip::Credentials::DeviceAttestationVerifier *dacVerifier = GetDefaultDACVerifier(&GetAttestationTrustStore());
    dacVerifier->EnableCdTestKeySupport(IsDevelopmentMode());
    SetDeviceAttestationVerifier(dacVerifier);

    chip::Platform::ScopedMemoryBuffer<uint8_t> noc;
    VerifyOrReturnError(noc.Alloc(chip::Controller::kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    chip::MutableByteSpan nocSpan(noc.Get(), chip::Controller::kMaxCHIPDERCertLength);

    chip::Platform::ScopedMemoryBuffer<uint8_t> icac;
    VerifyOrReturnError(icac.Alloc(chip::Controller::kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    chip::MutableByteSpan icacSpan(icac.Get(), chip::Controller::kMaxCHIPDERCertLength);

    chip::Platform::ScopedMemoryBuffer<uint8_t> rcac;
    VerifyOrReturnError(rcac.Alloc(chip::Controller::kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    chip::MutableByteSpan rcacSpan(rcac.Get(), chip::Controller::kMaxCHIPDERCertLength);

    CHIP_ERROR chainAvailable = LoadNOCChain(rcacSpan, icacSpan, nocSpan);
    bool fabricFound = false;

    if (ChipError::IsSuccess(chainAvailable))
    {
        FabricIndex controllerIndex;
        ReturnErrorOnFailure(
            GetFabricIndex(fabricTable, params.permitMultiControllerFabrics, rcacSpan, nocSpan, controllerIndex));
        fabricFound = controllerIndex != kUndefinedFabricIndex;
    }

    if (chainAvailable == CHIP_ERROR_NOT_FOUND || !fabricFound)
    {
        ChipLogProgress(Support,
                        "No Commissioner NOC chain or no fabric, requesting a new NOC and creating new fabric");

        chip::Platform::ScopedMemoryBuffer<uint8_t> csr;
        VerifyOrReturnError(csr.Alloc(kMIN_CSR_Buffer_Size), CHIP_ERROR_NO_MEMORY);

        chip::MutableByteSpan csrSpan(csr.Get(), kMIN_CSR_Buffer_Size);
        Optional<FabricIndex> nextAvailableIndex;

        /*
         * Key will be committed when SDK creates commissioner fabric
         * This will become the new controller fabric's operational key.
         */
        ReturnErrorOnFailure(fabricTable->AllocatePendingOperationalKey(nextAvailableIndex, csrSpan));

        chip::Platform::ScopedMemoryBuffer<uint8_t> nonce;
        VerifyOrReturnError(nonce.Alloc(Controller::kCSRNonceLength), CHIP_ERROR_NO_MEMORY);
        chip::MutableByteSpan nonceSpan(nonce.Get(), Controller::kCSRNonceLength);

        if (myFabricId != chip::kUndefinedFabricId)
        {
            operationalCredentialsIssuer->SetFabricIdForNextNOCRequest(myFabricId);
        }

        operationalCredentialsIssuer->ObtainCsrNonce(nonceSpan);

        nocSpan = MutableByteSpan(noc.Get(), chip::Controller::kMaxCHIPDERCertLength);
        icacSpan = MutableByteSpan(icac.Get(), chip::Controller::kMaxCHIPDERCertLength);
        rcacSpan = MutableByteSpan(rcac.Get(), chip::Controller::kMaxCHIPDERCertLength);

        ReturnErrorOnFailure(operationalCredentialsIssuer->GenerateNOCChainAfterValidation(
            myNodeId, myFabricId, csrSpan, nonceSpan, rcacSpan, icacSpan, nocSpan));

        ReturnErrorOnFailure(SaveNOCChain(rcacSpan, icacSpan, nocSpan));
    }
    else
    {
        ReturnErrorOnFailure(chainAvailable);
    }

    params.controllerRCAC = rcacSpan;
    params.controllerICAC = icacSpan;
    params.controllerNOC = nocSpan;
    params.operationalCredentialsDelegate = operationalCredentialsIssuer.get();
    params.deviceAttestationVerifier = dacVerifier;
    params.enableServerInteractions = true;

    auto &factory = chip::Controller::DeviceControllerFactory::GetInstance();
    ReturnErrorOnFailure(factory.Init(factoryParams));
    ReturnErrorOnFailure(factory.SetupCommissioner(params, *commissionerController));

    chip::app::InteractionModelEngine::GetInstance()->GetExchangeManager()->GetSessionManager()->SetMessageDelegate(
        &sessionMessageHandler);

    chip::FabricIndex fabricIndex = commissionerController->GetFabricIndex();
    VerifyOrReturnError(fabricIndex != chip::kUndefinedFabricIndex, CHIP_ERROR_INTERNAL);
    myFabricIndex = fabricIndex;

    ReturnLogErrorOnFailure(SetIPKOnce());

    auto ipk = GetCurrentIPK();

    if (ipk != nullptr)
    {
        ReturnLogErrorOnFailure(operationalCredentialsIssuer->SetIPKForNextNOCRequest(ipk.get()->Span()));
    }

    /*
     * Matter has a defect where loading a fabric with no label at all will
     * cause a NULL dereference.
     */
    chip::CharSpan labelSpan = CharSpan::fromCharString("controller");
    fabricTable->SetFabricLabel(fabricIndex, labelSpan);

    ReturnErrorOnFailure(ConfigureOTAProviderNode());

    ReturnLogErrorOnFailure(fabricTable->CommitPendingFabricData());

    // advertise operational since we are an admin
    chip::app::DnssdServer::Instance().AdvertiseOperational();

    ChipLogProgress(Support,
                    "InitCommissioner nodeId=0x" ChipLogFormatX64 " fabric.fabricId=0x" ChipLogFormatX64
                    " fabricIndex=0x%x",
                    ChipLogValueX64(commissionerController->GetNodeId()),
                    ChipLogValueX64(myFabricId),
                    static_cast<unsigned>(fabricIndex));

    return CHIP_NO_ERROR;
}

CHIP_ERROR Matter::SetIPKOnce()
{
    chip::Credentials::GroupDataProvider::KeySet ipkSet;
    FabricIndex fabricIndex = commissionerController->GetFabricIndex();
    CHIP_ERROR ipkStatus = groupDataProvider.GetIpkKeySet(fabricIndex, ipkSet);

    // TODO: add and use getSensitiveProperty
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    scoped_generic char *base64Ipk = b_core_property_provider_get_property_as_string(
        propertyProvider, DEVICE_PROP_MATTER_CURRENT_IPK, NULL);
    uint8_t compressedFabricId[sizeof(uint64_t)] = {0};
    chip::MutableByteSpan compressedFabricIdSpan(compressedFabricId);
    ReturnErrorOnFailure(commissionerController->GetCompressedFabricIdBytes(compressedFabricIdSpan));

    /*
     * Only new fabrics will ever be unkeyed. If the system property were to go away,
     * resetting the IPK would disable the entire fabric.
     * The fabric itself stores the operational IPK and leaving it alone will at least
     * allow continued communication between this node and others on the fabric.
     * Force old fabrics initialized with the default IPK to re-key, which will
     * invalidate communications between this and other nodes on the fabric.
     */

    if (ipkStatus == CHIP_NO_ERROR)
    {
        ByteSpan defaultIpk = chip::GroupTesting::DefaultIpkValue::GetDefaultIpk();

        /*
         * This is an operational key, not an epoch key. The structure member
         * name is misleading here. To be able to compare the default key
         * with what's in the fabric, the default IPK (an epoch key) needs
         * to be run through standard key derivation first to make an
         * operational key to test against.
         */

        ByteSpan currentIpk(ipkSet.epoch_keys[0].key);
        Crypto::GroupOperationalCredentials defaultOperationalIpk;
        VerifyOrReturnError(currentIpk.size() == Crypto::CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES,
                            CHIP_ERROR_BUFFER_TOO_SMALL);

        ReturnErrorOnFailure(
            Crypto::DeriveGroupOperationalCredentials(defaultIpk, compressedFabricIdSpan, defaultOperationalIpk));

        if (memcmp(defaultOperationalIpk.encryption_key,
                   currentIpk.data(),
                   Crypto::CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES) == 0)
        {
            icWarn("Fabric index %" PRIx8 " was initialized with the default IPK."
                   " A new, random IPK will be created. Existing paired Matter devices will no longer work.",
                   fabricIndex);

            ipkStatus = CHIP_ERROR_NOT_FOUND;
        }
    }

    if (!base64Ipk && ipkStatus == CHIP_NO_ERROR)
    {
        icError("Fabric index %" PRIx8 " has an operational IPK but there is no system IPK!"
                " Commissioning is unavailable!",
                fabricIndex);

        /*
         * Instead of aborting completely, let the fabric at least
         * work with its operational IPK
         */
        return CHIP_NO_ERROR;
    }

    if (ipkStatus == CHIP_NO_ERROR)
    {
        return CHIP_NO_ERROR;
    }
    else if (ipkStatus != CHIP_ERROR_NOT_FOUND)
    {
        return ipkStatus;
    }

    ChipLogProgress(Support,
                    "Setting up new IPK for Fabric Index %u with Compressed Fabric ID:",
                    static_cast<unsigned>(fabricIndex));

    ChipLogByteSpan(Support, compressedFabricIdSpan);

    chip::Crypto::IdentityProtectionKey ipk;
    ReturnErrorOnFailure(chip::Crypto::DRBG_get_bytes(ipk.Bytes(), ipk.Length()));

    /*
     * GroupDataProvider is tricky and derives and stores _operational_ keys.
     * API documentation and object declarations are misleading/unclear about this, but
     * Set/GetKeySet will _always_ store/load operational keys. The input epoch keys are
     * discarded by the SetKeySet API.
     * See Matter 1.0 4.15.2.
     */
    const IdentityProtectionKeySpan ipkSpan = ipk.Span();

    ReturnErrorOnFailure(
        chip::Credentials::SetSingleIpkEpochKey(&groupDataProvider, fabricIndex, ipk.Span(), compressedFabricIdSpan));

    free(base64Ipk);
    base64Ipk = icEncodeBase64(const_cast<uint8_t *>(ipkSpan.data()), ipkSpan.size());

    // Any specific errors are logged by the encoder
    VerifyOrReturnError(base64Ipk != NULL, CHIP_ERROR_INTERNAL);

    // TODO: add and use setSensitiveProperty
    VerifyOrReturnError(b_core_property_provider_set_property_string(
                            propertyProvider, DEVICE_PROP_MATTER_CURRENT_IPK, base64Ipk),
                        CHIP_ERROR_INTERNAL);

    return CHIP_NO_ERROR;
}

std::unique_ptr<chip::Crypto::IdentityProtectionKey> Matter::GetCurrentIPK()
{
    chip::FabricIndex fabricIndex = commissionerController->GetFabricIndex();
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    scoped_generic char *base64Ipk = b_core_property_provider_get_property_as_string(
        propertyProvider, DEVICE_PROP_MATTER_CURRENT_IPK, NULL);

    if (base64Ipk == NULL)
    {
        icError("Can not get current IPK set for fabric index %#" PRIx8 "!", fabricIndex);
        return nullptr;
    }

    scoped_generic uint8_t *tmp = NULL;
    uint16_t tmpLen;

    if (!icDecodeBase64(base64Ipk, &tmp, &tmpLen))
    {
        // Failure reason is reported by decoder
        return nullptr;
    }

    if (tmpLen != chip::Crypto::CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES)
    {
        icError("IPK corrupt: invalid key length '%" PRIu16 " bytes (must be %zu bytes)",
                tmpLen,
                chip::Crypto::CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES);

        return nullptr;
    }

    uint8_t keyBytes[chip::Crypto::CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES];
    memcpy(keyBytes, tmp, sizeof(keyBytes));

    return make_unique<IdentityProtectionKey>(keyBytes);
}

//
//  Start private stuff
//

CHIP_ERROR Matter::GetCommissioningParams(chip::Controller::CommissioningParameters &params)
{
    icDebug();

    CHIP_ERROR err = CHIP_NO_ERROR;

    params.SetDeviceAttestationDelegate(this);

#ifdef BARTON_CONFIG_THREAD
    if (threadOperationalDataset == nullptr)
    {
        g_autoptr(ThreadNetworkInfo) info = threadNetworkInfoCreate();
        if (threadSubsystemGetNetworkInfo(info) && info->activeDatasetLen > 0)
        {
            threadOperationalDataset = std::make_unique<std::vector<uint8_t>>();
            for (size_t i = 0; i < info->activeDatasetLen; i++)
            {
                threadOperationalDataset->push_back(info->activeDataset[i]);
            }
        }
        else
        {
            icWarn("Was unable to get thread operational dataset from the subsystem");
        }
    }

    if (threadOperationalDataset != nullptr)
    {
        chip::ByteSpan opDsBs(threadOperationalDataset->data(), threadOperationalDataset->size());
        params.SetThreadOperationalDataset(opDsBs);
    }
#endif

    g_autoptr(GError) error = NULL;
    g_autoptr(BCoreNetworkCredentialsProvider) provider =
        deviceServiceConfigurationGetNetworkCredentialsProvider();
    g_autoptr(BCoreWifiNetworkCredentials) wifiCredentials =
        b_core_network_credentials_provider_get_wifi_network_credentials(provider, &error);

    if (error == NULL && wifiCredentials != NULL)
    {
        free(wifiSsid);
        free(wifiPass);
        g_object_get(wifiCredentials,
                     B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_SSID],
                     &wifiSsid,
                     B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_PSK],
                     &wifiPass,
                     NULL);

        params.SetWiFiCredentials(
            chip::Controller::WiFiCredentials(ByteSpan(chip::Uint8::from_const_char(wifiSsid), strlen(wifiSsid) + 1),
                                              ByteSpan(chip::Uint8::from_const_char(wifiPass), strlen(wifiPass) + 1)));
    }
    else
    {
        icWarn("Failed to get wifi creds.  Commissioning to wifi operational network will fail, but on-network devices "
               "should work. [%d] %s",
               error->code,
               stringCoalesce(error->message));
    }

    return err;
}

void Matter::OnDeviceAttestationCompleted(
    Controller::DeviceCommissioner *deviceCommissioner,
    DeviceProxy *device,
    const chip::Credentials::DeviceAttestationVerifier::AttestationDeviceInfo &info,
    chip::Credentials::AttestationVerificationResult attestationResult)
{
    using namespace chip::Controller;
    icDebug();

    // An error message with code will have been logged by the SDK at this point

    if (IsDevelopmentMode())
    {
        icWarn("Attestation failed, but we are allowing it anyway");
        deviceCommissioner->CommissioningStageComplete(CHIP_NO_ERROR);
    }
    else
    {
        CommissioningDelegate::CommissioningReport report;
        report.Set<AttestationErrorInfo>(attestationResult);

        deviceCommissioner->CommissioningStageComplete(CHIP_ERROR_INTERNAL, report);
    }
}

bool Matter::isQRCode(const std::string &codeString)
{
    return codeString.rfind(chip::kQRCodePrefix) == 0;
}

CHIP_ERROR Matter::ParseSetupPayload(const std::string &codeString, chip::SetupPayload &payload)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    if (isQRCode(codeString))
    {
        err = chip::QRCodeSetupPayloadParser(codeString).populatePayload(payload);
    }
    else
    {
        err = chip::ManualSetupPayloadParser(codeString).populatePayload(payload);
    }

    return err;
}

chip::NodeId Matter::GetRandomOperationalNodeId()
{
    NodeId result = kUndefinedNodeId;
    // logic taken from ExampleOperationalCredentialsIssuer::GetRandomOperationalNodeId
    //  it makes up to 10 passes at generating a random and valid node id

    for (int i = 0; i < 10; ++i)
    {
        CHIP_ERROR err = DRBG_get_bytes(reinterpret_cast<uint8_t *>(&result), sizeof(result));

        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to DRBG_get_bytes");
            result = kUndefinedNodeId;
            break;
        }

        if (IsOperationalNodeId(result))
        {
            break;
        }
    }

    // If we don't have a valid node id after 10 tries, then we are extremely unlucky (the chances of this happening are
    // roughly 1 in 2^280, as per a comment in the ExampleOperationalCredentialsIssuer::GetRandomOperationalNodeId code
    // that this logic was taken from)
    return result;
}

chip::NodeId Matter::LoadOrGenerateLocalNodeId()
{
    chip::NodeId result = kUndefinedNodeId;

    // load what we have from storage, otherwise generate a new random one and persist
    scoped_generic char *localNodeIdStr = nullptr;
    if (!deviceServiceGetSystemProperty(LOCAL_NODE_ID_SYSTEM_PROPERTY_NAME, &localNodeIdStr) ||
        !stringToUnsignedNumberWithinRange(localNodeIdStr, &result, 16, 0, UINT64_MAX))
    {
        result = GetRandomOperationalNodeId();

        if (IsOperationalNodeId(result))
        {
            // persist it
            free(localNodeIdStr);
            localNodeIdStr = stringBuilder("%" PRIx64, result);
            deviceServiceSetSystemProperty(LOCAL_NODE_ID_SYSTEM_PROPERTY_NAME, localNodeIdStr);
        }
        else
        {
            icError("Failed to generate a valid node ID");
        }
    }

    return result;
}

CHIP_ERROR Matter::LoadNOCChain(MutableByteSpan &rCA, MutableByteSpan &iCA, MutableByteSpan &NOC)
{
    {
        scoped_generic char *b64RCA = NULL;

        if (deviceServiceGetSystemProperty(MATTER_RCA_SYSTEM_PROPERTY_NAME, &b64RCA))
        {
            if (rCA.size() < BASE64_MAX_DECODED_LEN(strlen(b64RCA)))
            {
                return CHIP_ERROR_BUFFER_TOO_SMALL;
            }

            if (strlen(b64RCA) == 0)
            {
                return CHIP_ERROR_NOT_FOUND;
            }

            rCA.reduce_size(chip::Base64Decode(b64RCA, std::strlen(b64RCA), rCA.data()));
        }
        else
        {
            return CHIP_ERROR_NOT_FOUND;
        }
    }

    {
        /* Optional */
        scoped_generic char *b64ICA = NULL;

        if (deviceServiceGetSystemProperty(MATTER_ICA_SYSTEM_PROPERTY_NAME, &b64ICA))
        {
            if (iCA.size() < BASE64_MAX_DECODED_LEN(strlen(b64ICA)))
            {
                return CHIP_ERROR_BUFFER_TOO_SMALL;
            }

            if (strlen(b64ICA) != 0)
            {
                iCA.reduce_size(chip::Base64Decode(b64ICA, std::strlen(b64ICA), iCA.data()));
            }
            else
            {
                iCA.reduce_size(0);
            }
        }
        else
        {
            iCA.reduce_size(0);
        }
    }

    {
        scoped_generic char *b64NOC = NULL;

        if (deviceServiceGetSystemProperty(MATTER_NOC_SYSTEM_PROPERTY_NAME, &b64NOC))
        {
            if (NOC.size() < BASE64_MAX_DECODED_LEN(strlen(b64NOC)))
            {
                return CHIP_ERROR_BUFFER_TOO_SMALL;
            }

            if (strlen(b64NOC) == 0)
            {
                return CHIP_ERROR_NOT_FOUND;
            }

            NOC.reduce_size(chip::Base64Decode(b64NOC, std::strlen(b64NOC), NOC.data()));
        }
        else
        {
            return CHIP_ERROR_NOT_FOUND;
        }
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR Matter::SaveNOCChain(ByteSpan rCA, ByteSpan iCA, ByteSpan NOC)
{
    {
        chip::Platform::ScopedMemoryBuffer<char> b64RCA;
        const uint16_t maxLen = BASE64_ENCODED_LEN(rCA.size());
        b64RCA.Alloc(maxLen + 1);
        uint16_t len = Base64Encode(rCA.data(), rCA.size(), b64RCA.Get());

        VerifyOrReturnError(len <= maxLen, CHIP_ERROR_INTERNAL);
        b64RCA[len] = '\0';

        VerifyOrReturnError(deviceServiceSetSystemProperty(MATTER_RCA_SYSTEM_PROPERTY_NAME, b64RCA.Get()),
                            CHIP_ERROR_INTERNAL);
    }

    if (iCA.size() != 0)
    {
        chip::Platform::ScopedMemoryBuffer<char> b64ICA;
        const uint16_t maxLen = BASE64_ENCODED_LEN(iCA.size());
        b64ICA.Alloc(maxLen + 1);
        uint16_t len = Base64Encode(iCA.data(), iCA.size(), b64ICA.Get());

        VerifyOrReturnError(len <= maxLen, CHIP_ERROR_INTERNAL);
        b64ICA[len] = '\0';

        VerifyOrReturnError(deviceServiceSetSystemProperty(MATTER_ICA_SYSTEM_PROPERTY_NAME, b64ICA.Get()),
                            CHIP_ERROR_INTERNAL);
    }

    {
        chip::Platform::ScopedMemoryBuffer<char> b64NOC;
        const uint16_t maxLen = BASE64_ENCODED_LEN(NOC.size());
        b64NOC.Alloc(maxLen + 1);
        uint16_t len = Base64Encode(NOC.data(), NOC.size(), b64NOC.Get());

        VerifyOrReturnError(len <= maxLen, CHIP_ERROR_INTERNAL);
        b64NOC[len] = '\0';

        VerifyOrReturnError(deviceServiceSetSystemProperty(MATTER_NOC_SYSTEM_PROPERTY_NAME, b64NOC.Get()),
                            CHIP_ERROR_INTERNAL);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR Matter::GetFabricIndex(FabricTable *fabricTable,
                                  bool mutliControllerAllowed,
                                  ByteSpan rCA,
                                  ByteSpan NOC,
                                  FabricIndex &idxOut)
{
    NodeId nodeId;
    FabricId tmpFabricId;
    Credentials::P256PublicKeySpan rootPublicKeySpan;

    chip::Platform::ScopedMemoryBuffer<uint8_t> chipNOC;

    VerifyOrReturnError(chipNOC.Alloc(chip::Credentials::kMaxCHIPCertLength), CHIP_ERROR_NO_MEMORY);
    chip::MutableByteSpan chipNOCSpan(chipNOC.Get(), chip::Credentials::kMaxCHIPCertLength);
    ReturnErrorOnFailure(Credentials::ConvertX509CertToChipCert(NOC, chipNOCSpan));
    ReturnErrorOnFailure(Credentials::ExtractNodeIdFabricIdFromOpCert(chipNOCSpan, &nodeId, &tmpFabricId));

    chip::Platform::ScopedMemoryBuffer<uint8_t> chipRCA;
    VerifyOrReturnError(chipRCA.Alloc(chip::Controller::kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    chip::MutableByteSpan chipRCASpan(chipRCA.Get(), chip::Credentials::kMaxCHIPCertLength);
    ReturnErrorOnFailure(Credentials::ConvertX509CertToChipCert(rCA, chipRCASpan));
    ReturnErrorOnFailure(Credentials::ExtractPublicKeyFromChipCert(chipRCASpan, rootPublicKeySpan));
    Crypto::P256PublicKey rootPublicKey {rootPublicKeySpan};

    const FabricInfo *controllerFabric = nullptr;

    if (mutliControllerAllowed)
    {
        controllerFabric = fabricTable->FindIdentity(rootPublicKey, nodeId, tmpFabricId);
    }
    else
    {
        controllerFabric = fabricTable->FindFabric(rootPublicKey, tmpFabricId);
    }

    if (controllerFabric != nullptr)
    {
        idxOut = controllerFabric->GetFabricIndex();
    }
    else
    {
        idxOut = kUndefinedFabricIndex;
    }

    return CHIP_NO_ERROR;
}

bool Matter::IsAccessibleByOTARequestors()
{
    icDebug();

    bool result = false;

    // This will always return false if the node isn't initialized

    if (myFabricId == chip::kUndefinedFabricId || myNodeId == chip::kUndefinedNodeId)
    {
        icWarn("Node isn't initialized");
        return result;
    }

    size_t index = 0;
    CHIP_ERROR aclCountErr = chip::Access::GetAccessControl().GetEntryCount(myFabricIndex, index);

    if (aclCountErr != CHIP_NO_ERROR)
    {
        icError("Failed to get ACL entry count for fabric index %" PRIu8 ": %s", myFabricIndex, aclCountErr.AsString());
        return false;
    }

    while (index)
    {
        Access::AccessControl::Entry entry;
        CHIP_ERROR err = Access::GetAccessControl().ReadEntry(myFabricIndex, --index, entry);

        if (err != CHIP_NO_ERROR)
        {
            icWarn("Unable to read ACL entry at index: %zu", index);
            continue;
        }

        chip::FabricIndex currFabricIndex;
        chip::Access::Privilege currPrivilege;
        chip::Access::AuthMode currAuthMode;

        LogErrorOnFailure(AccessControlDump(entry));

        if (entry.GetFabricIndex(currFabricIndex) == CHIP_NO_ERROR &&
            entry.GetPrivilege(currPrivilege) == CHIP_NO_ERROR && entry.GetAuthMode(currAuthMode) == CHIP_NO_ERROR)
        {
            if (currFabricIndex == myFabricIndex && currPrivilege >= chip::Access::Privilege::kOperate &&
                currAuthMode == chip::Access::AuthMode::kCase)
            {
                // OTA Requestors need at least the "operate" privilege over CASE
                result = true;
                break;
            }
        }
    }

    return result;
}

CHIP_ERROR Matter::AppendOTARequestorsACLEntry(chip::FabricId fabricId, chip::FabricIndex fabricIndex)
{
    icDebug();

    CHIP_ERROR err = CHIP_NO_ERROR;

    if (fabricIndex == chip::kUndefinedFabricIndex)
    {
        icError("No such entry on fabric table for fabricId: %" PRIu64, fabricId);
        return CHIP_ERROR_INVALID_FABRIC_INDEX;
    }

    chip::Access::SubjectDescriptor subjectDescriptor = {
        .fabricIndex = fabricIndex,
        .authMode = Access::AuthMode::kCase,
    };

    icDebug("fabricIndex = %d", fabricIndex);
    chip::Access::AccessControl::Entry::Target target = {
        .flags = chip::Access::AccessControl::Entry::Target::kCluster,
        .cluster = chip::app::Clusters::OtaSoftwareUpdateProvider::Id,
    };

    chip::Access::AccessControl::Entry entry;
    ReturnErrorOnFailure(Access::GetAccessControl().PrepareEntry(entry));
    ReturnErrorOnFailure(entry.SetFabricIndex(fabricIndex));
    ReturnErrorOnFailure(entry.SetPrivilege(Access::Privilege::kOperate));
    ReturnErrorOnFailure(entry.SetAuthMode(Access::AuthMode::kCase));
    ReturnErrorOnFailure(entry.AddTarget(nullptr, target));

    LogErrorOnFailure(AccessControlDump(entry));

    err = Access::GetAccessControl().CreateEntry(&subjectDescriptor, fabricIndex, nullptr, entry);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to add ACL entry: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    return err;
}

CHIP_ERROR Matter::ConfigureOTAProviderNode()
{
    icDebug();

    if (IsAccessibleByOTARequestors() == false)
    {
        icDebug("Adding ACL entries so that OTA Requestors can poll our node");
        ReturnErrorOnFailure(AppendOTARequestorsACLEntry(myFabricId, myFabricIndex));
    }

    // This functionality will be added back when we support being a Controller
    // chip::app::Clusters::OTAProvider::SetDelegate(otaProviderEndpointId, &otaProvider);

    return CHIP_NO_ERROR;
}

CHIP_ERROR Matter::AccessControlDump(const chip::Access::AccessControl::Entry &entry)
{
    // TODO: Write a proper implementation when there is value in doing so
    return CHIP_NO_ERROR;
}

bool Matter::OpenLocalCommissioningWindow(uint16_t discriminator, uint16_t timeoutSecs, SetupPayload &setupPayload)
{
    bool success = false;

    auto &commissioningWindowManager = Server::GetInstance().GetCommissioningWindowManager();
    uint32_t iterations = kDefaultPAKEIterationCount;

    chip::DeviceLayer::PlatformMgr().LockChipStack();

    if (commissioningWindowManager.IsCommissioningWindowOpen())
    {
        icWarn("Commissioning window was already open.  Closing it and opening a new one.");
        commissioningWindowManager.CloseCommissioningWindow();
    }

    uint8_t pbkdfSaltBuffer[Crypto::kSpake2p_Max_PBKDF_Salt_Length];
    ByteSpan pbkdfSalt;
    Crypto::Spake2pVerifierSerialized pakePasscodeVerifierBuffer;

    Crypto::DRBG_get_bytes(pbkdfSaltBuffer, sizeof(pbkdfSaltBuffer));
    pbkdfSalt = ByteSpan(pbkdfSaltBuffer);

    Spake2pVerifier verifier;
    if (CHIP_NO_ERROR != verifier.Generate(iterations, pbkdfSalt, setupPayload.setUpPINCode))
    {
        icError("Failed to generate verifier");
        success = false;
    }
    else
    {
        uint16_t vendorId;
        DeviceLayer::GetDeviceInstanceInfoProvider()->GetVendorId(vendorId);
        success = CHIP_NO_ERROR ==
                  commissioningWindowManager.OpenEnhancedCommissioningWindow(System::Clock::Seconds16(timeoutSecs),
                                                                             discriminator,
                                                                             verifier,
                                                                             iterations,
                                                                             pbkdfSalt,
                                                                             myFabricIndex,
                                                                             (VendorId)vendorId);
    }

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    return success;
}

bool Matter::OpenCommissioningWindow(chip::NodeId nodeId,
                                     uint16_t timeoutSecs,
                                     std::string &setupCode,
                                     std::string &qrCode)
{
    bool success;

    SetupPayload setupPayload;
    auto &commissioningWindowManager = Server::GetInstance().GetCommissioningWindowManager();

    // since we (or the remote device) are already commissioned, we can only support on network commissioning
    setupPayload.rendezvousInformation.SetValue(RendezvousInformationFlags(RendezvousInformationFlag::kOnNetwork));

    uint16_t discriminator = 0;
    if (commissionableDataProvider->GetSetupDiscriminator(discriminator) != CHIP_NO_ERROR)
    {
        icError("Failed to get setup discriminator from CommissionableDataProvider");
        return false;
    }
    setupPayload.discriminator.SetLongValue(discriminator);

    uint32_t setupPINCode = 0;
    if (commissionableDataProvider->GetSetupPasscode(setupPINCode) != CHIP_NO_ERROR)
    {
        icError("Failed to get setup passcode from CommissionableDataProvider");
        return false;
    }
    setupPayload.setUpPINCode = setupPINCode;

    if (timeoutSecs == 0)
    {
        // zero means 'use default' which is max
        timeoutSecs = commissioningWindowManager.MaxCommissioningTimeout().count();
    }
    else
    {
        if (timeoutSecs > commissioningWindowManager.MaxCommissioningTimeout().count())
        {
            icWarn("Requested commissioning window timeout of %" PRIu16 " seconds is too long, using max of %" PRIu16
                   " seconds",
                   timeoutSecs,
                   commissioningWindowManager.MaxCommissioningTimeout().count());
            timeoutSecs = commissioningWindowManager.MaxCommissioningTimeout().count();
        }
        else if (timeoutSecs < commissioningWindowManager.MinCommissioningTimeout().count())
        {
            icWarn("Requested commissioning window timeout of %" PRIu16 " seconds is too short, using min of %" PRIu16
                   " seconds",
                   timeoutSecs,
                   commissioningWindowManager.MinCommissioningTimeout().count());
            timeoutSecs = commissioningWindowManager.MinCommissioningTimeout().count();
        }
    }

    if (nodeId == 0)
    {
        icInfo("Opening Local Commissioning Window for %" PRIu16 " seconds", timeoutSecs);
    }
    else
    {
        icInfo("Opening Commissioning Window on node 0x%" PRIx64 " for %" PRIu16 " seconds", nodeId, timeoutSecs);
    }


    if (nodeId == 0)
    {
        success = OpenLocalCommissioningWindow(discriminator, timeoutSecs, setupPayload);
    }
    else
    {
        chip::DeviceLayer::PlatformMgr().LockChipStack();

        success = CHIP_NO_ERROR == Controller::AutoCommissioningWindowOpener::OpenCommissioningWindow(
                                       commissionerController.get(),
                                       nodeId,
                                       System::Clock::Seconds16(timeoutSecs),
                                       kDefaultPAKEIterationCount,
                                       setupPayload.discriminator.GetLongValue(),
                                       MakeOptional<uint32_t>(setupPayload.setUpPINCode),
                                       NullOptional,
                                       setupPayload);

        chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    }


    if (success)
    {
        QRCodeSetupPayloadGenerator qrGenerator(setupPayload);
        success &= CHIP_NO_ERROR == qrGenerator.payloadBase38RepresentationWithAutoTLVBuffer(qrCode);

        ManualSetupPayloadGenerator manualGenerator(setupPayload);
        success &= CHIP_NO_ERROR == manualGenerator.payloadDecimalStringRepresentation(setupCode);
    }


    ConfigurationMgr().LogDeviceConfig();

    return success;
}

bool Matter::SetAccessRestrictionList()
{
#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS && ENABLE_ARLS_FOR_TESTING
    bool success = false;

    //TODO make these real.  The restrictions now are just for certification testing and only block some attribute that wont cause cert issue
    icWarn("Setting Access Restrictions");

    chip::DeviceLayer::PlatformMgr().LockChipStack();

    Access::AccessRestrictionProvider::Entry locationEntry;
    locationEntry.endpointNumber = 1;
    locationEntry.clusterId = app::Clusters::WiFiNetworkManagement::Id;
    Access::AccessRestrictionProvider::Restriction restriction;
    restriction.restrictionType = Access::AccessRestrictionProvider::Type::kAttributeAccessForbidden;
    restriction.id.SetValue(app::Clusters::WiFiNetworkManagement::Attributes::Ssid::Id);
    locationEntry.restrictions.push_back(restriction);

    success = (CHIP_NO_ERROR == accessRestrictionProvider.SetCommissioningEntries(std::vector<Access::AccessRestrictionProvider::Entry>({locationEntry})));

    //TODO proactively set for fabrics.  This should instead be done at commissioning time.
    for (FabricIndex fabric = 1; fabric <=10; fabric++)
    {
        locationEntry.fabricIndex = fabric;
        success &= (CHIP_NO_ERROR == accessRestrictionProvider.SetEntries(fabric, std::vector<Access::AccessRestrictionProvider::Entry>({locationEntry})));
    }

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    return success;
#else
    return true;
#endif
}

bool Matter::ClearAccessRestrictionList()
{
    bool success = true;

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    icWarn("Certification: Clearing AccessRestrictions");

    chip::DeviceLayer::PlatformMgr().LockChipStack();

    success &= (CHIP_NO_ERROR == accessRestrictionProvider.SetCommissioningEntries(std::vector<Access::AccessRestrictionProvider::Entry>()));

    //FabricTable *fabricTable = &Server::GetInstance().GetFabricTable();
    //for (const auto & iterFabricInfo : *fabricTable)
    //TODO clear our proactively configured fabric indices. Once the hard-coded fabric id range is deprecated from above, work with actual fabrics
    for (FabricIndex fabric = 1; fabric <=10; fabric++)
    {
        success &= (CHIP_NO_ERROR == accessRestrictionProvider.SetEntries(fabric, std::vector<Access::AccessRestrictionProvider::Entry>()));
    }

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
#endif

    return success;
}

// Note: Because we have initialized two parts of the stack (commissioner and commissionee), the callback functions
// below are invoked twice. As long as they're attempting to perform the exact same initialization each time, we can
// safely ignore calls after the first as a workaround.
void emberAfThreadNetworkDirectoryClusterInitCallback(EndpointId endpoint)
{
    if (threadNetworkDirectoryServer)
    {
        VerifyOrDie(endpoint == threadNetworkDirectoryServerEndpointId);
    }
    else
    {
        threadNetworkDirectoryServer.emplace(endpoint).Init();
        // The server object's endpointId accessor is private,
        // so this is the only way to track previously received endpoints
        threadNetworkDirectoryServerEndpointId = endpoint;
    }
}

void emberAfWiFiNetworkManagementClusterInitCallback(EndpointId endpoint)
{
    if (wifiNetworkManagementServer)
    {
        VerifyOrDie(endpoint == wifiNetworkManagementServerEndpointId);
    }
    else
    {
        wifiNetworkManagementServer.emplace(endpoint).Init();
        wifiNetworkManagementServerEndpointId = endpoint;
    }
}

void emberAfThreadBorderRouterManagementClusterInitCallback(EndpointId endpoint)
{
    if (threadBorderRouterManagementServer)
    {
        VerifyOrDie(endpoint == threadBorderRouterManagementServerEndpointId);
    }
    else
    {
        threadBorderRouterManagementServer.emplace(endpoint,
                                                   otbrDelegate.get(),
                                                   Server::GetInstance().GetFailSafeContext()).Init();
        threadBorderRouterManagementServerEndpointId = endpoint;
    }
}
