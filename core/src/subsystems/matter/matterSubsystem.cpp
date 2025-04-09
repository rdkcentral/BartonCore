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
 * Created by Thomas Lea on 3/11/21.
 */

#define G_LOG_DOMAIN "MatterSubsystem"
#define logFmt(fmt)  "MatterSubsystem (%s): " fmt, __func__
#include "MatterCommon.h"
#include <cassert>
#include <iomanip>

#include <libxml/parser.h>

extern "C" {
#include "deviceServiceConfiguration.h"
#include "event/deviceEventProducer.h"
#include "events/device-service-storage-changed-event.h"
#include "icTypes/sbrm.h"
#include "icUtil/array.h"
#include "icUtil/fileUtils.h"
#include "icUtil/stringUtils.h"
#include <device-driver/device-driver.h>
#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icUtil/base64.h>
#include <subsystemManager.h>
#include <subsystemManagerCallbacks.h>
}

#include "CommissioningOrchestrator.h"
#include "DiscoveredDeviceDetailsStore.h"
#include "Matter.h"
#include "matter/MatterDeviceDriver.h"
#include "matter/MatterDriverFactory.h"
#include "matterSubsystem.h"

using namespace barton;
using namespace std;

#define MATTER_CHIP_PREFIX                   "chip_"
#define MATTER_CHIP_CONFIG                   MATTER_CHIP_PREFIX "config.ini"
#define MATTER_CHIP_COUNTERS                 MATTER_CHIP_PREFIX "counters.ini"
#define MATTER_CHIP_FACTORY                  MATTER_CHIP_PREFIX "factory.ini"
#define MATTERKV_FILE_NAME                   "matterkv"
#define MIGRATED_FILE_SUFFIX                 "_0"

#define MATTER_INIT_ATTEMPT_INTERVAL_SECONDS 15U

static std::mutex subsystemMtx;
static bool initialized = false;
static bool busy = false;
static GMainLoop *matterInitLoop = nullptr;
static guint sourceId = 0;
static char *matterConfigRoot;
static char *matterKVPath;

static subsystemInitializedFunc subsystemInitializedCallback = nullptr;
static subsystemDeInitializedFunc subsystemDeinitializedCallback = nullptr;

static bool matterSubsystemInitialize(subsystemInitializedFunc initializedCallback,
                                      subsystemDeInitializedFunc deInitializedCallback);

static void matterSubsystemShutdown();

static void matterSubsystemAllServicesAvailable();

static bool matterSubsystemMigrate(uint16_t oldVersion, uint16_t newVersion);

static bool matterSubsystemRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath);

static void matterSubsystemPostRestoreConfig(void);

static void
onDeviceCommissioningStatusChanged(void *context, chip::Optional<chip::NodeId> nodeId, CommissioningStatus status);

static void matterInitLoopThreadFunc();

static gboolean maybeInitMatter(void *context);

static cJSON *getStatusJson();

static void accountIdChanged(const gchar *accountId);

__attribute__((constructor)) static void registerSubsystem()
{
    static struct Subsystem matterSubsystem = {.migrate = matterSubsystemMigrate,
                                               .initialize = matterSubsystemInitialize,
                                               .shutdown = matterSubsystemShutdown,
                                               .onAllServicesAvailable = matterSubsystemAllServicesAvailable,
                                               .onRestoreConfig = matterSubsystemRestoreConfig,
                                               .onPostRestoreConfig = matterSubsystemPostRestoreConfig,
                                               .getStatusJson = getStatusJson,
                                               .name = MATTER_SUBSYSTEM_NAME,
                                               .version = MATTER_SUBSYSTEM_VERSION};

    subsystemManagerRegister(&matterSubsystem);
}

static char *getAccountId()
{
#ifdef BARTON_CONFIG_MATTER_SELF_SIGNED_OP_CREDS_ISSUER
    // If we're using a self-signed CA, we don't care about the actual value of the account id.
    // This just gets us to a usable matter state in developer environments without activation.
    return strdup("1");
#else
    return deviceServiceConfigurationGetAccountId();
#endif
}

/**
 * @brief Attempts to initialize our matter stack.
 *
 * @return TRUE - When there was an error and the caller should try again
 * @return FALSE - When we successfully initialized matter and the caller should not call this function again
 */
static gboolean maybeInitMatter(void *context)
{
    (void) context;

    bool initSuccessful = true;

    {
        std::lock_guard<std::mutex> l(subsystemMtx);

        if (initialized || busy)
        {
            // If we've already initialized or some other thread is already running this function, then don't run this
            // again.
            return false;
        }

        busy = true;
    }

    try
    {
        scoped_generic char *accountIdStr = getAccountId();
        uint64_t accountId = 0;
        if (!stringToUint64(accountIdStr, &accountId) || accountId == 0)
        {
            icError("failed to retrieve account ID");
            initSuccessful = false;
        }
        else
        {
            scoped_generic char *trustStore = deviceServiceConfigurationGetMatterAttestationTrustStoreDir();
            std::string attestationTrustStorePath(trustStore);

            if (!Matter::GetInstance().Init(accountId, std::move(attestationTrustStorePath)))
            {
                initSuccessful = false;
            }

            // FIXME: Matter shouldn't be a singleton with Init() but a properly constructed object.

            // Start() is the final arbiter of truth. If Init() fails, it's presumed Start()
            // will definitely fail. Once Start()ed, the subsystem is up, and Start() SHOULD NOT be called again.
            // Start() is internally protected and will warn if already running (instead of aborting in the matter
            // stack).
            initSuccessful = Matter::GetInstance().Start();

            if (!initSuccessful)
            {
                icError("failed to start Matter interface");
            }
        }
    }
    catch (const runtime_error &e)
    {
        icError("failed to initialize/start Matter interface: %s", e.what());
    }

    subsystemMtx.lock();
    initialized = initSuccessful;
    busy = false;
    subsystemMtx.unlock();

    if (initSuccessful)
    {
        subsystemInitializedCallback(MATTER_SUBSYSTEM_NAME);
    }

    return initSuccessful ? false : true;
}

static void matterInitLoopThreadFunc()
{
    /*
    Attempt to init matter now. If it doesn't work (returns true), use g_timeout_add_seconds to
    add a new source to the thread GMainContext and try to initialize matter on a repeating time
    interval until it works.

    FIXME: We could optimize this by having our own custom source to manage on an event basis
    instead of a timer. The main issue with that approach is that we have two external
    dependencies that need to be "ready" before we will succed:
        1. account id - can be set at any time by the client application
        2. token provider - can fail for any reason (eg. not ready) and has no event to notify us
    */
    if (maybeInitMatter(nullptr))
    {
        subsystemMtx.lock();

        // Create our own GMainContext for this thread. We can't use the global default as matter
        // uses it for its own loop and attempting to init matter what holding the global default context
        // will create a deadlock.
        g_autoptr(GMainContext) thisThreadContext = g_main_context_new();
        g_main_context_push_thread_default(thisThreadContext);
        matterInitLoop = g_main_loop_new(thisThreadContext, false);

        // Create a timer source. This is what g_timeout_add_* does but it attaches to the global default context. We
        // need to attach to our new context, so make the source by hand.
        GSource *source = g_timeout_source_new(MATTER_INIT_ATTEMPT_INTERVAL_SECONDS * 1000);
        g_source_set_priority(source, G_PRIORITY_DEFAULT);
        g_source_set_callback(source, maybeInitMatter, nullptr, nullptr);
        g_source_set_name(source, "maybeInitMatter");
        sourceId = g_source_attach(source, thisThreadContext);
        g_source_unref(source); // Forwarded to the context.

        icDebug("Scheduling maybeInitMatter on %u second interval with source id %u",
                MATTER_INIT_ATTEMPT_INTERVAL_SECONDS,
                sourceId);

        g_autoptr(GMainLoop) localLoopCopy = g_main_loop_ref(matterInitLoop);

        subsystemMtx.unlock();

        g_main_loop_run(localLoopCopy);
    }
}

static void
matterChanged(GFileMonitor *self, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
    bool notify = false;

    switch (event_type)
    {
        case G_FILE_MONITOR_EVENT_MOVED_IN:
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_DELETED:
        case G_FILE_MONITOR_EVENT_CREATED:
            g_debug("File '%s' changed", g_file_peek_path(file));
            notify = true;
            break;

        case G_FILE_MONITOR_EVENT_RENAMED:
            g_debug("File '%s' renamed from '%s'", g_file_peek_path(other_file), g_file_peek_path(file));
            notify = true;
            break;

        default:
            // Not interested
            break;
    }

    if (notify)
    {
        sendStorageChangedEvent(event_type);
    }
}

static GFileMonitor *matterMon;

static void monitorMatterConfig(void)
{
    if (matterMon == nullptr)
    {
        g_autoptr(GFile) matterKV = g_file_new_for_path(matterKVPath);
        g_autoptr(GError) err = nullptr;

        matterMon = g_file_monitor_file(
            /* file */ matterKV,
            /* flags */ G_FILE_MONITOR_WATCH_MOVES,
            /* cancellable */ nullptr,
            /* error */ &err);

        if (err != nullptr)
        {
            g_critical("Failed to monitor '%s': %s", matterKVPath, err->message);
            g_object_unref(g_steal_pointer(&matterKV));

            return;
        }

        g_signal_connect(matterMon, "changed", G_CALLBACK(matterChanged), nullptr);
    }
}

static bool matterSubsystemInitialize(subsystemInitializedFunc initializedCallback,
                                      subsystemDeInitializedFunc deInitializedCallback)
{
    icDebug();

    subsystemInitializedCallback = initializedCallback;
    subsystemDeinitializedCallback = deInitializedCallback;

    free(g_steal_pointer(&matterKVPath));
    free(g_steal_pointer(&matterConfigRoot));

    matterConfigRoot = deviceServiceConfigurationGetMatterStorageDir();

    if (matterConfigRoot == nullptr)
    {
        icError("Matter config directory not set and is required for the Matter subsystem.");
        return false;
    }

    matterKVPath = stringBuilder("%s/%s", matterConfigRoot, MATTERKV_FILE_NAME);

    monitorMatterConfig();

    if (!deviceServiceConfigurationRegisterAccountIdListener(accountIdChanged))
    {
        icWarn("Failed to register account id listener");
    }

    // Attempt to init matter in a different thread. This may need to make use of an event queue for retries,
    // so giving it its own thread to not hold this one up.
    std::thread matterInitThread = std::thread(matterInitLoopThreadFunc);
    matterInitThread.detach();

    return true;
}

static void matterSubsystemShutdown()
{
    icDebug();

    subsystemMtx.lock();
    initialized = false;
    if (sourceId)
    {
        g_source_remove(sourceId);
        sourceId = 0;
    }
    if (matterInitLoop)
    {
        g_main_loop_quit(matterInitLoop);
        g_main_loop_unref(matterInitLoop);
        matterInitLoop = nullptr;
    }
    subsystemMtx.unlock();

    Matter::GetInstance().Stop();

    g_object_unref(g_steal_pointer(&matterMon));
    free(g_steal_pointer(&matterKVPath));
    free(g_steal_pointer(&matterConfigRoot));

    if (!deviceServiceConfigurationUnregisterAccountIdListener(accountIdChanged))
    {
        icWarn("Failed to unregister account id listener");
    }

    subsystemDeinitializedCallback(MATTER_SUBSYSTEM_NAME);
}

static void matterSubsystemAllServicesAvailable()
{
    icDebug();
}

bool matterSubsystemCommissionDevice(const char *setupPayload, uint16_t timeoutSeconds)
{
    icDebug();

    CommissioningOrchestrator orchestrator(onDeviceCommissioningStatusChanged, nullptr);
    return orchestrator.Commission(setupPayload, timeoutSeconds);
}

bool matterSubsystemPairDevice(uint64_t nodeId, uint16_t timeoutSeconds)
{
    icDebug();

    CommissioningOrchestrator orchestrator(onDeviceCommissioningStatusChanged, nullptr);
    return orchestrator.Pair(nodeId, timeoutSeconds);
}

bool matterSubsystemOpenCommissioningWindow(const char *nodeId, uint16_t timeoutSeconds, char **setupCode, char **qrCode)
{
    bool result = false;

    chip::NodeId _nodeId = 0;
    std::string _setupCode;
    std::string _qrCode;

    if (setupCode == nullptr || qrCode == nullptr)
    {
        icError("Invalid arguments");
        return false;
    }

    if (nodeId != nullptr)
    {
        if (!stringToUint64(nodeId, &_nodeId))
        {
            icError("Invalid nodeId");
            return false;
        }
    }

    result = Matter::GetInstance().OpenCommissioningWindow(_nodeId, timeoutSeconds, _setupCode, _qrCode);

    if (result)
    {
        *setupCode = strdup(_setupCode.c_str());
        *qrCode = strdup(_qrCode.c_str());
    }

    return result;
}

bool matterSubsystemClearAccessRestrictionList(void)
{
    return Matter::GetInstance().ClearAccessRestrictionList();
}

static bool matterSubsystemMigrate(uint16_t oldVersion, uint16_t newVersion)
{
    icDebug();

    // Nothing to do (how did we get here?)
    if (oldVersion == newVersion)
    {
        return true;
    }

    bool retVal = true;

    /*
        For now, move anything pre matter subsystem version 1 to backup files as it may be unusable (but don't
        outright delete in case we need to recover something)
        FIXME: We really shouldn't know the filesystem path to matterkv here, it's a build option for the matter
        sdk which isn't currently set by the barton src build. In the future, we should centralize this as a macro
        baked in by the build.
        TODO: Refactor for multiple versions if the need arises.
    */
    scoped_generic char *dynamicConfigDir = deviceServiceConfigurationGetStorageDir();
    scoped_generic char *chipConfigPath = stringBuilder("%s/%s", dynamicConfigDir, MATTER_CHIP_CONFIG);
    scoped_generic char *chipCountersPath = stringBuilder("%s/%s", dynamicConfigDir, MATTER_CHIP_COUNTERS);
    scoped_generic char *chipFactoryPath = stringBuilder("%s/%s", dynamicConfigDir, MATTER_CHIP_FACTORY);

    scoped_generic char *migratedChipConfigPath = stringBuilder("%s%s", chipConfigPath, MIGRATED_FILE_SUFFIX);
    scoped_generic char *migratedChipCountersPath = stringBuilder("%s%s", chipCountersPath, MIGRATED_FILE_SUFFIX);
    scoped_generic char *migratedChipFactoryPath = stringBuilder("%s%s", chipFactoryPath, MIGRATED_FILE_SUFFIX);
    scoped_generic char *migratedMatterKVPath = stringBuilder("%s%s", matterKVPath, MIGRATED_FILE_SUFFIX);

    const char *const configPaths[] = {chipConfigPath, chipCountersPath, chipFactoryPath, matterKVPath};

    const char *const migratedPaths[] = {
        migratedChipConfigPath, migratedChipCountersPath, migratedChipFactoryPath, migratedMatterKVPath};

    for (size_t i = 0; i < ARRAY_LENGTH(configPaths); i++)
    {
        const char *configPath = configPaths[i];
        const char *migratedPath = migratedPaths[i];
        if (doesFileExist(configPath))
        {
            retVal &= moveFile(configPath, migratedPath);
        }
    }

    return retVal;
}

/**
 * @brief Restore config for RMA
 */
static bool matterSubsystemRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    icDebug();

    // Matter must be stopped before these config files are restored, else the stack will clobber them
    matterSubsystemShutdown();

    bool result = true;

    scoped_generic char *restoreChipConfigPath = stringBuilder("%s/%s", tempRestoreDir, MATTER_CHIP_CONFIG);
    scoped_generic char *restoreChipCountersPath = stringBuilder("%s/%s", tempRestoreDir, MATTER_CHIP_COUNTERS);
    scoped_generic char *restoreChipFactoryPath = stringBuilder("%s/%s", tempRestoreDir, MATTER_CHIP_FACTORY);
    scoped_generic char *restoreMatterKVPath = stringBuilder("%s/%s", tempRestoreDir, MATTERKV_FILE_NAME);
    const char *const restoreConfigPaths[] = {
        restoreChipConfigPath, restoreChipCountersPath, restoreChipFactoryPath, restoreMatterKVPath};

    scoped_generic char *destChipConfigPath = stringBuilder("%s/%s", dynamicConfigPath, MATTER_CHIP_CONFIG);
    scoped_generic char *destChipCountersPath = stringBuilder("%s/%s", dynamicConfigPath, MATTER_CHIP_COUNTERS);
    scoped_generic char *destChipFactoryPath = stringBuilder("%s/%s", dynamicConfigPath, MATTER_CHIP_FACTORY);
    scoped_generic char *destMatterKVPath = stringBuilder("%s/%s", dynamicConfigPath, MATTERKV_FILE_NAME);
    const char *const destConfigPaths[] = {
        destChipConfigPath, destChipCountersPath, destChipFactoryPath, destMatterKVPath};

    assert(ARRAY_LENGTH(restoreConfigPaths) == ARRAY_LENGTH(destConfigPaths));

    for (size_t i = 0; i < ARRAY_LENGTH(restoreConfigPaths); i++)
    {
        const char *restoreConfigPath = restoreConfigPaths[i];
        const char *destConfigPath = destConfigPaths[i];
        if (doesNonEmptyFileExist(restoreConfigPath))
        {
            icInfo("Restoring Matter config file from temp restore path %s", restoreConfigPath);
            result &= copyFileByPath(restoreConfigPath, destConfigPath);
        }
        else
        {
            icWarn("Missing Matter config file %s, cannot restore", restoreConfigPath);
        }
    }

    if (!result)
    {
        icWarn("Failed to restore some or all Matter config files");
    }

    return result;
}

static void matterSubsystemPostRestoreConfig(void)
{
    // icDebug();

    // FIXME: If this subsystem stops itself to carry out its restore config process, then it should also,
    // ideally, restart itself, rather than rely on the external knowledge that the device service as a whole
    // will always be restarted after a restore. However, doing the restart here currently causes the restored
    // configs to become mangled down the line, so we will rely on said external knowledge for the time being.
    // matterSubsystemInitialize(subsystemInitializedCallback, subsystemDeinitializedCallback);
}

static void
onDeviceCommissioningStatusChanged(void *context, chip::Optional<chip::NodeId> nodeId, CommissioningStatus status)
{
    std::stringstream ss;
    ss << status;
    icInfo("Commissioning status = %s", ss.str().c_str());
}

static cJSON *getStatusJson()
{
    icDebug();

    auto nodeId = Matter::GetInstance().GetCommissioner().GetNodeId();

    cJSON *result = cJSON_CreateObject();

    subsystemMtx.lock();
    bool localInitialized = initialized;
    subsystemMtx.unlock();

    cJSON_AddBoolToObject(result, SUBSYSTEM_STATUS_COMMON_READY, localInitialized);

    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(16) << std::hex << nodeId;
    cJSON_AddStringToObject(result, SUBSYSTEM_STATUS_MATTER_NODE_ID, ss.str().c_str());

    auto currentIpk = Matter::GetInstance().GetCurrentIPK();

    if (currentIpk != nullptr)
    {
        scoped_generic char *b64IPK = icEncodeBase64(currentIpk->Bytes(), currentIpk->Length());

        cJSON_AddStringToObject(result, SUBSYSTEM_STATUS_MATTER_IPK, b64IPK);
    }
    else
    {
        cJSON_AddNullToObject(result, SUBSYSTEM_STATUS_MATTER_IPK);
    }

    return result;
}

static void accountIdChanged(const gchar *accountId)
{
    std::thread matterInitThread = std::thread(maybeInitMatter, nullptr);
    matterInitThread.detach();
}
