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
// Created by Raiyan Chowdhury on 3/5/2025.
//

#include "WebRTCTransportRequestorDelegate.hpp"
#include "lib/core/DataModelTypes.h"
#include "lib/core/NodeId.h"
#include <libxml/parser.h>
#include <subsystems/matter/MatterCommon.h>

#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <cjson/cJSON.h>
#include <commonDeviceDefs.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
}

// TODO: this is defined both here and in MatterCameraDeviceDriver.cpp, not good, fix later
#define CAMERA_ENDPOINT "1"

using namespace barton;

void WebRTCTransportRequestorDelegate::OnAnswer(
    const chip::NodeId nodeId,
    const chip::app::Clusters::WebRTCTransportRequestor::Commands::Answer::DecodableType &commandData)
{
    icInfo("Handling WebRTCTransportRequestor::Answer command");

    uint16_t webRTCSessionID = commandData.webRTCSessionID;
    std::string sdp(commandData.sdp.data(), commandData.sdp.size());

    // Add the data above to a json object that can be parsed as metadata by whoever handles this event
    scoped_cJSON *dataJson = cJSON_CreateObject();
    cJSON_AddNumberToObject(dataJson, "webRTCSessionID", webRTCSessionID);
    cJSON_AddStringToObject(dataJson, "sdp", sdp.c_str());
    scoped_generic char *data = cJSON_PrintUnformatted(dataJson);

    auto deviceUuid = Subsystem::Matter::NodeIdToUuid(nodeId);

    updateResource(deviceUuid.c_str(), CAMERA_ENDPOINT, CAMERA_PROFILE_RESOURCE_RECEIVE_ANSWER, data, NULL);
}

void WebRTCTransportRequestorDelegate::OnICECandidates(
    const chip::NodeId nodeId,
    const chip::app::Clusters::WebRTCTransportRequestor::Commands::ICECandidates::DecodableType &commandData)
{
    icInfo("Handling WebRTCTransportRequestor::ICECandidates command");

    uint16_t webRTCSessionID = commandData.webRTCSessionID;
    chip::app::DataModel::DecodableList<chip::CharSpan> iceCandidates = commandData.ICECandidates;

    scoped_cJSON *dataJson = cJSON_CreateObject();
    cJSON_AddNumberToObject(dataJson, "webRTCSessionID", webRTCSessionID);

    auto iceCandidatesArr = cJSON_AddArrayToObject(dataJson, "iceCandidates");
    if (iceCandidatesArr == nullptr)
    {
        icError("Error handling ICECandidates command; failed to create iceCandidates json array");
        return;
    }

    auto iter = iceCandidates.begin();
    while (iter.Next())
    {
        auto &entry = iter.GetValue();
        std::string iceCandidate(entry.data(), entry.size());
        cJSON_AddItemToArray(iceCandidatesArr, cJSON_CreateString(iceCandidate.c_str()));
    }

    scoped_generic char *data = cJSON_PrintUnformatted(dataJson);

    auto deviceUuid = Subsystem::Matter::NodeIdToUuid(nodeId);

    updateResource(deviceUuid.c_str(), CAMERA_ENDPOINT, CAMERA_PROFILE_RESOURCE_RECEIVE_ICE_CANDIDATES, data, NULL);
}

void WebRTCTransportRequestorDelegate::OnEnd(
    const chip::NodeId nodeId,
    const chip::app::Clusters::WebRTCTransportRequestor::Commands::End::DecodableType &commandData)
{
    icInfo("Unimplemented");
    // TODO
    // For now, we'll just end the session by sending an EndSession command from our side; this will be good to
    // implement later
}
