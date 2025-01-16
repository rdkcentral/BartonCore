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

#ifndef ZILKER_DISCOVEREDDEVICEDETAILS_H
#define ZILKER_DISCOVEREDDEVICEDETAILS_H


#include "lib/core/Optional.h"
#include <cjson/cJSON.h>
#include <glib.h>
#include <iostream>
#include <lib/core/DataModelTypes.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "icUtil/stringUtils.h"
#include "jsonHelper/jsonHelper.h"
}

namespace zilker
{
    class DescriptorClusterData
    {

    public:
        explicit DescriptorClusterData(chip::EndpointId endpointId) : endpointId(endpointId) {};

        ~DescriptorClusterData()
        {
            delete deviceTypes;
            delete clientList;
            delete serverList;
            delete partsList;
        }

        cJSON *toJson() const
        {
            cJSON *json = cJSON_CreateObject();

            cJSON_AddNumberToObject(json, "endpointId", endpointId);

            cJSON *array = cJSON_AddArrayToObject(json, "deviceTypes");
            if (deviceTypes)
            {
                for (const auto &type : *deviceTypes)
                {
                    cJSON *typeObj = cJSON_CreateObject();
                    cJSON_AddNumberToObject(typeObj, "type", type);
                    cJSON_AddItemToArray(array, typeObj);
                }
            }

            array = cJSON_AddArrayToObject(json, "servers");
            if (serverList)
            {
                for (const auto &cluster : *serverList)
                {
                    cJSON_AddItemToArray(array, cJSON_CreateNumber(cluster));
                }
            }

            array = cJSON_AddArrayToObject(json, "clients");
            if (clientList)
            {
                for (const auto &cluster : *clientList)
                {
                    cJSON_AddItemToArray(array, cJSON_CreateNumber(cluster));
                }
            }

            array = cJSON_AddArrayToObject(json, "parts");
            if (partsList)
            {
                for (const auto &cluster : *partsList)
                {
                    cJSON_AddItemToArray(array, cJSON_CreateNumber(cluster));
                }
            }

            return json;
        }

        static std::unique_ptr<DescriptorClusterData> fromJson(const cJSON *json)
        {
            std::unique_ptr<DescriptorClusterData> result;

            cJSON *ep = cJSON_GetObjectItem(json, "endpointId");
            cJSON *types = cJSON_GetObjectItem(json, "deviceTypes");
            cJSON *servers = cJSON_GetObjectItem(json, "servers");
            cJSON *clients = cJSON_GetObjectItem(json, "clients");
            cJSON *parts = cJSON_GetObjectItem(json, "parts");

            if (ep != nullptr && types != nullptr && servers != nullptr && clients != nullptr && parts != nullptr)
            {
                result = std::make_unique<DescriptorClusterData>((chip::EndpointId) cJSON_GetNumberValue(ep));

                cJSON *entry = nullptr;
                cJSON_ArrayForEach(entry, types)
                {
                    if (result->deviceTypes == nullptr)
                        result->deviceTypes = new std::vector<chip::DeviceTypeId>();
                    cJSON *type = cJSON_GetObjectItem(entry, "type");
                    result->deviceTypes->push_back(type->valueint);
                }

                cJSON_ArrayForEach(entry, servers)
                {
                    if (result->serverList == nullptr)
                        result->serverList = new std::vector<chip::ClusterId>();
                    result->serverList->push_back(entry->valueint);
                }

                cJSON_ArrayForEach(entry, clients)
                {
                    if (result->clientList == nullptr)
                        result->clientList = new std::vector<chip::ClusterId>();
                    result->clientList->push_back(entry->valueint);
                }

                cJSON_ArrayForEach(entry, parts)
                {
                    if (result->partsList == nullptr)
                        result->partsList = new std::vector<chip::EndpointId>();
                    result->partsList->push_back(entry->valueint);
                }
            }

            return result;
        }

        // the endpoint that this data belongs to
        chip::EndpointId endpointId;

        std::vector<chip::DeviceTypeId> *deviceTypes {};
        std::vector<chip::ClusterId> *serverList {};
        std::vector<chip::ClusterId> *clientList {};
        std::vector<chip::EndpointId> *partsList {};
    };

    class DiscoveredDeviceDetails
    {
    public:
        DiscoveredDeviceDetails() :
            vendorName(nullptr), productName(nullptr), hardwareVersion(nullptr), softwareVersion(nullptr)
        {
        }
        ~DiscoveredDeviceDetails()
        {
            delete vendorName;
            delete productName;
            delete hardwareVersion;
            delete softwareVersion;
        }

        cJSON *toJson() const
        {
            cJSON *json = cJSON_CreateObject();

            cJSON_AddStringToObject(json, "vendor", vendorName ? vendorName->c_str() : "(null)");
            cJSON_AddStringToObject(json, "product", productName ? productName->c_str() : "(null)");
            cJSON_AddStringToObject(json, "hwVer", hardwareVersion ? hardwareVersion->c_str() : "(null)");
            cJSON_AddStringToObject(json, "swVer", softwareVersion ? softwareVersion->c_str() : "(null)");
            cJSON_AddStringToObject(json, "swStr", softwareVersionString.c_str());

            scoped_cJSON *serialJson = serialNumber.HasValue() && !serialNumber.Value().empty()
                                           ? cJSON_CreateString(serialNumber.Value().c_str())
                                           : cJSON_CreateNull();

            scoped_cJSON *macJson = macAddress.HasValue() && !macAddress.Value().empty()
                                        ? cJSON_CreateString(macAddress.Value().c_str())
                                        : cJSON_CreateNull();

            scoped_cJSON *networkJson = networkType.HasValue() && !networkType.Value().empty()
                                            ? cJSON_CreateString(networkType.Value().c_str())
                                            : cJSON_CreateNull();

            cJSON_AddItemToObjectCS(json, "serialNumber", (cJSON *) g_steal_pointer(&serialJson));
            cJSON_AddItemToObjectCS(json, "macAddress", (cJSON *) g_steal_pointer(&macJson));
            cJSON_AddItemToObjectCS(json, "networkType", (cJSON *) g_steal_pointer(&networkJson));

            cJSON *array = cJSON_AddArrayToObject(json, "endpoints");
            for (const auto &ep : endpointDescriptorData)
            {
                if (ep.second)
                {
                    cJSON_AddItemToArray(array, ep.second->toJson());
                }
            }

            return json;
        }

        static DiscoveredDeviceDetails *fromJson(const cJSON *json)
        {
            DiscoveredDeviceDetails *result = nullptr;

            cJSON *vendor = cJSON_GetObjectItem(json, "vendor");
            cJSON *product = cJSON_GetObjectItem(json, "product");
            cJSON *hw = cJSON_GetObjectItem(json, "hwVer");
            cJSON *sw = cJSON_GetObjectItem(json, "swVer");
            cJSON *swStr = cJSON_GetObjectItem(json, "swStr");
            cJSON *endpoints = cJSON_GetObjectItem(json, "endpoints");
            cJSON *macAddress = cJSON_GetObjectItem(json, "macAddress");
            cJSON *networkType = cJSON_GetObjectItem(json, "networkType");

            if (vendor != nullptr && product != nullptr && hw != nullptr && sw != nullptr)
            {
                result = new DiscoveredDeviceDetails;

                result->vendorName = new std::string(stringCoalesce(cJSON_GetStringValue(vendor)));
                result->productName = new std::string(stringCoalesce(cJSON_GetStringValue(product)));
                result->hardwareVersion = new std::string(stringCoalesce(cJSON_GetStringValue(hw)));
                result->softwareVersion = new std::string(stringCoalesce(cJSON_GetStringValue(sw)));
                result->softwareVersionString = stringCoalesceAlt(cJSON_GetStringValue(swStr), "");
                result->serialNumber = chip::MakeOptional(
                    std::string(stringCoalesce(cJSON_GetStringValue(cJSON_GetObjectItem(json, "serialNumber")))));

                result->macAddress = chip::MakeOptional(std::string(stringCoalesce(cJSON_GetStringValue(macAddress))));
                result->networkType =
                    chip::MakeOptional(std::string(stringCoalesce(cJSON_GetStringValue(networkType))));

                cJSON *entry = nullptr;
                cJSON_ArrayForEach(entry, endpoints)
                {
                    auto data = DescriptorClusterData::fromJson(entry);
                    if (data != nullptr)
                    {
                        result->endpointDescriptorData[data->endpointId] = std::move(data);
                    }
                }
            }

            return result;
        }

        std::string *vendorName;
        std::string *productName;
        std::string *hardwareVersion;
        std::string *softwareVersion;
        std::string softwareVersionString;

        // The presence of a value for Optionals indicates whether or not that stage of
        // disovery is complete (i.e., the read interaction has completed).
        // It shall contain the empty string ("") when the attribute is not supported.
        chip::Optional<std::string> serialNumber;
        chip::Optional<std::string> macAddress;
        chip::Optional<std::string> networkType;
        std::map<chip::EndpointId, std::shared_ptr<DescriptorClusterData>> endpointDescriptorData;
    };
} // namespace zilker

#endif // ZILKER_DISCOVEREDDEVICEDETAILS_H
