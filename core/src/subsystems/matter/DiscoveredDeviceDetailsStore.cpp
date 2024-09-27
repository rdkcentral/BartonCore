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
 * Created by Thomas Lea on 11/17/21.
 */

#define LOG_TAG     "MatterDetailsStorage"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"
#include <memory>

#include "DiscoveredDeviceDetailsStore.h"

extern "C" {
#include <icLog/logging.h>
#include <deviceService.h>
#include <deviceHelper.h>
#include <jsonHelper/jsonHelper.h>
}

using namespace zilker;

#define DETAILS_METADATA "matterDetails"

std::shared_ptr<DiscoveredDeviceDetails> DiscoveredDeviceDetailsStore::Get(std::string deviceId)
{
    std::shared_ptr<DiscoveredDeviceDetails> result;

    //first try our common case... from device's metadata
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

void DiscoveredDeviceDetailsStore::Put(const std::string& deviceId, std::shared_ptr<DiscoveredDeviceDetails> details)
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

void DiscoveredDeviceDetailsStore::PutTemporarily(std::string deviceId, std::shared_ptr<DiscoveredDeviceDetails> details)
{
    std::lock_guard<std::mutex> lock(temporaryDeviceDetailsMtx);
    temporaryDeviceDetails[deviceId] = details;
}
