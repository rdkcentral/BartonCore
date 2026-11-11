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
 * Created by Thomas Lea on 12/7/21.
 */

#include "cjson/cJSON.h"
#include "icConcurrent/delayedTask.h"
#include "subsystemManagerCallbacks.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#include <libxml/parser.h>

extern "C" {
#include "deviceService.h"
#include "deviceServiceConfiguration.h"
#include "deviceServiceProperties.h"
#include "event/deviceEventProducer.h"
#include "icConcurrent/repeatingTask.h"
#include "icLog/logging.h"
#include "subsystemManager.h"
#include "threadSubsystem.h"
}

#include "OpenThreadClient.hpp"
#include <glib.h>

#define logFmt(fmt)                "%s: " fmt, __func__
#define LOG_TAG                    "threadSubsystem"

#define NETWORK_BLOB_PROPERTY_NAME "threadNetworkConfig"
#define MONITOR_TASK_INTERVAL_SECS 60

using namespace barton;

namespace
{
    constexpr uint16_t THREAD_SUBSYSTEM_VERSION = 1;
    // TODO: Make this a compile time option when both zigbee and thread can be controlled by it.
    constexpr uint8_t THREAD_RADIO_CHANNEL = 25;
} // namespace

static std::mutex lifecycleDataGuard;
static subsystemInitializedFunc notifySubsystemInitialized;
static subsystemDeInitializedFunc notifySubsystemDeInitialized;
static uint32_t monitorTask = 0;
static std::shared_ptr<OpenThreadClient> otClient;
static bool initialized = false;

/**
 * Callers use the returned reference, not the global, so shutdown can wait for them before destroying the client.
 */
static std::shared_ptr<OpenThreadClient> getClient(bool requireInitialized = true)
{
    std::lock_guard<std::mutex> lock(lifecycleDataGuard);
    return (initialized || !requireInitialized) ? otClient : nullptr;
}
// The Thread network name must be 16 chars or less
// Test value for development only; this value must be provided by the client otherwise
static std::string bartonThreadNetworkName = "TestNetwork";

static bool initTaskFunc(void *arg);
static bool initialize(subsystemInitializedFunc initializedCallback, subsystemDeInitializedFunc deInitializedCallback);
static void shutdown();
static cJSON *getStatusJson();
static bool migrate(uint16_t oldVersion, uint16_t newVersion);

__attribute__((constructor)) __attribute__((used)) static void registerSubsystem(void)
{
    g_autoptr(Subsystem) threadSubsystem = createSubsystem();
    threadSubsystem->migrate = migrate;
    threadSubsystem->initialize = initialize;
    threadSubsystem->shutdown = shutdown;
    threadSubsystem->getStatusJson = getStatusJson;
    threadSubsystem->name = THREAD_SUBSYSTEM_NAME;
    threadSubsystem->version = THREAD_SUBSYSTEM_VERSION;

    subsystemManagerRegister(threadSubsystem);
}

/*
 * Called by subsystemManager once
 */
static bool initialize(subsystemInitializedFunc initializedCallback, subsystemDeInitializedFunc deInitializedCallback)
{
    icDebug();

    std::lock_guard<std::mutex> lock(lifecycleDataGuard);

    notifySubsystemInitialized = initializedCallback;
    notifySubsystemDeInitialized = deInitializedCallback;

    RepeatingTaskPolicy *policy = createFixedRepeatingTaskPolicy(MONITOR_TASK_INTERVAL_SECS, DELAY_SECS);

    // Retry initialization until success
    monitorTask = createPolicyRepeatingTask(initTaskFunc, nullptr, policy, nullptr);

    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    scoped_generic char *defaultNetworkName = b_core_property_provider_get_property_as_string(
        propertyProvider, DEFAULT_THREAD_NETWORK_NAME_PROP_KEY, bartonThreadNetworkName.c_str());
    bartonThreadNetworkName = defaultNetworkName;
    icDebug("Using Thread network name: %s", bartonThreadNetworkName.c_str());

    return true;
}

static void shutdown()
{
    icDebug();

    uint32_t localMonitorTaskHandle;
    subsystemDeInitializedFunc localNotify;
    std::shared_ptr<OpenThreadClient> localClient;

    {
        std::lock_guard<std::mutex> lock(lifecycleDataGuard);

        localMonitorTaskHandle = monitorTask;
        monitorTask = 0;

        notifySubsystemInitialized = nullptr;
        localNotify = notifySubsystemDeInitialized;
        notifySubsystemDeInitialized = nullptr;

        // No new caller can get the client from here on.
        initialized = false;
        localClient = std::move(otClient);
    }

    cancelRepeatingTask(localMonitorTaskHandle);

    // Wait for calls already in progress to release the client, so it is destroyed here and never while in use.
    // Calls are bounded by D-Bus timeouts. lifecycleDataGuard is not held: the client's callback thread can need it.
    while (localClient && localClient.use_count() > 1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    localClient.reset();

    localNotify(THREAD_SUBSYSTEM_NAME);
}

static cJSON *getStatusJson()
{
    icDebug();

    lifecycleDataGuard.lock();
    bool localInitialized = initialized;
    lifecycleDataGuard.unlock();

    cJSON *retVal = NULL;

    if (localInitialized)
    {
        g_autoptr(ThreadNetworkInfo) threadInfo = threadNetworkInfoCreate();
        g_return_val_if_fail(threadSubsystemGetNetworkInfo(threadInfo), nullptr);
        retVal = threadNetworkInfoToJson(threadInfo);
    }
    else
    {
        retVal = cJSON_CreateObject();
    }

    cJSON_AddBoolToObject(retVal, SUBSYSTEM_STATUS_COMMON_READY, localInitialized);

    return retVal;
}

static bool migrate(uint16_t oldVersion, uint16_t newVersion)
{
    return true;
}

const char *threadSubsystemGetInterfaceName(void)
{
    static std::once_flag interfaceNameOnce;
    static std::string interfaceName;

    // Read once so that every caller (Matter and the OTBR client) agrees on the border router for this run
    std::call_once(interfaceNameOnce, []() {
        g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        scoped_generic char *name = b_core_property_provider_get_property_as_string(
            propertyProvider, B_CORE_BARTON_THREAD_INTERFACE_NAME, THREAD_INTERFACE_NAME_DEFAULT);

        interfaceName = (name != nullptr && *name != '\0') ? name : THREAD_INTERFACE_NAME_DEFAULT;
        icInfo("Using Thread interface %s", interfaceName.c_str());
    });

    return interfaceName.c_str();
}

bool threadSubsystemGetNetworkInfo(ThreadNetworkInfo *info)
{
    icDebug();

    g_return_val_if_fail(info, false);

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    bool retVal = false;

    if (localInitialized)
    {
        // get all the data
        uint16_t channel = client->GetChannel();
        uint16_t panId = client->GetPanId();
        uint64_t extendedPanId = client->GetExtPanId();
        std::vector<uint8_t> networkKey = client->GetNetworkKey();
        std::vector<uint8_t> activeTlvs = client->GetActiveDatasetTlvs();
        std::vector<uint8_t> pendingTlvs = client->GetPendingDatasetTlvs();
        std::string networkName = client->GetNetworkName();
        OpenThreadClient::DeviceRole role = client->GetDeviceRole();
        bool nat64Enabled = client->IsNat64Enabled();
        std::vector<uint8_t> borderAgentId = client->GetBorderAgentId();
        uint16_t threadVersion = client->GetThreadVersion();
        bool interfaceUp = client->IsThreadInterfaceUp();

        // validate what we can
        g_return_val_if_fail(channel != UINT16_MAX, false);
        g_return_val_if_fail(panId != UINT16_MAX, false);
        g_return_val_if_fail(extendedPanId != UINT64_MAX, false);
        g_return_val_if_fail(networkKey.size() == THREAD_NETWORK_KEY_LEN, false);
        g_return_val_if_fail(!activeTlvs.empty(), false);
        // pendingTlvs can be empty
        g_return_val_if_fail(borderAgentId.size() == THREAD_BORDER_AGENT_ID_LEN, false);
        g_return_val_if_fail(threadVersion != 0, false);

        // validation succeeded, so fill it in
        info->channel = channel;
        info->panId = panId;
        info->extendedPanId = extendedPanId;
        std::memcpy(info->networkKey, networkKey.data(), THREAD_NETWORK_KEY_LEN);

        info->activeDataset = (uint8_t *) malloc(activeTlvs.size());
        std::memcpy(info->activeDataset, activeTlvs.data(), activeTlvs.size());
        info->activeDatasetLen = activeTlvs.size();

        info->pendingDataset = (uint8_t *) malloc(pendingTlvs.size());
        std::memcpy(info->pendingDataset, pendingTlvs.data(), pendingTlvs.size());
        info->pendingDatasetLen = pendingTlvs.size();

        info->networkName = strdup(networkName.c_str());

        switch (role)
        {
            case OpenThreadClient::DeviceRole::DEVICE_ROLE_DISABLED:
                info->role = strdup("disabled");
                break;
            case OpenThreadClient::DeviceRole::DEVICE_ROLE_CHILD:
                info->role = strdup("child");
                break;
            case OpenThreadClient::DeviceRole::DEVICE_ROLE_ROUTER:
                info->role = strdup("router");
                break;
            case OpenThreadClient::DeviceRole::DEVICE_ROLE_LEADER:
                info->role = strdup("leader");
                break;
            default:
                info->role = strdup("unknown");
                break;
        }

        info->nat64Enabled = nat64Enabled;
        std::memcpy(info->borderAgentId, borderAgentId.data(), THREAD_BORDER_AGENT_ID_LEN);
        info->threadVersion = threadVersion;
        info->interfaceUp = interfaceUp;

        retVal = true;
    }

    return retVal;
}

bool threadSubsystemGetBorderAgentId(GArray **borderAgentId)
{
    icDebug();

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    bool retVal = false;

    if (localInitialized)
    {
        auto id = client->GetBorderAgentId();
        if (!id.empty())
        {
            *borderAgentId = g_array_new(FALSE, FALSE, sizeof(uint8_t));
            g_array_append_vals(*borderAgentId, id.data(), id.size());
            retVal = true;
        }
    }

    return retVal;
}

uint16_t threadSubsystemGetThreadVersion()
{
    icDebug();

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    uint16_t retVal = 0;

    if (localInitialized)
    {
        retVal = client->GetThreadVersion();
    }

    return retVal;
}

bool threadSubsystemIsThreadInterfaceUp()
{
    icDebug();

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    bool retVal = false;

    if (localInitialized)
    {
        retVal = client->IsThreadInterfaceUp();
    }

    return retVal;
}

bool threadSubsystemGetActiveDatasetTlvs(GArray **datasetTlvs)
{
    icDebug();

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    bool retVal = false;

    if (localInitialized)
    {
        auto tlvs = client->GetActiveDatasetTlvs();
        if (!tlvs.empty())
        {
            *datasetTlvs = g_array_new(FALSE, FALSE, sizeof(uint8_t));
            g_array_append_vals(*datasetTlvs, tlvs.data(), tlvs.size());
            retVal = true;
        }
    }

    return retVal;
}

bool threadSubsystemGetPendingDatasetTlvs(GArray **datasetTlvs)
{
    icDebug();

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    bool retVal = false;

    if (localInitialized)
    {
        auto tlvs = client->GetPendingDatasetTlvs();
        if (!tlvs.empty())
        {
            *datasetTlvs = g_array_new(FALSE, FALSE, sizeof(uint8_t));
            g_array_append_vals(*datasetTlvs, tlvs.data(), tlvs.size());
            retVal = true;
        }
    }

    return retVal;
}

bool threadSubsystemSetNat64Enabled(bool enable)
{
    icDebug();

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    if (localInitialized)
    {
        return client->SetNat64Enabled(enable);
    }

    return false;
}

char *threadSubsystemActivateEphemeralKeyMode(void)
{
    std::string key;

    icDebug();

    std::shared_ptr<OpenThreadClient> client = getClient();
    bool localInitialized = client != nullptr;

    if (localInitialized && client->ActivateEphemeralKeyMode(key))
    {
        return strdup(key.c_str());
    }

    return nullptr;
}

/**
 * Either create a new network or restore the existing one.
 */
static bool initializeThreadStack(void)
{
    icDebug();

    std::shared_ptr<OpenThreadClient> client;

    {
        std::lock_guard<std::mutex> lock(lifecycleDataGuard);

        if (!otClient)
        {
            otClient = std::make_shared<OpenThreadClient>(
                []() { sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonSubsystemStatus); });
        }

        client = otClient;
    }

    bool result = false;

    scoped_generic char *blob = nullptr;

    if (deviceServiceGetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, &blob) == false || blob == nullptr ||
        strlen(blob) == 0)
    {
        icWarn("No network configuration loaded; a new network will be created");

        std::vector<uint8_t> networkTlvs = client->CreateNetwork(bartonThreadNetworkName, THREAD_RADIO_CHANNEL);
        if (networkTlvs.size() > 0)
        {
            const uint8_t *networkData = networkTlvs.data();
            blob = g_base64_encode(networkData, networkTlvs.size());
            if (blob)
            {
                icInfo("Successfully initialized a new Thread network");
                result = deviceServiceSetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, blob);
                if (result == false)
                {
                    icError("Failed to save new network configuration.");
                }
            }
            else
            {
                icError("Failed to encode thread network data to base64.");
            }
        }
        else
        {
            icError("Failed to create network, trying again in %d seconds...", MONITOR_TASK_INTERVAL_SECS);
        }
    }
    else
    {
        gsize decodedLen = 0;
        g_autofree guchar *decodedTlvs = g_base64_decode(blob, &decodedLen);

        if (decodedLen > 0)
        {
            std::vector<uint8_t> tlvsVect(decodedTlvs, decodedTlvs + decodedLen);
            result = client->RestoreNetwork(tlvsVect);
            if (result == false)
            {
                icError("Failed to restore network configuration, trying again in %d seconds...",
                        MONITOR_TASK_INTERVAL_SECS);
            }
            else
            {
                icInfo("Thread network restored.");
            }
        }
        else
        {
            icError("Failed to base64 decode network data");
        }
    }

    return result;
}

static bool initTaskFunc(void *arg)
{
    icDebug();

    bool stop = false;

    lifecycleDataGuard.lock();
    bool localInitialized = initialized;
    subsystemInitializedFunc localNotify = notifySubsystemInitialized;
    lifecycleDataGuard.unlock();

    if (!localInitialized)
    {
        if (initializeThreadStack())
        {
            stop = true;

            if (localNotify)
            {
                localNotify(THREAD_SUBSYSTEM_NAME);
            }

            std::lock_guard<std::mutex> lock(lifecycleDataGuard);
            initialized = true;
        }
    }
    else
    {
        // the network was initialized before and happy.  Check that it is still happy.
        std::shared_ptr<OpenThreadClient> client = getClient(false);
        OpenThreadClient::DeviceRole role = client ? client->GetDeviceRole() : OpenThreadClient::DEVICE_ROLE_UNKNOWN;
        if (role == OpenThreadClient::DEVICE_ROLE_LEADER || role == OpenThreadClient::DEVICE_ROLE_ROUTER)
        {
            icDebug("thread network is already initialized and our role is %d", (int) role);

            stop = true;
        }
        else
        {
            icDebug("thread network was already initialized, but is not in a good state. Our role is currently %d",
                    (int) role);
        }
    }

    return stop;
}
