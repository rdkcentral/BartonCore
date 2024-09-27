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
 * Created by Thomas Lea on 11/15/22.
 */

#define LOG_TAG     "MatterChimeDD"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterChimeDeviceDriver.h"
#include "MatterDriverFactory.h"
#include "clusters/LevelControl.h"

extern "C" {
#include "device/icDeviceResource.h"
#include "deviceServiceConfiguration.h"
#include "deviceServiceProps.h"
#include "provider/device-service-property-provider.h"
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>
#include <resourceTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
}

#include <chrono>
#include <cinttypes>
#include <controller-clusters/zap-generated/CHIPClusters.h>
#include <subsystems/matter/DiscoveredDeviceDetailsStore.h>
#include <subsystems/matter/Matter.h>

using namespace zilker;
using chip::Callback::Callback;
using namespace std::chrono_literals;

#define DEVICE_DRIVER_NAME "matterChime"

#define DC_VERSION 1

// these are our endpoints, not the device's
#define CHIME_ENDPOINT "1"
#define CHIME_WARNING_DEVICE_ENDPOINT "2"

#define DEFAULT_SIREN_URI "tag:siren:options?ledPattern=pattern4&volume=100&durationSecs=240&force=true&priority=true"

namespace
{
    constexpr uint32_t IDENTIFIER_LEN = 4;

    struct ClusterReadContext
    {
        std::promise<bool> *operationComplete;          // Indicates read completion with success/fail
        icInitialResourceValues *initialResourceValues; // non-null if this read is the initial resource fetch
        char **value;                                   // non-null if this read is a regular resource read
    };
} // namespace

// auto register with the factory
bool MatterChimeDeviceDriver::registeredWithFactory =
    MatterDriverFactory::Instance().RegisterDriver(new MatterChimeDeviceDriver());

MatterChimeDeviceDriver::MatterChimeDeviceDriver() : MatterDeviceDriver(DEVICE_DRIVER_NAME, CHIME_DC, DC_VERSION)
{
    chimeClusterEventHandler.deviceDriver = this;
    levelControlClusterEventHandler.deviceDriver = this;
    wifiDiagnosticsClusterEventHandler.deviceDriver = this;
}

bool MatterChimeDeviceDriver::ClaimDevice(DiscoveredDeviceDetails *details)
{
    icDebug();

    // see if any endpoint (not the special 0 entry) has our device id
    for (auto &entry : details->endpointDescriptorData)
    {
        if (entry.first > 0)
        {
            for (auto &deviceTypeEntry : *entry.second->deviceTypes)
            {
                switch (deviceTypeEntry)
                {
                    case MATTER_SPEAKER_DEVICE_ID:
                        return true;

                    default:
                        break;
                }
            }
        }
    }

    return false;
}

std::vector<MatterCluster *> MatterChimeDeviceDriver::GetClustersToSubscribeTo(const std::string &deviceId)
{
    icDebug();

    auto chimeServer = static_cast<ComcastChime *>(GetAnyServerById(deviceId, CHIME_CLUSTER_ID));
    if (chimeServer == nullptr)
    {
        icError("Chime cluster not present on device %s!", deviceId.c_str());
        return {};
    }

    auto wifiDiagServer = static_cast<WifiNetworkDiagnostics *>(
        GetAnyServerById(deviceId, chip::app::Clusters::WiFiNetworkDiagnostics::Id));

    if (wifiDiagServer == nullptr)
    {
        icError("Wifi Diagnostics cluster not present on device %s!", deviceId.c_str());
        return {};
    }

    std::vector<MatterCluster *> clusters;
    clusters.push_back(chimeServer);
    clusters.push_back(wifiDiagServer);
    return clusters;
}

void MatterChimeDeviceDriver::SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                const std::string &deviceId,
                                                chip::Messaging::ExchangeManager &exchangeMgr,
                                                const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // Configuring attribute reporting triggers reports to be sent which refreshes our local cache.  Attributes
    //  that are not configured for reporting will need to be read independently.
    ConfigureDevice(promises, deviceId, nullptr, exchangeMgr, sessionHandle);
}

void MatterChimeDeviceDriver::FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                         const std::string &deviceId,
                                                         icInitialResourceValues *initialResourceValues,
                                                         chip::Messaging::ExchangeManager &exchangeMgr,
                                                         const chip::SessionHandle &sessionHandle)
{
    bool result = false;

    icDebug();

    auto chimeServer = static_cast<ComcastChime *>(GetAnyServerById(deviceId, CHIME_CLUSTER_ID));

    if (chimeServer == nullptr)
    {
        icError("Chime cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromise = promises.front();
    auto readContext = new ClusterReadContext {};
    readContext->operationComplete = &readPromise;
    readContext->initialResourceValues = initialResourceValues;

    // Paired with AudioAssetsReadComplete
    AssociateStoredContext(readContext->operationComplete);

    if (!chimeServer->GetAudioAssets(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromise);
        delete readContext;
    }

    auto wifiDiagServer = static_cast<WifiNetworkDiagnostics *>(GetAnyServerById(deviceId, chip::app::Clusters::WiFiNetworkDiagnostics::Id));

    if (wifiDiagServer == nullptr)
    {
        icError("Wifi Diagnostics cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromiseWifiDiag = promises.front();
    readContext = new ClusterReadContext {};
    readContext->operationComplete = &readPromiseWifiDiag;
    readContext->initialResourceValues = initialResourceValues;

    // Paired with WifiNetworkDiagnosticsEventHandler::RssiReadComplete
    AssociateStoredContext(readContext->operationComplete);

    if (!wifiDiagServer->GetRssi(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromiseWifiDiag);
        delete readContext;
    }

    auto levelControlServer =
        static_cast<LevelControl *>(GetAnyServerById(deviceId, chip::app::Clusters::LevelControl::Id));

    if (levelControlServer == nullptr)
    {
        icError("Level Control cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromiseCurrentLevel = promises.front();
    readContext = new ClusterReadContext {};
    readContext->operationComplete = &readPromiseCurrentLevel;
    readContext->initialResourceValues = initialResourceValues;

    // Paired with LevelControlClusterEventHandler::CurrentLevelReadComplete
    AssociateStoredContext(readContext->operationComplete);

    if (!levelControlServer->GetCurrentLevel(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromiseCurrentLevel);
        delete readContext;
    }
}

bool MatterChimeDeviceDriver::RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icDebug();

    icDeviceEndpoint *chimeEndpoint = createEndpoint(device, CHIME_ENDPOINT, CHIME_PROFILE, true);
    icDeviceEndpoint *warningDeviceEndpoint =
        createEndpoint(device, CHIME_WARNING_DEVICE_ENDPOINT, WARNING_DEVICE_PROFILE, true);
    warningDeviceEndpoint->profileVersion = WARNING_DEVICE_PROFILE_VERSION;

    //TODO these are common device resources and should probably be moved up to MatterDeviceDriver if we know its a wifi device
    const char *initialRssi = initialResourceValuesGetDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FERSSI);
    result &= createDeviceResource(device,
                                   COMMON_DEVICE_RESOURCE_FERSSI,
                                   initialRssi,
                                   RESOURCE_TYPE_SIGNAL_STRENGTH,
                                   RESOURCE_MODE_READABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                   CACHING_POLICY_ALWAYS) != NULL;

    scoped_generic char *scoreStr = nullptr;
    int8_t rssi = 0;
    if (initialRssi && stringToInt8(initialRssi, &rssi))
    {
        scoreStr = stringBuilder("%" PRIu8, getLinkScore(rssi));
    }

    result &= createDeviceResource(device,
                                   COMMON_DEVICE_RESOURCE_LINK_SCORE,
                                   scoreStr,
                                   RESOURCE_TYPE_PERCENTAGE,
                                   RESOURCE_MODE_READABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                   CACHING_POLICY_ALWAYS) != NULL;

    auto chimeLabel = CreateChimeLabel(device);

    result &= createEndpointResource(chimeEndpoint,
                                     COMMON_ENDPOINT_RESOURCE_LABEL,
                                     chimeLabel.c_str(),
                                     RESOURCE_TYPE_LABEL,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    result &= createEndpointResource(chimeEndpoint,
                                     CHIME_PROFILE_RESOURCE_PLAY_URL,
                                     nullptr,
                                     RESOURCE_TYPE_URL,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != nullptr;

    result &= createEndpointResource(chimeEndpoint,
                                     CHIME_PROFILE_RESOURCE_SNOOZE_UNTIL,
                                     nullptr,
                                     RESOURCE_TYPE_DATETIME,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    const char *initialVolumeValue =
        initialResourceValuesGetEndpointValue(initialResourceValues, CHIME_ENDPOINT, CHIME_PROFILE_RESOURCE_VOLUME);

    result &= createEndpointResource(chimeEndpoint,
                                     CHIME_PROFILE_RESOURCE_VOLUME,
                                     initialVolumeValue,
                                     RESOURCE_TYPE_PERCENTAGE,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    const char *initialAudioAssetsValue = initialResourceValuesGetEndpointValue(
        initialResourceValues, CHIME_ENDPOINT, CHIME_PROFILE_RESOURCE_AUDIO_ASSET_LIST);

    result &= createEndpointResource(chimeEndpoint,
                                     CHIME_PROFILE_RESOURCE_AUDIO_ASSET_LIST,
                                     initialAudioAssetsValue,
                                     RESOURCE_TYPE_AUDIO_ASSETS,
                                     RESOURCE_MODE_READABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResource(warningDeviceEndpoint,
                                     WARNING_DEVICE_RESOURCE_TONE,
                                     WARNING_DEVICE_TONE_NONE,
                                     RESOURCE_TYPE_WARNING_TONE,
                                     RESOURCE_MODE_READWRITEABLE,
                                     CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResource(warningDeviceEndpoint,
                                     WARNING_DEVICE_RESOURCE_SILENCED,
                                     "true",
                                     RESOURCE_TYPE_BOOLEAN,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != NULL;

    return result;
}

void MatterChimeDeviceDriver::ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                                              const std::string &deviceId,
                                              icDeviceResource *resource,
                                              const char *arg,
                                              char **response,
                                              chip::Messaging::ExchangeManager &exchangeMgr,
                                              const chip::SessionHandle &sessionHandle)
{
    icDebug("%s: %s", resource->id, arg);

    if (resource->endpointId != nullptr)
    {
        if (stringCompare(resource->id, CHIME_PROFILE_RESOURCE_PLAY_URL, false) == 0)
        {
            PlaySound(promises, deviceId, arg, exchangeMgr, sessionHandle);
        }
    }
}

bool MatterChimeDeviceDriver::WriteResource(std::forward_list<std::promise<bool>> &promises,
                                            const std::string &deviceId,
                                            icDeviceResource *resource,
                                            const char *previousValue,
                                            const char *newValue,
                                            chip::Messaging::ExchangeManager &exchangeMgr,
                                            const chip::SessionHandle &sessionHandle)
{
    icDebug("%s", resource->id);

    if (resource->endpointId == nullptr)
    {
        return false;
    }

    bool shouldUpdateResource = true;

    if (stringCompare(resource->id, CHIME_PROFILE_RESOURCE_SNOOZE_UNTIL, false) == 0)
    {
        // here we just verify that its a valid value then update the resource with what was provided
        uint64_t timestamp = 0;
        if (stringToUint64(newValue, &timestamp))
        {
            icInfo("Snooze updated to %" PRIu64, timestamp);
        }
        else
        {
            FailOperation(promises);
        }
    }

    else if (stringCompare(resource->id, CHIME_PROFILE_RESOURCE_VOLUME, false) == 0)
    {
        int64_t num = 0;

        if (stringToNumberWithinRange(newValue, &num, 10, 0, 100))
        {
            auto *server = static_cast<LevelControl *>(GetAnyServerById(deviceId, LEVEL_CONTROL_CLUSTER_ID));
            if (server == nullptr)
            {
                icError("Level Control cluster not present on device %s!", deviceId.c_str());
            }
            else
            {
                uint8_t volume = (uint8_t) num;
                promises.emplace_front();
                auto &levelPromise = promises.front();

                // Paired with LevelControlClusterEventHandler::CommandCompleted
                AssociateStoredContext(&levelPromise);

                if (!server->MoveToLevel(&levelPromise, volume, exchangeMgr, sessionHandle))
                {
                    AbandonDeviceWork(levelPromise);
                }
            }
        }
        else
        {
            icError("Invalid volume percent %s", newValue);
            FailOperation(promises);
        }
    }

    else if (stringCompare(resource->id, WARNING_DEVICE_RESOURCE_TONE, false) == 0)
    {
        if (IsSilenced(deviceId.c_str()))
        {
            icInfo("Chime warning device is silenced; ignoring writes to tone resource");
            shouldUpdateResource = false;
        }
        else if (stringCompare(newValue, WARNING_DEVICE_TONE_FIRE, false) == 0 ||
                 stringCompare(newValue, WARNING_DEVICE_TONE_CO, false) == 0)
        {
            icDebug("Chime not configured for fire or CO alarms; ignoring");
            shouldUpdateResource = false;
        }
        else if (stringCompare(newValue, WARNING_DEVICE_TONE_WARBLE, false) == 0)
        {
            g_autoptr(BDeviceServicePropertyProvider) provider = deviceServiceConfigurationGetPropertyProvider();
            scoped_generic char *uri = b_device_service_property_provider_get_property_as_string(
                provider, DEVICE_PROP_MATTER_SIREN_URI, DEFAULT_SIREN_URI);
            PlaySound(promises,
                      deviceId,
                      uri,
                      exchangeMgr,
                      sessionHandle);
        }
        else if (stringCompare(newValue, WARNING_DEVICE_TONE_NONE, false) == 0)
        {
            StopSound(promises, deviceId, exchangeMgr, sessionHandle);
        }
        else
        {
            icError("Invalid tone: %s", newValue);
            FailOperation(promises);
        }
    }

    else if (stringCompare(resource->id, WARNING_DEVICE_RESOURCE_SILENCED, false) == 0)
    {
        bool silenced;
        if (stringToBoolStrict(newValue, &silenced))
        {
            if (silenced)
            {
                // There should be no siren going off on a 'silenced' warning device
                scoped_icDeviceResource *toneRes = deviceServiceGetResourceById(deviceId.c_str(),
                                                                                CHIME_WARNING_DEVICE_ENDPOINT,
                                                                                WARNING_DEVICE_RESOURCE_TONE);
                if (stringCompare(toneRes->value, WARNING_DEVICE_TONE_NONE, false) != 0)
                {
                    StopSound(promises, deviceId, exchangeMgr, sessionHandle);
                    updateResource(deviceId.c_str(),
                                   CHIME_WARNING_DEVICE_ENDPOINT,
                                   WARNING_DEVICE_RESOURCE_TONE,
                                   WARNING_DEVICE_TONE_NONE,
                                   nullptr);
                }
            }
        }
        else
        {
            FailOperation(promises);
            icError("Invalid argument: %s", newValue);
        }
    }

    return shouldUpdateResource;
}

void MatterChimeDeviceDriver::PlaySound(std::forward_list<std::promise<bool>> &promises,
                                        const std::string &deviceId,
                                        const char *uriStr,
                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                        const chip::SessionHandle &sessionHandle)
{
    icDebug("%s", uriStr);

    g_autoptr(GUri) uri = g_uri_parse(uriStr, G_URI_FLAGS_ENCODED_QUERY, nullptr);

    if (!uri)
    {
        icError("Invalid URL: %s", uriStr);
        FailOperation(promises);
        return;
    }

    if (shouldPlaySound(deviceId.c_str(), uri))
    {
        auto *server = static_cast<ComcastChime *>(GetAnyServerById(deviceId, CHIME_CLUSTER_ID));

        if (server == nullptr)
        {
            icError("Chime cluster not present on device %s!", deviceId.c_str());
            FailOperation(promises);
            return;
        }

        promises.emplace_front();
        auto &playPromise = promises.front();

        // Paired with ChimeClusterEventHandler::CommandCompleted
        AssociateStoredContext(&playPromise);

        if (!server->PlayUrl(&playPromise, uriStr, exchangeMgr, sessionHandle))
        {
            AbandonDeviceWork(playPromise);
        }
    }
    else
    {
        // if we decided not to play the sound, that is still a success
        // Not promising any work will be done is success
    }
}

void MatterChimeDeviceDriver::StopSound(std::forward_list<std::promise<bool>> &promises,
                                        const std::string &deviceId,
                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                        const chip::SessionHandle &sessionHandle)
{
    icDebug();

    auto *server = static_cast<ComcastChime *>(GetAnyServerById(deviceId, CHIME_CLUSTER_ID));

    if (server == nullptr)
    {
        icError("Chime cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &stopPromise = promises.front();

    // Paired with ChimeClusterEventHandler::CommandCompleted
    AssociateStoredContext(&stopPromise);

    if (!server->StopPlay(&stopPromise, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(stopPromise);
    }
}

std::string MatterChimeDeviceDriver::CreateChimeLabel(icDevice *device)
{
    std::string chimeLabelPrefix("Chime ");

    // A valid chime label is "Chime <unique idenitifer>" where the idenitifier can be anything,
    // but is preferably the last 4 characters of the mac address or serial number.
    const char *const resources[] = {COMMON_DEVICE_RESOURCE_MAC_ADDRESS, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER};

    for (size_t i = 0; i < ARRAY_LENGTH(resources); i++)
    {
        icDeviceResource *res = deviceServiceFindDeviceResourceById(device, resources[i]);
        if (res && res->value)
        {
            std::string resourceValue(res->value);
            if (resourceValue.size() >= IDENTIFIER_LEN)
            {
                icDebug("Using %s resource value to form unique identifier for chime label", resources[i]);
                return chimeLabelPrefix + resourceValue.substr(resourceValue.size() - IDENTIFIER_LEN);
            }
        }
    }

    scoped_generic char *identifier = generateRandomToken(IDENTIFIER_LEN, IDENTIFIER_LEN, 0);
    icWarn("Failed to retrieve both valid mac addr and serial num for device %s; "
           "using this randomly generated 4-character identifier '%s' for chime label instead",
           device->uuid,
           identifier);
    return chimeLabelPrefix + identifier;
}

bool MatterChimeDeviceDriver::shouldPlaySound(const char *deviceUuid, GUri *uri)
{
    // if we are in a snooze window, we shouldn't play a sound and just log a message, unless the force parameter is set
    // to true.
    scoped_icDeviceResource *snoozeRes =
        deviceServiceGetResourceById(deviceUuid, CHIME_ENDPOINT, CHIME_PROFILE_RESOURCE_SNOOZE_UNTIL);

    uint64_t snoozeUntil = 0;
    bool snooze = (snoozeRes != nullptr) && stringToUint64(snoozeRes->value, &snoozeUntil) &&
                  (getCurrentUnixTimeMillis() < snoozeUntil);
    bool forced = false;

    if (snooze)
    {
        // check to see if the force query string arg was set
        g_autoptr(GHashTable) query_params =
            g_uri_parse_params(g_uri_get_query(uri), -1, "&", G_URI_PARAMS_NONE, nullptr);
        if (query_params)
        {
            forced = stringToBool((char *) g_hash_table_lookup(query_params, "force"));
        }
    }

    if (snooze)
    {
        if (forced)
        {
            icInfo("Force play sound, ignoring snooze on %s.", snoozeRes->deviceUuid);
        }
        else
        {
            icInfo("Snooze active on %s, ignoring request.", snoozeRes->deviceUuid);
        }
    }

    return !snooze || forced;
}

bool MatterChimeDeviceDriver::IsSilenced(const char *deviceUuid)
{
    icDebug();

    scoped_icDeviceResource *silencedRes =
        deviceServiceGetResourceById(deviceUuid, CHIME_WARNING_DEVICE_ENDPOINT, WARNING_DEVICE_RESOURCE_SILENCED);

    if (silencedRes != nullptr)
    {
        bool silenced;
        if (stringToBoolStrict(silencedRes->value, &silenced))
        {
            return silenced;
        }
    }

    icWarn("Failed to read silenced resource; assuming silenced is true by default");
    return true;
}

uint8_t MatterChimeDeviceDriver::getLinkScore(int8_t rssi)
{
    static double DBM_FLOOR = -90.0;
    static double DBM_CEIL = -20.0;

    // Clamp rssi to reasonable boundaries
    rssi = (rssi < DBM_FLOOR) ? DBM_FLOOR : (rssi > DBM_CEIL) ? DBM_CEIL : rssi;

    // This will be a number between 0 and 1 where closer to 0 is close to the ceiling (good) and closer to 1 is close to the floor (bad).
    double normalizedRSSI = (DBM_CEIL - rssi) / (DBM_CEIL - DBM_FLOOR);

    // A normalizedRSSI close to 0 (good) should cause the second subtraction operand to be small, leading to a percentage close to 100.
    // A normalizedRSSI close to 1 (bad) should cause the second subtraction operand being close to 100, leading to a percentage close to 0.
    uint8_t percentage = 100 - (100 * normalizedRSSI);

    return percentage;
}

std::unique_ptr<MatterCluster> MatterChimeDeviceDriver::MakeCluster(std::string const &deviceUuid,
                                                                    chip::EndpointId endpointId,
                                                                    chip::ClusterId clusterId)
{
    switch (clusterId)
    {
        case CHIME_CLUSTER_ID:
            return std::make_unique<ComcastChime>(
                (ComcastChime::EventHandler *) &chimeClusterEventHandler, deviceUuid, endpointId);
            break;

        case LEVEL_CONTROL_CLUSTER_ID:
            return std::make_unique<LevelControl>(
                (LevelControl::EventHandler *) &levelControlClusterEventHandler, deviceUuid, endpointId);
            break;

        case chip::app::Clusters::WiFiNetworkDiagnostics::Id:
            return std::make_unique<WifiNetworkDiagnostics>(
                (WifiNetworkDiagnostics::EventHandler *) &wifiDiagnosticsClusterEventHandler, deviceUuid, endpointId);
            break;

        default:
            return nullptr;
    }
}

void MatterChimeDeviceDriver::ChimeClusterEventHandler::AudioAssetsChanged(const std::string &deviceUuid,
                                                                           std::unique_ptr<std::string> assets,
                                                                           void *asyncContext)
{
    icDebug();

    const char *value = assets.get() ? assets.get()->c_str() : nullptr;
    updateResource(deviceUuid.c_str(), CHIME_ENDPOINT, CHIME_PROFILE_RESOURCE_AUDIO_ASSET_LIST, value, nullptr);
}

void MatterChimeDeviceDriver::ChimeClusterEventHandler::AudioAssetsReadComplete(const std::string &deviceUuid,
                                                                                std::unique_ptr<std::string> assets,
                                                                                void *asyncContext)
{
    icDebug();

    auto readContext = static_cast<ClusterReadContext *>(asyncContext);
    const char *value = assets.get() ? assets.get()->c_str() : nullptr;

    // TODO: instead of readContext, this work could be a in a functor (usually lambda) that receives assets and does
    // one of the other instead of inferring intent here
    if (readContext->initialResourceValues)
    {
        initialResourceValuesPutEndpointValue(
            readContext->initialResourceValues, CHIME_ENDPOINT, CHIME_PROFILE_RESOURCE_AUDIO_ASSET_LIST, value);
    }
    else
    {
        if (value != nullptr)
        {
            *readContext->value = strdup(value);
        }
        else
        {
            *readContext->value = nullptr;
        }
    }

    deviceDriver->OnDeviceWorkCompleted(readContext->operationComplete, true);

    delete readContext;
}

void MatterChimeDeviceDriver::LevelControlClusterEventHandler::CurrentLevelChanged(const std::string &deviceUuid,
                                                                                   uint8_t level,
                                                                                   void *asyncContext)
{
    icDebug();

    scoped_generic char *levelStr = stringBuilder("%" PRIu8, level);
    updateResource(deviceUuid.c_str(), CHIME_ENDPOINT, CHIME_PROFILE_RESOURCE_VOLUME, levelStr, nullptr);
}

void MatterChimeDeviceDriver::LevelControlClusterEventHandler::CurrentLevelReadComplete(const std::string &deviceUuid,
                                                                                        uint8_t level,
                                                                                        void *asyncContext)
{
    auto readContext = static_cast<ClusterReadContext *>(asyncContext);
    scoped_generic char *volumeStr = stringBuilder("%" PRIu8, level);

    if (readContext->initialResourceValues)
    {
        initialResourceValuesPutEndpointValue(
            readContext->initialResourceValues, CHIME_ENDPOINT, CHIME_PROFILE_RESOURCE_VOLUME, volumeStr);
    }
    else
    {
        *readContext->value = volumeStr;
        volumeStr = nullptr;
    }

    deviceDriver->OnDeviceWorkCompleted(readContext->operationComplete, true);

    delete readContext;
}

void MatterChimeDeviceDriver::WifiNetworkDiagnosticsEventHandler::RssiChanged(const std::string &deviceUuid, int8_t *rssi, void *asyncContext)
{
    icDebug();

    scoped_generic char *rssiStr = nullptr;
    scoped_generic char *scoreStr = nullptr;
    if (rssi)
    {
        rssiStr = stringBuilder("%" PRId8, *rssi);
        scoreStr = stringBuilder("%" PRIu8, getLinkScore(*rssi));
    }

    updateResource(deviceUuid.c_str(), nullptr, COMMON_DEVICE_RESOURCE_FERSSI, rssiStr, nullptr);
    updateResource(deviceUuid.c_str(), nullptr, COMMON_DEVICE_RESOURCE_LINK_SCORE, scoreStr, nullptr);
}

void MatterChimeDeviceDriver::WifiNetworkDiagnosticsEventHandler::RssiReadComplete(const std::string &deviceUuid, int8_t *rssi, void *asyncContext)
{
    auto readContext = static_cast<ClusterReadContext *>(asyncContext);

    scoped_generic char *rssiStr = nullptr;
    if (rssi)
    {
        rssiStr = stringBuilder("%" PRId8, *rssi);
    }

    icDebug("RSSI read request is %s", stringCoalesce(rssiStr));

    if (readContext && readContext->initialResourceValues)
    {
        initialResourceValuesPutDeviceValue(
            readContext->initialResourceValues, COMMON_DEVICE_RESOURCE_FERSSI, rssiStr);
    }
    else if (readContext)
    {
        *readContext->value = rssiStr;
        rssiStr = nullptr;
    }

    deviceDriver->OnDeviceWorkCompleted(readContext->operationComplete, true);

    delete readContext;
}
