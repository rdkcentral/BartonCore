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
 * Created by Thomas Lea on 11/17/21.
 */

#define LOG_TAG     "MatterDetailsStorage"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"
#include <memory>

#include "DiscoveredDeviceDetailsStore.h"

#include <libxml/parser.h>

extern "C" {
#include <deviceHelper.h>
#include <deviceService.h>
#include <icLog/logging.h>
#include <jsonHelper/jsonHelper.h>
}

using namespace zilker;

#define DETAILS_METADATA "matterDetails"

std::shared_ptr<DiscoveredDeviceDetails> DiscoveredDeviceDetailsStore::Get(std::string deviceId)
{
    std::shared_ptr<DiscoveredDeviceDetails> result;

    // first try our common case... from device's metadata
    scoped_icDevice *device = deviceServiceGetDevice(deviceId.c_str());
    if (device != nullptr)
    {
        scoped_generic char *deviceMetadataUri = createDeviceMetadataUri(device->uuid, DETAILS_METADATA);
        scoped_generic char *jsonStr = nullptr;
        if (deviceServiceGetMetadata(deviceMetadataUri, &jsonStr) == false)
        {
            icError("Failed to load details metadata");
        }
        else
        {
            scoped_cJSON *json = cJSON_Parse(jsonStr);
            DiscoveredDeviceDetails *ddd = DiscoveredDeviceDetails::fromJson(json);
            if (json == nullptr || ddd == nullptr)
            {
                icError("Failed to parse details metadata JSON: %s", jsonStr);
            }
            else
            {
                result.reset(ddd);
            }
        }
    }

    if (result == nullptr)
    {
        // try our local temp storage
        std::lock_guard<std::mutex> lock(temporaryDeviceDetailsMtx);
        const auto &it = temporaryDeviceDetails.find(deviceId);
        if (it != temporaryDeviceDetails.end())
        {
            result = it->second;
        }
    }

    return result;
}

void DiscoveredDeviceDetailsStore::Put(const std::string &deviceId, std::shared_ptr<DiscoveredDeviceDetails> details)
{
    scoped_icDevice *device = deviceServiceGetDevice(deviceId.c_str());
    if (device != nullptr)
    {
        scoped_generic char *deviceMetadataUri = createDeviceMetadataUri(device->uuid, DETAILS_METADATA);
        scoped_cJSON *json = details->toJson();
        scoped_generic char *jsonStr = cJSON_PrintUnformatted(json);
        if (deviceServiceSetMetadata(deviceMetadataUri, jsonStr) == false)
        {
            icError("Failed to save details metadata");
        }

        std::lock_guard<std::mutex> lock(temporaryDeviceDetailsMtx);
        temporaryDeviceDetails.erase(device->uuid);
    }
}

void DiscoveredDeviceDetailsStore::PutTemporarily(std::string deviceId,
                                                  std::shared_ptr<DiscoveredDeviceDetails> details)
{
    std::lock_guard<std::mutex> lock(temporaryDeviceDetailsMtx);
    temporaryDeviceDetails[deviceId] = details;
}
