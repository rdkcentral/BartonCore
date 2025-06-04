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
 * Created by Raiyan Chowdhury on 3/3/25.
 */

#include "deviceService/resourceModes.h"
#define LOG_TAG     "MatterCameraDD"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterCameraDeviceDriver.h"
#include "MatterDriverFactory.h"

extern "C" {
#include <cjson/cJSON.h>
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <resourceTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
}

#include "app/data-model/Nullable.h"
#include <chrono>
#include <subsystems/matter/DiscoveredDeviceDetailsStore.h>
#include <subsystems/matter/Matter.h>

using namespace barton;
using chip::Callback::Callback;
using namespace std::chrono_literals;

#define MATTER_CAMERA_DEVICE_DRIVER_NAME "matterCamera"

// this is our endpoint, not the device's
#define CAMERA_ENDPOINT                  "1"

struct ClusterReadContext
{
    void *driverContext;                            // the context provided to the driver for the operation
    icInitialResourceValues *initialResourceValues; // non-null if this read is the initial resource fetch
    char **value;                                   // non-null if this read is a regular resource read
};

// auto register with the factory
bool MatterCameraDeviceDriver::registeredWithFactory =
    MatterDriverFactory::Instance().RegisterDriver(new MatterCameraDeviceDriver());

MatterCameraDeviceDriver::MatterCameraDeviceDriver() :
    MatterDeviceDriver(MATTER_CAMERA_DEVICE_DRIVER_NAME, CAMERA_DC, 0)
{
    webRtcTransportProviderClusterEventHandler.deviceDriver = this;
}

bool MatterCameraDeviceDriver::ClaimDevice(DiscoveredDeviceDetails *details)
{
    icDebug();

    // see if any endpoint (not the special 0 entry) has our device id
    for (auto &entry : details->endpointDescriptorData)
    {
        if (entry.first > 0)
        {
            for (auto &deviceTypeEntry : *entry.second->deviceTypes)
            {
                if (deviceTypeEntry == MATTER_CAMERA_DEVICE_ID)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

std::vector<MatterCluster *> MatterCameraDeviceDriver::GetClustersToSubscribeTo(const std::string &deviceId)
{
    icDebug();

    using namespace chip::app;

    auto webRtcTransportServer =
        static_cast<WebRTCTransportProvider *>(GetAnyServerById(deviceId, Clusters::WebRTCTransportProvider::Id));
    if (webRtcTransportServer == nullptr)
    {
        icError("WebRTC Transport Provider cluster not present on device %s!", deviceId.c_str());
        // return {};
    }
    else
    {
        icInfo("WebRTC Transport Provider cluster present on device %s", deviceId.c_str()); // TODO delete
    }

    std::vector<MatterCluster *> clusters;
    clusters.push_back(webRtcTransportServer);
    return clusters;
}

void MatterCameraDeviceDriver::SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                 const std::string &deviceId,
                                                 chip::Messaging::ExchangeManager &exchangeMgr,
                                                 const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // currently we dont do anything during configuration except set up reporting, which also triggers an immediate
    //  report and that takes care of synchronizing state/resources as well.
    ConfigureDevice(promises, deviceId, nullptr, exchangeMgr, sessionHandle);
}

void MatterCameraDeviceDriver::FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                          const std::string &deviceId,
                                                          icInitialResourceValues *initialResourceValues,
                                                          chip::Messaging::ExchangeManager &exchangeMgr,
                                                          const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // TODO
    // no attributes to read for now
}

bool MatterCameraDeviceDriver::RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icDebug();

    icDeviceEndpoint *endpoint = createEndpoint(device, CAMERA_ENDPOINT, CAMERA_PROFILE, true);

    result &= createEndpointResource(endpoint,
                                     COMMON_ENDPOINT_RESOURCE_LABEL,
                                     "Matter Camera",
                                     RESOURCE_TYPE_LABEL,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    result &= createEndpointResource(endpoint,
                                     CAMERA_PROFILE_FUNCTION_PROVIDE_OFFER,
                                     NULL,
                                     RESOURCE_TYPE_SEND_PROVIDE_OFFER_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != nullptr;

    result &= createEndpointResource(endpoint,
                                     CAMERA_PROFILE_RESOURCE_PROVIDE_OFFER_RESPONSE,
                                     NULL,
                                     RESOURCE_TYPE_RECEIVE_OFFER_RESPONSE_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE | RESOURCE_MODE_EMIT_EVENTS,
                                     CACHING_POLICY_NEVER) != nullptr;

    result &= createEndpointResource(endpoint,
                                     CAMERA_PROFILE_RESOURCE_RECEIVE_ANSWER,
                                     NULL,
                                     RESOURCE_TYPE_RECEIVE_ANSWER_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE | RESOURCE_MODE_EMIT_EVENTS,
                                     CACHING_POLICY_NEVER) != nullptr;

    result &= createEndpointResource(endpoint,
                                     CAMERA_PROFILE_FUNCTION_PROVIDE_ICE_CANDIDATES,
                                     NULL,
                                     RESOURCE_TYPE_PROVIDE_ICE_CANDIDATES_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != nullptr;

    result &= createEndpointResource(endpoint,
                                     CAMERA_PROFILE_RESOURCE_RECEIVE_ICE_CANDIDATES,
                                     NULL,
                                     RESOURCE_TYPE_RECEIVE_ICE_CANDIDATES_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE | RESOURCE_MODE_EMIT_EVENTS,
                                     CACHING_POLICY_NEVER) != nullptr;

    result &= createEndpointResource(endpoint,
                                     CAMERA_PROFILE_FUNCTION_SEND_END_SESSION,
                                     NULL,
                                     RESOURCE_TYPE_SEND_END_SESSION_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != nullptr;

    return result;
}

void MatterCameraDeviceDriver::ReadResource(std::forward_list<std::promise<bool>> &promises,
                                            const std::string &deviceId,
                                            icDeviceResource *resource,
                                            char **value,
                                            chip::Messaging::ExchangeManager &exchangeMgr,
                                            const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
    // icDebug("%s", resource->id);
}

bool MatterCameraDeviceDriver::WriteResource(std::forward_list<std::promise<bool>> &promises,
                                             const std::string &deviceId,
                                             icDeviceResource *resource,
                                             const char *previousValue,
                                             const char *newValue,
                                             chip::Messaging::ExchangeManager &exchangeMgr,
                                             const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
    // icDebug("%s = %s", resource->id, newValue);
    return false;
}

void MatterCameraDeviceDriver::ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                                               const std::string &deviceId,
                                               icDeviceResource *resource,
                                               const char *arg,
                                               char **response,
                                               chip::Messaging::ExchangeManager &exchangeMgr,
                                               const chip::SessionHandle &sessionHandle)
{
    icDebug("%s: %s", resource->id, arg);

    // Check whether resource->endpoint is non-null before continuing?
    if (stringCompare(resource->id, CAMERA_PROFILE_FUNCTION_PROVIDE_OFFER, false) == 0)
    {
        ProvideOffer(promises, deviceId, arg, exchangeMgr, sessionHandle);
    }
    else if (stringCompare(resource->id, CAMERA_PROFILE_FUNCTION_PROVIDE_ICE_CANDIDATES, false) == 0)
    {
        ProvideICECandidates(promises, deviceId, arg, exchangeMgr, sessionHandle);
    }
    else if (stringCompare(resource->id, CAMERA_PROFILE_FUNCTION_SEND_END_SESSION, false) == 0)
    {
        SendEndSession(promises, deviceId, arg, exchangeMgr, sessionHandle);
    }
    else
    {
        icError("Unknown resource %s", resource->id);
        FailOperation(promises);
    }
}

void MatterCameraDeviceDriver::ProvideOffer(std::forward_list<std::promise<bool>> &promises,
                                            const std::string &deviceId,
                                            const char *sdp,
                                            chip::Messaging::ExchangeManager &exchangeMgr,
                                            const chip::SessionHandle &sessionHandle)
{
    icDebug();

    using namespace chip::app;
    using namespace chip::app::DataModel;
    using chip::app::Clusters::WebRTCTransportProvider::StreamUsageEnum;

    auto *server =
        static_cast<WebRTCTransportProvider *>(GetAnyServerById(deviceId, Clusters::WebRTCTransportProvider::Id));

    if (server == nullptr)
    {
        icError("WebRTC Transport Provider cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &provideOfferPromise = promises.front();

    // Paired with WebRTCTransportProviderClusterEventHandler::CommandCompleted
    AssociateStoredContext(&provideOfferPromise);

    Nullable<uint16_t> webRTCSessionID(NullNullable);         // null value means we're requesting a new session
    StreamUsageEnum streamUsage = StreamUsageEnum::kLiveView; // only supporting livestreaming for now
    if (!server->SendProvideOffer(&provideOfferPromise, webRTCSessionID, sdp, streamUsage, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(provideOfferPromise);
    }
}

void MatterCameraDeviceDriver::ProvideICECandidates(std::forward_list<std::promise<bool>> &promises,
                                                    const std::string &deviceId,
                                                    const char *arg,
                                                    chip::Messaging::ExchangeManager &exchangeMgr,
                                                    const chip::SessionHandle &sessionHandle)
{
    icDebug();

    using namespace chip::app;

    auto *server =
        static_cast<WebRTCTransportProvider *>(GetAnyServerById(deviceId, Clusters::WebRTCTransportProvider::Id));

    if (server == nullptr)
    {
        icError("WebRTC Transport Provider cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    uint16_t webRTCSessionID;
    std::vector<CharSpan> iceCandidates;
    scoped_cJSON *json = cJSON_Parse(arg);
    cJSON *webRTCSessionIDJson = cJSON_GetObjectItem(json, "webRTCSessionID");
    cJSON *iceCandidatesJson = cJSON_GetObjectItem(json, "iceCandidates");
    if (webRTCSessionIDJson == NULL || iceCandidatesJson == NULL)
    {
        icError("Failed to parse webRTCSessionID and ICE candidate from arg: %s", arg);
        FailOperation(promises);
        return;
    }
    if (!cJSON_IsNumber(webRTCSessionIDJson) || !cJSON_IsArray(iceCandidatesJson))
    {
        icError("Invalid webRTCSessionID or ICE candidates in arg: %s", arg);
        FailOperation(promises);
        return;
    }
    webRTCSessionID = (uint16_t) cJSON_GetNumberValue(webRTCSessionIDJson);
    cJSON *iceCandidateJson = NULL;
    cJSON_ArrayForEach(iceCandidateJson, iceCandidatesJson)
    {
        if (!cJSON_IsString(iceCandidateJson))
        {
            icError("Invalid ICE candidate in arg: %s", arg);
            FailOperation(promises);
            return;
        }

        // Remove newlines and carriage returns from candidate string, else it will have issues with the CommandSender
        scoped_generic char *iceCandidateCharStr = cJSON_GetStringValue(iceCandidateJson);
        std::string iceCandidateStr = iceCandidateCharStr;
        iceCandidateStr.erase(std::remove(iceCandidateStr.begin(), iceCandidateStr.end(), '\n'), iceCandidateStr.end());
        iceCandidateStr.erase(std::remove(iceCandidateStr.begin(), iceCandidateStr.end(), '\r'), iceCandidateStr.end());

        auto iceCandidate = CharSpan::fromCharString(iceCandidateStr.c_str());
        iceCandidates.push_back(iceCandidate);
    }

    promises.emplace_front();
    auto &provideICECandidatesPromise = promises.front();

    // Paired with WebRTCTransportProviderClusterEventHandler::CommandCompleted
    AssociateStoredContext(&provideICECandidatesPromise);

    if (!server->SendProvideICECandidate(
            &provideICECandidatesPromise, webRTCSessionID, iceCandidates, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(provideICECandidatesPromise);
    }
}

void MatterCameraDeviceDriver::SendEndSession(std::forward_list<std::promise<bool>> &promises,
                                              const std::string &deviceId,
                                              const char *arg,
                                              chip::Messaging::ExchangeManager &exchangeMgr,
                                              const chip::SessionHandle &sessionHandle)
{
    icDebug();

    using namespace chip::app;

    auto *server =
        static_cast<WebRTCTransportProvider *>(GetAnyServerById(deviceId, Clusters::WebRTCTransportProvider::Id));

    if (server == nullptr)
    {
        icError("WebRTC Transport Provider cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    uint16_t webRTCSessionID;
    uint8_t reason;
    scoped_cJSON *json = cJSON_Parse(arg);
    cJSON *webRTCSessionIDJson = cJSON_GetObjectItem(json, "webRTCSessionID");
    if (webRTCSessionIDJson == NULL || !cJSON_IsNumber(webRTCSessionIDJson))
    {
        icError("Failed to parse webRTCSessionID from arg: %s", arg);
        FailOperation(promises);
        return;
    }
    webRTCSessionID = (uint16_t) cJSON_GetNumberValue(webRTCSessionIDJson);
    cJSON *reasonJson = cJSON_GetObjectItem(json, "reason");
    if (reasonJson == NULL || !cJSON_IsNumber(reasonJson))
    {
        icError("Failed to parse reason from arg: %s", arg);
        FailOperation(promises);
        return;
    }
    reason = (uint8_t) cJSON_GetNumberValue(reasonJson);

    if (reason >= (uint8_t) Clusters::WebRTCTransportProvider::WebRTCEndReasonEnum::kUnknownEnumValue)
    {
        icError("Invalid reason %d", reason);
        FailOperation(promises);
        return;
    }
    auto endReason = static_cast<Clusters::WebRTCTransportProvider::WebRTCEndReasonEnum>(reason);

    promises.emplace_front();
    auto &sendEndSessionPromise = promises.front();

    // Paired with WebRTCTransportProviderClusterEventHandler::CommandCompleted
    AssociateStoredContext(&sendEndSessionPromise);

    if (!server->SendEndSessionCommand(&sendEndSessionPromise, webRTCSessionID, endReason, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(sendEndSessionPromise);
    }
}

std::unique_ptr<MatterCluster> MatterCameraDeviceDriver::MakeCluster(std::string const &deviceUuid,
                                                                     chip::EndpointId endpointId,
                                                                     chip::ClusterId clusterId)
{
    using namespace chip::app;

    switch (clusterId)
    {
        case Clusters::WebRTCTransportProvider::Id:
            return std::make_unique<WebRTCTransportProvider>(
                (WebRTCTransportProvider::EventHandler *) &webRtcTransportProviderClusterEventHandler,
                deviceUuid,
                endpointId);
            break;

        default:
            return nullptr;
    }
}

void MatterCameraDeviceDriver::WebRTCTransportProviderClusterEventHandler::HandleProvideOfferResponse(
    uint16_t webRTCSessionID,
    std::string &deviceId)
{
    icDebug();

    scoped_generic char *webRTCSessionIDStr = stringBuilder("%d", webRTCSessionID);

    updateResource(
        deviceId.c_str(), CAMERA_ENDPOINT, CAMERA_PROFILE_RESOURCE_PROVIDE_OFFER_RESPONSE, webRTCSessionIDStr, NULL);
}
