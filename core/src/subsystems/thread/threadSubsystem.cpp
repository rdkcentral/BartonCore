//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 12/7/21.
 */

#include "cjson/cJSON.h"
#include "icConcurrent/delayedTask.h"
#include "subsystemManagerCallbacks.h"
#include <cstdint>
#include <cstring>
#include <mutex>

extern "C" {
#include "deviceService.h"
#include "icConcurrent/repeatingTask.h"
#include "icLog/logging.h"
#include "subsystemManager.h"
}

#include "OpenThreadClient.hpp"
#include "threadSubsystem.hpp"
#include <glib.h>

#define logFmt(fmt) "%s: " fmt, __func__
#define LOG_TAG     "threadSubsystem"

#define NETWORK_BLOB_PROPERTY_NAME "threadNetworkConfig"
#define MONITOR_TASK_INTERVAL_SECS 60

#define THREAD_OPERATIONAL_DATASET_FALLBACK_PROPERTY "threadOpDataSet"

using namespace zilker;

namespace
{
    // MUST BE 16 chars OR LESS
    // TODO: brandify this
    constexpr char ZILKER_THREAD_NETWORK_NAME[] = "Xfinity";
    constexpr uint16_t THREAD_SUBSYSTEM_VERSION = 1;
    // TODO: Make this a compile time option when both zigbee and thread can be controlled by it.
    constexpr uint8_t THREAD_RADIO_CHANNEL = 25;
} // namespace

static std::mutex lifecycleDataGuard;
static subsystemInitializedFunc notifySubsystemInitialized;
static subsystemDeInitializedFunc notifySubsystemDeInitialized;
static uint32_t monitorTask = 0;
static OpenThreadClient *otClient = nullptr;
static bool initialized = false;

static bool monitorTaskFunc(void *arg);
static bool initialize(subsystemInitializedFunc initializedCallback, subsystemDeInitializedFunc deInitializedCallback);
static void shutdown();
static cJSON *getStatusJson();
static bool migrate(uint16_t oldVersion, uint16_t newVersion);

__attribute__((constructor)) __attribute__((used)) static void registerSubsystem(void)
{
    static Subsystem threadSubsystem = {.migrate = migrate,
                                        .initialize = initialize,
                                        .shutdown = shutdown,
                                        .getStatusJson = getStatusJson,
                                        .name = THREAD_SUBSYSTEM_NAME,
                                        .version = THREAD_SUBSYSTEM_VERSION};

    subsystemManagerRegister(&threadSubsystem);
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
    monitorTask = createPolicyRepeatingTask(monitorTaskFunc, nullptr, policy, nullptr);

    return true;
}

static void shutdown()
{
    icDebug();

    uint32_t localMonitorTaskHandle;
    subsystemDeInitializedFunc localNotify;

    {
        std::lock_guard<std::mutex> lock(lifecycleDataGuard);

        localMonitorTaskHandle = monitorTask;
        monitorTask = 0;

        notifySubsystemInitialized = nullptr;
        localNotify = notifySubsystemDeInitialized;
        notifySubsystemDeInitialized = nullptr;

        delete otClient;
        otClient = nullptr;
    }

    cancelRepeatingTask(localMonitorTaskHandle);

    localNotify(THREAD_SUBSYSTEM_NAME);
}

static cJSON *getStatusJson()
{
    icDebug();

    lifecycleDataGuard.lock();
    bool localInitialized = initialized;
    lifecycleDataGuard.unlock();

    g_return_val_if_fail(localInitialized, nullptr);

    g_autoptr(ThreadNetworkInfo) threadInfo = threadNetworkInfoCreate();
    g_return_val_if_fail(threadSubsystemGetNetworkInfo(threadInfo), nullptr);

    cJSON *retVal = threadNetworkInfoToJson(threadInfo);

    cJSON_AddBoolToObject(retVal, SUBSYSTEM_STATUS_COMMON_READY, localInitialized);

    return retVal;
}

static bool migrate(uint16_t oldVersion, uint16_t newVersion)
{
    return true;
}

bool threadSubsystemGetNetworkInfo(ThreadNetworkInfo *info)
{
    icDebug();

    g_return_val_if_fail(info, false);

    lifecycleDataGuard.lock();
    bool localInitialized = initialized;
    lifecycleDataGuard.unlock();

    bool retVal = false;

    /*
        Two methods we can supply a valid dataset:
            1. Normal happy path of having a valid connection to a running OTBR with an
                active network we created and lead (i.e. subsystem initialized).
            2. A fallback mechanism of having a string of hex digits available in device
               service system properties (output of ot-ctl dataset active -x) representing
               a valid thread network we don't directly manage.
    */

    if (localInitialized)
    {
        std::vector<uint8_t> networkDataTlvs = otClient->GetActiveDatasetTlvs();
        std::vector<uint8_t> networkKey = otClient->GetNetworkKey();
        std::string networkName = otClient->GetNetworkName();
        info->channel = otClient->GetChannel();
        info->panId = otClient->GetPanId();
        info->extendedPanId = otClient->GetExtPanId();

        g_return_val_if_fail(info->channel != UINT16_MAX, false);
        g_return_val_if_fail(info->panId != UINT16_MAX, false);
        g_return_val_if_fail(info->extendedPanId != UINT64_MAX, false);

        g_return_val_if_fail(!networkDataTlvs.empty() && networkDataTlvs.size() <= THREAD_OPERATIONAL_DATASET_MAX_LEN &&
                                 !networkKey.empty() && networkKey.size() <= THREAD_NETWORK_KEY_MAX_LEN,
                             false);

        std::memcpy(info->dataset, networkDataTlvs.data(), networkDataTlvs.size());
        info->datasetLen = networkDataTlvs.size();
        std::memcpy(info->networkKey, networkKey.data(), networkKey.size());
        info->networkKeyLen = networkKey.size();
        info->networkName = strdup(networkName.c_str());
        retVal = true;
    }
    else
    {
        // Check system properties for an override (legacy dev feature)
        scoped_generic char *tmpThreadDatasetChars = nullptr;
        if (deviceServiceGetSystemProperty(THREAD_OPERATIONAL_DATASET_FALLBACK_PROPERTY, &tmpThreadDatasetChars))
        {
            icLogInfo(LOG_TAG, "%s: using thread operational dataset from system properties", __func__);
            size_t dataSetLen = strlen(tmpThreadDatasetChars);
            for (size_t i = 0, k = 0; i < dataSetLen; i += 2, k++)
            {
                gint byte1 = g_ascii_xdigit_value(tmpThreadDatasetChars[i]);
                gint byte2 = g_ascii_xdigit_value(tmpThreadDatasetChars[i + 1]);
                info->dataset[k] = byte1 << 4 | byte2;
            }
            info->datasetLen = dataSetLen / 2;

            retVal = true;
        }
    }

    return retVal;
}

/**
 * Either create a new network or restore the existing one.
 */
static bool initializeThreadStack(void)
{
    icDebug();

    {
        std::lock_guard<std::mutex> lock(lifecycleDataGuard);

        if (!otClient)
        {
            otClient = new OpenThreadClient();
        }
    }

    bool result = false;

    scoped_generic char *blob = nullptr;

    if (deviceServiceGetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, &blob) == false || blob == nullptr ||
        strlen(blob) == 0)
    {
        icWarn("No network configuration loaded; a new network will be created");

        std::vector<uint8_t> networkTlvs = otClient->CreateNetwork(ZILKER_THREAD_NETWORK_NAME, THREAD_RADIO_CHANNEL);
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
            result = otClient->RestoreNetwork(tlvsVect);
            if (result == false)
            {
                icError("Failed to restore network configuration, trying again in %d seconds...", MONITOR_TASK_INTERVAL_SECS);
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

static bool monitorTaskFunc(void *arg)
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
        OpenThreadClient::DeviceRole role = otClient->GetDeviceRole();
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
