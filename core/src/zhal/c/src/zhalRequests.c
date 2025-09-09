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

#include "zhalPrivate.h"
#include <cjson/cJSON.h>
#include <icLog/logging.h>
#include <icUtil/base64.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zhal/zhal.h>

/*
 * This is where the requests that can be made to ZigbeeCore reside.
 *
 * Created by Thomas Lea on 3/14/16.
 */

#define DEFAULT_REQUEST_TIMEOUT_SECONDS 30

// On EM357 with maximum of 70 devices this could take about a minute.  Extrapolating to a max 128 is about 150 secs.
#define SET_DEVICES_TIMEOUT_SECONDS     150

// If we get a 'network busy' response from ZigbeeCore, we will wait a little bit and try again
#define MAX_NETWORK_BUSY_RETRIES        5
#define NETWORK_BUSY_RETRY_DELAY_MILLIS 250

static int sendRequest(uint64_t eui64, cJSON *request, cJSON **response);

static int sendRequestWithTimeout(uint64_t eui64, cJSON *request, int timeoutSecs, cJSON **response);

static int sendRequestNoResponse(uint64_t eui64, cJSON *request);

static void setAddress(uint64_t eui64, cJSON *request);

int zhalNetworkInit(uint64_t eui64, const char *region, const char *networkConfigData, icStringHashMap *properties)
{
    icLogDebug(LOG_TAG, "zhalNetworkInit: eui64=%016" PRIx64 ", region = %s", eui64, region);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "networkInit");

    if (eui64 != 0)
    {
        setAddress(eui64, request);
    }

    if (region != NULL)
    {
        cJSON_AddStringToObject(request, "region", region);
    }

    if (networkConfigData != NULL)
    {
        cJSON_AddStringToObject(request, "networkConfigData", networkConfigData);
    }

    if (properties != NULL)
    {
        cJSON_AddItemToObject(request, "properties", stringHashMapToJSON(properties));
    }

    return sendRequestNoResponse(0, request);
}

int zhalNetworkTerm(void)
{
    icLogDebug(LOG_TAG, "%s", __func__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "networkTerm");

    return sendRequestNoResponse(0, request);
}

int zhalHeartbeat(pid_t *pid, bool *initialized)
{
    icLogDebug(LOG_TAG, "zhalHeartbeat");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "heartbeat");

    cJSON *response = NULL;
    int rc = sendRequest(0, request, &response);
    if (rc == 0 && response != NULL)
    {
        if (pid != NULL)
        {
            cJSON *tmp = cJSON_GetObjectItem(response, "pid");
            if (cJSON_IsNumber(tmp) == true)
            {
                *pid = tmp->valueint;
            }
            else
            {
                *pid = -1;
            }
        }

        if (initialized != NULL)
        {
            cJSON *tmp = cJSON_GetObjectItem(response, "initialized");
            *initialized = cJSON_IsTrue(tmp);
        }
    }

    cJSON_Delete(response);

    return rc;
}

int zhalRefreshOtaFiles(void)
{
    icLogDebug(LOG_TAG, "zhalRefreshOtaFiles");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "refreshOtaFiles");

    return sendRequestNoResponse(0, request);
}

cJSON *zhalGetAndClearCounters(void)
{
    cJSON *result = NULL;

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "getCounters");

    int rc = sendRequest(0, request, &result);
    if (rc != 0 || result == NULL)
    {
        icLogDebug(LOG_TAG, "zhalGetAndClearCounters failed.");
    }
    else
    {
        // it worked, just remove the 4 JSON IPC related items and we will be left with only counters.
        cJSON_DeleteItemFromObject(result, "eventType");
        cJSON_DeleteItemFromObject(result, "ipcResponseType");
        cJSON_DeleteItemFromObject(result, "resultCode");
        cJSON_DeleteItemFromObject(result, "requestId");
    }

    return result;
}

int zhalSetPiezoTone(zhalPiezoTone tone)
{
    icLogDebug(LOG_TAG, "%s: tone=%d", __FUNCTION__, tone);

    cJSON *request = cJSON_CreateObject();

    char *toneStr = NULL;
    switch (tone)
    {
        case piezoToneNone:
            // leave toneStr NULL to trigger deactivation of piezo
            break;
        case piezoToneWarble:
            toneStr = "warble";
            break;
        case piezoToneFire:
            toneStr = "fire";
            break;
        case piezoToneCo:
            toneStr = "t4_co";
            break;
        case piezoToneHighFreq:
            toneStr = "highFrequency";
            break;
        case piezoToneLowFreq:
            toneStr = "lowFrequency";
            break;

        default:
            icLogError(LOG_TAG, "%s: unsupported tone", __FUNCTION__);
            return -1;
    }

    if (toneStr != NULL)
    {
        cJSON_AddStringToObject(request, "request", "activatePiezo");
        cJSON_AddStringToObject(request, "tone", toneStr);
    }
    else
    {
        cJSON_AddStringToObject(request, "request", "deactivatePiezo");
    }

    return sendRequestNoResponse(0, request);
}

int zhalGetSystemStatus(zhalSystemStatus *status)
{
    int result = 0;

    icLogDebug(LOG_TAG, "zhalGetSystemStatus");

    if (status == NULL)
    {
        icLogError(LOG_TAG, "null zhalSystemStatus argument provided.");
        return -1;
    }

    memset(status, 0, sizeof(zhalSystemStatus));

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "getSystemStatus");

    cJSON *response = NULL;
    result = sendRequest(0, request, &response);
    if (result == 0 && response != NULL)
    {
        cJSON *item = cJSON_GetObjectItem(response, "networkIsUp");
        if (item != NULL)
        {
            status->networkIsUp = cJSON_IsTrue(item);
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'networkIsUp'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "networkIsOpenForJoin");
        if (item != NULL)
        {
            status->networkIsOpenForJoin = cJSON_IsTrue(item);
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'networkIsOpenForJoin'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "eui64");
        if (item == NULL ||
            stringToUnsignedNumberWithinRange(item->valuestring, &status->eui64, 16, 0, UINT64_MAX) == false)
        {
            icLogError(LOG_TAG, "system status response missing/invalid 'eui64'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "originalEui64");
        if (item == NULL ||
            stringToUnsignedNumberWithinRange(item->valuestring, &status->originalEui64, 16, 0, UINT64_MAX) == false)
        {
            icLogError(LOG_TAG, "system status response missing/invalid 'originalEui64'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "channel");
        if (item != NULL)
        {
            status->channel = (uint8_t) item->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'channel'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "panId");
        if (item != NULL)
        {
            status->panId = (uint16_t) item->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'panId'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "networkKey");
        if (item != NULL && item->valuestring != NULL && strlen(item->valuestring) == 32)
        {
            for (int i = 0; i < 16; i++)
            {
                uint64_t val;
                char str[3] = {0, 0, 0};
                strncpy(str, item->valuestring + 2 * i, 2);
                if (stringToUnsignedNumberWithinRange(str, &val, 16, 0, UINT8_MAX) == true)
                {
                    status->networkKey[i] = val;
                }
                else
                {
                    icLogError(LOG_TAG, "%s: invalid data in networkKey (%s)", __FUNCTION__, item->valuestring);
                    result = -1;
                    break;
                }
            }
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing/invalid 'networkKey'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "version");
        if (item != NULL)
        {
            strncpy(status->version, item->valuestring, sizeof(status->version) - 1);
        }

        cJSON_Delete(response);
    }
    else
    {
        icLogWarn(LOG_TAG, "zhalGetSystemStatus failed.");
        result = -1;
    }

    return result;
}

int zhalNetworkEnableJoin(void)
{
    icLogDebug(LOG_TAG, "zhalNetworkEnableJoin");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "networkEnableJoin");
    cJSON_AddNumberToObject(request, "durationSeconds", 255);

    return sendRequestNoResponse(0, request);
}

int zhalNetworkDisableJoin(void)
{
    icLogDebug(LOG_TAG, "zhalNetworkDisableJoin");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "networkDisableJoin");

    return sendRequestNoResponse(0, request);
}

int zhalGetEndpointIds(uint64_t eui64, uint8_t **endpointIds, uint8_t *numEndpointIds)
{
    int result = 0;

    icLogDebug(LOG_TAG, "zhalGetEndpointIds");

    if (endpointIds == NULL || numEndpointIds == NULL)
    {
        icLogError(LOG_TAG, "zhalGetEndpointIds: invalid arguments");
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "getEndpointIds");

    setAddress(eui64, request);

    cJSON *response;
    result = sendRequest(eui64, request, &response);
    if (result == 0 && response != NULL)
    {
        cJSON *endpointIdsJson = cJSON_GetObjectItem(response, "endpointIds");
        if (endpointIdsJson != NULL)
        {
            *numEndpointIds = (uint8_t) cJSON_GetArraySize(endpointIdsJson);
            *endpointIds = (uint8_t *) calloc(*numEndpointIds, 1);
            for (uint8_t i = 0; i < *numEndpointIds; i++)
            {
                cJSON *item = cJSON_GetArrayItem(endpointIdsJson, i);
                (*endpointIds)[i] = (uint8_t) item->valueint;
            }
        }
        else
        {
            icLogError(LOG_TAG, "get endpoint ids response missing 'endpointIds'");
            result = -1;
        }

        cJSON_Delete(response);
    }

    return result;
}

int zhalGetEndpointInfo(uint64_t eui64, uint8_t endpointId, zhalEndpointInfo *info)
{
    int result = 0;

    icLogDebug(LOG_TAG, "zhalGetEndpointInfo");

    if (info == NULL)
    {
        icLogError(LOG_TAG, "zhalGetEndpointInfo: invalid arguments");
        return -1;
    }

    memset(info, 0, sizeof(zhalEndpointInfo));

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "getClustersInfo");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);

    cJSON *response;
    result = sendRequest(eui64, request, &response);
    if (result == 0 && response != NULL)
    {
        cJSON *item = cJSON_GetObjectItem(response, "endpointId");
        if (item != NULL)
        {
            info->endpointId = (uint8_t) item->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'endpointId'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "appProfileId");
        if (item != NULL)
        {
            info->appProfileId = (uint16_t) item->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'appProfileId'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "appDeviceId");
        if (item != NULL)
        {
            info->appDeviceId = (uint16_t) item->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'appDeviceId'");
            result = -1;
        }

        item = cJSON_GetObjectItem(response, "appDeviceVersion");
        if (item != NULL)
        {
            info->appDeviceVersion = (uint8_t) item->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "system status response missing 'appDeviceVersion'");
            result = -1;
        }

        for (int i = 0; i < 2; i++) // loop first gets input/server clusters, then output/client
        {
            char *itemName = (i == 0 ? "appInputClusterIds" : "appOutputClusterIds");

            uint16_t *clusters = (i == 0 ? info->serverClusterIds : info->clientClusterIds);
            uint8_t *numClusters = (i == 0 ? &info->numServerClusterIds : &info->numClientClusterIds);

            item = cJSON_GetObjectItem(response, itemName);
            if (item != NULL)
            {
                *numClusters = (uint8_t) cJSON_GetArraySize(item);
                for (int j = 0; j < *numClusters; j++)
                {
                    cJSON *itemEntry = cJSON_GetArrayItem(item, j);
                    clusters[j] = (uint16_t) itemEntry->valueint;
                }
            }
            else
            {
                icLogError(LOG_TAG, "zhalGetEndpointInfo: getClustersInfo response missing '%s'", itemName);
                result = -1;
            }
        }

        cJSON_Delete(response);
    }

    return result;
}

int zhalGetAttributeInfos(uint64_t eui64,
                          uint8_t endpointId,
                          uint16_t clusterId,
                          bool toServer,
                          zhalAttributeInfo **infos,
                          uint16_t *numInfos)
{
    int result = 0;

    icLogDebug(LOG_TAG, "zhalGetAttributeInfos");

    if (infos == NULL || numInfos == NULL)
    {
        icLogError(LOG_TAG, "zhalGetAttributeInfos: invalid arguments");
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "getAttributeInfos");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);
    cJSON_AddNumberToObject(request, "clientToServer", toServer ? 1 : 0);

    cJSON *response;
    result = sendRequest(eui64, request, &response);
    if (result == 0 && response != NULL)
    {
        cJSON *attributeInfosJson = cJSON_GetObjectItem(response, "attributeInfos");
        if (attributeInfosJson != NULL)
        {
            *numInfos = (uint16_t) cJSON_GetArraySize(attributeInfosJson);
            if (*numInfos > 0)
            {
                *infos = (zhalAttributeInfo *) calloc(*numInfos, sizeof(zhalAttributeInfo));
                for (uint16_t i = 0; i < *numInfos; i++)
                {
                    cJSON *arrayItem = cJSON_GetArrayItem(attributeInfosJson, i);

                    cJSON *item = cJSON_GetObjectItem(arrayItem, "id");
                    (*infos)[i].id = (uint16_t) item->valueint;

                    item = cJSON_GetObjectItem(arrayItem, "type");
                    (*infos)[i].type = (uint8_t) item->valueint;
                }
            }
            else
            {
                *infos = NULL;
            }
        }
        else
        {
            icLogError(LOG_TAG, "zhalGetAttributeInfos: getAttributeInfos response missing 'attributeInfos'");
            result = -1;
        }

        cJSON_Delete(response);
    }

    return result;
}

static int attributesRead(uint64_t eui64,
                          uint8_t endpointId,
                          uint16_t clusterId,
                          bool toServer,
                          bool isMfgSpecific,
                          uint16_t mfgId,
                          const uint16_t *attributeIds,
                          uint8_t numAttributeIds,
                          zhalAttributeData *attributeData)
{
    int result = 0;

    icLogDebug(LOG_TAG, "zhalAttributesRead");

    if (attributeIds == NULL || numAttributeIds == 0 || attributeData == NULL)
    {
        icLogError(LOG_TAG, "zhalAttributesRead: invalid arguments");
        return -1;
    }

    memset(attributeData, 0, sizeof(zhalAttributeData) * numAttributeIds);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "attributesRead");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);
    cJSON_AddNumberToObject(request, "clientToServer", toServer ? 1 : 0);
    cJSON_AddNumberToObject(request, "isMfgSpecific", isMfgSpecific ? 1 : 0);
    if (isMfgSpecific)
    {
        cJSON_AddNumberToObject(request, "mfgId", mfgId);
    }

    cJSON *infosJson = cJSON_CreateArray();
    for (uint8_t i = 0; i < numAttributeIds; i++)
    {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "id", attributeIds[i]);
        cJSON_AddItemToArray(infosJson, entry);
    }
    cJSON_AddItemToObject(request, "infos", infosJson);

    cJSON *response;
    result = sendRequest(eui64, request, &response);
    if (result == 0 && response != NULL)
    {
        cJSON *attributeDataJson = cJSON_GetObjectItem(response, "attributeData");
        if (attributeDataJson != NULL)
        {
            uint8_t numAttributeData = (uint8_t) cJSON_GetArraySize(attributeDataJson);
            if (numAttributeData != numAttributeIds)
            {
                icLogError(LOG_TAG,
                           "zhalAttributesRead: received %d attribute datas but was expecting %d",
                           numAttributeData,
                           numAttributeIds);
                result = -1;
            }
            else
            {
                for (uint8_t i = 0; i < numAttributeData; i++)
                {
                    cJSON *arrayItem = cJSON_GetArrayItem(attributeDataJson, i);

                    cJSON *item = cJSON_GetObjectItem(arrayItem, "id");
                    if (item != NULL)
                    {
                        attributeData[i].attributeInfo.id = (uint16_t) item->valueint;

                        item = cJSON_GetObjectItem(arrayItem, "type");
                        if (item != NULL)
                        {
                            attributeData[i].attributeInfo.type = (uint8_t) item->valueint;
                        }

                        item = cJSON_GetObjectItem(arrayItem, "success");
                        if (item != NULL && item->valueint)
                        {
                            item = cJSON_GetObjectItem(arrayItem, "data"); // base64 encoded string

                            uint8_t *bytes = NULL;
                            uint16_t byteLen = 0;

                            if (icDecodeBase64(item->valuestring, &bytes, &byteLen))
                            {
                                // caller will free bytes
                                attributeData[i].data = bytes;
                                attributeData[i].dataLen = byteLen;
                            }
                            else
                            {
                                icLogError(LOG_TAG, "unable to decode data!");
                            }
                        }
                        else
                        {
                            icLogError(LOG_TAG, "an attribute failed to read");
                        }
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Got bad data in attribute read response");
                        result = -1;
                    }
                }
            }
        }
        else
        {
            icLogError(LOG_TAG, "attributesRead: response missing 'attributeData'");
            result = -1;
        }

        cJSON_Delete(response);
    }

    return result;
}

int zhalAttributesRead(uint64_t eui64,
                       uint8_t endpointId,
                       uint16_t clusterId,
                       bool toServer,
                       const uint16_t *attributeIds,
                       uint8_t numAttributeIds,
                       zhalAttributeData *attributeData)
{
    return attributesRead(eui64,
                          endpointId,
                          clusterId,
                          toServer,
                          false,
                          0xFFFF /*some invalid id since isMfgSpecific is false */,
                          attributeIds,
                          numAttributeIds,
                          attributeData);
}

int zhalAttributesReadMfgSpecific(uint64_t eui64,
                                  uint8_t endpointId,
                                  uint16_t clusterId,
                                  uint16_t mfgId,
                                  bool toServer,
                                  const uint16_t *attributeIds,
                                  uint8_t numAttributeIds,
                                  zhalAttributeData *attributeData)
{
    return attributesRead(
        eui64, endpointId, clusterId, toServer, true, mfgId, attributeIds, numAttributeIds, attributeData);
}

static int attributesWrite(uint64_t eui64,
                           uint8_t endpointId,
                           uint16_t clusterId,
                           bool isMfgSpecific,
                           uint16_t mfgId,
                           bool toServer,
                           zhalAttributeData *attributeData,
                           uint8_t numAttributes)
{
    icLogDebug(LOG_TAG, "zhalAttributesWrite");

    if (attributeData == NULL || numAttributes == 0)
    {
        icLogError(LOG_TAG, "zhalAttributesWrite: invalid arguments");
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "attributesWrite");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);
    cJSON_AddNumberToObject(request, "clientToServer", toServer ? 1 : 0);
    cJSON_AddNumberToObject(request, "isMfgSpecific", isMfgSpecific ? 1 : 0);
    if (isMfgSpecific)
    {
        cJSON_AddNumberToObject(request, "mfgId", mfgId);
    }

    cJSON *datasJson = cJSON_CreateArray();
    for (uint8_t i = 0; i < numAttributes; i++)
    {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "id", attributeData[i].attributeInfo.id);
        cJSON_AddNumberToObject(entry, "type", attributeData[i].attributeInfo.type);

        char *encodedData = icEncodeBase64(attributeData[i].data, attributeData[i].dataLen);
        cJSON_AddStringToObject(entry, "data", encodedData);
        free(encodedData);

        cJSON_AddItemToArray(datasJson, entry);
    }
    cJSON_AddItemToObject(request, "attributes", datasJson);

    return sendRequestNoResponse(eui64, request);
}

int zhalAttributesWrite(uint64_t eui64,
                        uint8_t endpointId,
                        uint16_t clusterId,
                        bool toServer,
                        zhalAttributeData *attributeData,
                        uint8_t numAttributes)
{
    return attributesWrite(eui64, endpointId, clusterId, false, 0xFFFF, toServer, attributeData, numAttributes);
}

int zhalAttributesWriteMfgSpecific(uint64_t eui64,
                                   uint8_t endpointId,
                                   uint16_t clusterId,
                                   uint16_t mfgId,
                                   bool toServer,
                                   zhalAttributeData *attributeData,
                                   uint8_t numAttributes)
{
    return attributesWrite(eui64, endpointId, clusterId, true, mfgId, toServer, attributeData, numAttributes);
}

int zhalBindingSet(uint64_t eui64, uint8_t endpointId, uint16_t clusterId)
{
    icLogDebug(LOG_TAG, "zhalBindingSet: %" PRIx64 " endpoint %d cluster %d", eui64, endpointId, clusterId);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "bindingSet");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);

    return sendRequestNoResponse(eui64, request);
}

int zhalBindingSetTarget(uint64_t eui64,
                         uint8_t endpointId,
                         uint64_t targetEui64,
                         uint8_t targetEndpointId,
                         uint16_t clusterId)
{
    icLogDebug(LOG_TAG, "zhalBindingSetTarget");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "bindingSet");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);

    char buf[21]; // max uint64_t is 18446744073709551615
    sprintf(buf, "%016" PRIx64, targetEui64);
    cJSON_AddStringToObject(request, "targetAddress", buf);
    cJSON_AddNumberToObject(request, "targetEndpointId", targetEndpointId);

    return sendRequestNoResponse(eui64, request);
}

icLinkedList *zhalBindingGet(uint64_t eui64)
{
    icLinkedList *result = NULL;

    icLogDebug(LOG_TAG, "zhalBindingGet");

    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "request", "bindingGet");
    setAddress(eui64, request);

    cJSON *response;
    if (sendRequest(0, request, &response) == 0)
    {
        if (response != NULL)
        {
            result = linkedListCreate();

            cJSON *entries = cJSON_GetObjectItem(response, "entries");
            if (entries != NULL)
            {
                for (int i = 0; i < cJSON_GetArraySize(entries); ++i)
                {
                    cJSON *entry = cJSON_GetArrayItem(entries, i);

                    zhalBindingTableEntry *bte = calloc(1, sizeof(*bte));

                    cJSON *tmp = cJSON_GetObjectItem(entry, "sourceAddress");
                    if (tmp != NULL)
                    {
                        sscanf(tmp->valuestring, "%" PRIx64, &bte->sourceAddress);
                    }

                    tmp = cJSON_GetObjectItem(entry, "sourceEndpoint");
                    if (tmp != NULL)
                    {
                        bte->sourceEndpoint = tmp->valueint;
                    }

                    tmp = cJSON_GetObjectItem(entry, "clusterId");
                    if (tmp != NULL)
                    {
                        bte->clusterId = tmp->valueint;
                    }

                    // can either have destinationAddress + destinationEndpoint OR just destinationGroup
                    tmp = cJSON_GetObjectItem(entry, "destinationGroup");
                    if (tmp != NULL)
                    {
                        bte->destinationAddressMode = ZHAL_DESTINATION_ADDRESS_MODE_GROUP;
                        bte->destination.group = tmp->valueint;
                    }
                    else
                    {
                        bte->destinationAddressMode = ZHAL_DESTINATION_ADDRESS_MODE_DEVICE;
                        tmp = cJSON_GetObjectItem(entry, "destinationAddress");
                        if (tmp != NULL)
                        {
                            sscanf(tmp->valuestring, "%" PRIx64, &bte->destination.extendedAddress.eui64);
                        }

                        tmp = cJSON_GetObjectItem(entry, "destinationEndpoint");
                        if (tmp != NULL)
                        {
                            bte->destination.extendedAddress.endpoint = tmp->valueint;
                        }
                    }

                    linkedListAppend(result, bte);
                }
            }

            cJSON_Delete(response);
        }
    }

    return result;
}

int zhalBindingClear(uint64_t eui64, uint8_t endpointId, uint16_t clusterId)
{
    icLogDebug(LOG_TAG, "zhalBindingClear");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "bindingClear");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);

    return sendRequestNoResponse(eui64, request);
}

int zhalBindingClearTarget(uint64_t eui64,
                           uint8_t endpointId,
                           uint16_t clusterId,
                           uint64_t targetEui64,
                           uint8_t targetEndpoint)
{
    icLogDebug(LOG_TAG, "zhalBindingClearTarget");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "bindingClear");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);

    char buf[21]; // max uint64_t is 18446744073709551615
    sprintf(buf, "%016" PRIx64, targetEui64);
    cJSON_AddStringToObject(request, "targetAddress", buf);
    cJSON_AddNumberToObject(request, "targetEndpointId", targetEndpoint);

    return sendRequestNoResponse(eui64, request);
}

static int zhalAttributesSetReportingInternal(uint64_t eui64,
                                              uint8_t endpointId,
                                              uint16_t clusterId,
                                              zhalAttributeReportingConfig *configs,
                                              uint8_t numConfigs,
                                              bool mfgSpecific,
                                              uint16_t mfgId)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    if (configs == NULL || numConfigs == 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "attributesSetReporting");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);

    cJSON *configsJson = cJSON_CreateArray();
    for (uint8_t i = 0; i < numConfigs; i++)
    {
        cJSON *configJson = cJSON_CreateObject();

        cJSON *infoJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(infoJson, "id", configs[i].attributeInfo.id);
        cJSON_AddNumberToObject(infoJson, "type", configs[i].attributeInfo.type);
        cJSON_AddItemToObject(configJson, "info", infoJson);

        cJSON_AddNumberToObject(configJson, "minInterval", configs[i].minInterval);
        cJSON_AddNumberToObject(configJson, "maxInterval", configs[i].maxInterval);
        cJSON_AddNumberToObject(configJson, "reportableChange", configs[i].reportableChange);

        cJSON_AddItemToArray(configsJson, configJson);
    }
    cJSON_AddItemToObject(request, "configs", configsJson);

    cJSON_AddBoolToObject(request, "isMfgSpecific", mfgSpecific);
    if (mfgSpecific == true)
    {
        cJSON_AddNumberToObject(request, "mfgId", mfgId);
    }
    return sendRequestNoResponse(eui64, request);
}

int zhalAttributesSetReporting(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               zhalAttributeReportingConfig *configs,
                               uint8_t numConfigs)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    return zhalAttributesSetReportingInternal(eui64, endpointId, clusterId, configs, numConfigs, false, 0);
}

int zhalAttributesSetReportingMfgSpecific(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          uint16_t mfgId,
                                          zhalAttributeReportingConfig *configs,
                                          uint8_t numConfigs)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    return zhalAttributesSetReportingInternal(eui64, endpointId, clusterId, configs, numConfigs, true, mfgId);
}

int zhalSetDevices(zhalDeviceEntry *devices, uint16_t numDevices)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (devices == NULL && numDevices != 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "setDevices");

    cJSON *deviceEntries = cJSON_CreateArray();
    for (uint16_t i = 0; i < numDevices; i++)
    {
        cJSON *deviceEntry = cJSON_CreateObject();

        char buf[21]; // max uint64_t is 18446744073709551615
        sprintf(buf, "%016" PRIx64, devices[i].eui64);
        cJSON_AddStringToObject(deviceEntry, "eui64", buf);
        cJSON_AddNumberToObject(deviceEntry, "flags", devices[i].flags.byte);

        cJSON_AddItemToArray(deviceEntries, deviceEntry);
    }

    cJSON_AddItemToObject(request, "devices", deviceEntries);

    return sendRequestWithTimeout(0, request, SET_DEVICES_TIMEOUT_SECONDS, NULL);
}

int zhalRemoveDeviceAddress(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "zhalRemoveDeviceAddress");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "removeDeviceAddress");

    setAddress(eui64, request);

    return sendRequestNoResponse(0, request);
}

int zhalSendCommand(uint64_t eui64,
                    uint8_t endpointId,
                    uint16_t clusterId,
                    bool toServer,
                    bool useApsEncryption,
                    bool useDefaultResponse,
                    uint8_t commandId,
                    uint8_t *message,
                    uint16_t messageLen)
{
    icLogDebug(LOG_TAG, "zhalSendCommand");

    if (message != NULL && messageLen == 0)
    {
        icLogError(LOG_TAG, "zhalSendCommand: invalid arguments");
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "sendCommand");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);
    cJSON_AddStringToObject(request, "direction", toServer ? "clientToServer" : "serverToClient");
    cJSON_AddNumberToObject(request, "isMfgSpecific", 0);
    cJSON_AddNumberToObject(request, "mfgId", 0);
    cJSON_AddNumberToObject(request, "commandId", commandId);

    if (message != NULL)
    {
        char *encoded = icEncodeBase64(message, messageLen);
        cJSON_AddStringToObject(request, "encodedMessage", encoded);
        free(encoded);
    }
    else
    {
        cJSON_AddStringToObject(request, "encodedMessage", "");
    }

    cJSON_AddBoolToObject(request, "requestDefaultResponse", useDefaultResponse);
    cJSON_AddBoolToObject(request, "useApsEncryption", useApsEncryption);

    return sendRequestNoResponse(eui64, request);
}

int zhalSendMfgCommand(uint64_t eui64,
                       uint8_t endpointId,
                       uint16_t clusterId,
                       bool toServer,
                       bool useApsEncryption,
                       bool useDefaultResponse,
                       uint8_t commandId,
                       uint16_t mfgId,
                       uint8_t *message,
                       uint16_t messageLen)
{
    icLogDebug(LOG_TAG, "zhalSendCommand");

    if (message != NULL && messageLen == 0)
    {
        icLogError(LOG_TAG, "zhalSendCommand: invalid arguments");
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "sendCommand");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);
    cJSON_AddStringToObject(request, "direction", toServer ? "clientToServer" : "serverToClient");
    cJSON_AddNumberToObject(request, "isMfgSpecific", 1);
    cJSON_AddNumberToObject(request, "mfgId", mfgId);
    cJSON_AddNumberToObject(request, "commandId", commandId);

    if (message != NULL)
    {
        char *encoded = icEncodeBase64(message, messageLen);
        cJSON_AddStringToObject(request, "encodedMessage", encoded);
        free(encoded);
    }
    else
    {
        cJSON_AddStringToObject(request, "encodedMessage", "");
    }

    cJSON_AddBoolToObject(request, "requestDefaultResponse", useDefaultResponse);
    cJSON_AddBoolToObject(request, "useApsEncryption", useApsEncryption);

    return sendRequestNoResponse(eui64, request);
}

int zhalSendViaApsAck(uint64_t eui64,
                      uint8_t endpointId,
                      uint16_t clusterId,
                      uint8_t sequenceNum,
                      uint8_t *message,
                      uint16_t messageLen)
{
    icLogDebug(LOG_TAG, "zhalSendViaApsAck");

    if (message != NULL && messageLen == 0)
    {
        icLogError(LOG_TAG, "zhalSendViaApsAck: invalid arguments");
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "sendViaApsAck");

    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpointId", endpointId);
    cJSON_AddNumberToObject(request, "clusterId", clusterId);
    cJSON_AddNumberToObject(request, "sequenceNum", sequenceNum);

    if (message != NULL)
    {
        char *encoded = icEncodeBase64(message, messageLen);
        cJSON_AddStringToObject(request, "encodedMessage", encoded);
        free(encoded);
    }
    else
    {
        cJSON_AddStringToObject(request, "encodedMessage", "");
    }

    return sendRequestNoResponse(eui64, request);
}

int zhalRequestLeave(uint64_t eui64, bool withRejoin, bool isEndDevice)
{
    icLogDebug(LOG_TAG, "zhalRequestLeave");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "requestLeave");
    setAddress(eui64, request);

    if (withRejoin)
    {
        cJSON_AddTrueToObject(request, "withRejoin");
    }

    if (isEndDevice)
    {
        cJSON_AddTrueToObject(request, "isEndDevice");
    }

    return sendRequestNoResponse(eui64, request);
}

/**
 * Change the configuration of the zigbee network.  Pass 0 for channel, panId, or networkKey to indicate that you
 * don't want that value to change.
 *
 * @return 0 on success
 */
int zhalNetworkChange(zhalNetworkChangeRequest *networkChangeRequest)
{
    icLogDebug(LOG_TAG, "zhalNetworkChange");

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "networkChange");
    cJSON_AddNumberToObject(request, "channel", networkChangeRequest->channel);
    cJSON_AddNumberToObject(request, "panId", networkChangeRequest->panId);
    // Format the network key as a hex string, since the network key is 16 bytes long, and we need 2 chars for each
    // byte, we need 32 characters plus space for NULL terminator
    char netKey[33];
    for (int i = 0; i < 16; ++i)
    {
        sprintf(netKey + (i * 2), "%02x", networkChangeRequest->networkKey[i]);
    }
    cJSON_AddStringToObject(request, "netKey", netKey);

    return sendRequestNoResponse(0, request);
}

/**
 * Check if the provided eui64 is a child of ours
 *
 * @return true if its a child, false otherwise
 */
bool zhalDeviceIsChild(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "zhalDeviceIsChild");

    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "request", "deviceIsChild");
    setAddress(eui64, request);

    return sendRequestNoResponse(0, request) == 0;
}

/**
 * Get the source route for the given eui64
 *
 * @return linked list of eui64s as uint64_t, or NULL if fails
 */
icLinkedList *zhalGetSourceRoute(uint64_t eui64)
{
    icLinkedList *sourceRouteList = NULL;

    icLogDebug(LOG_TAG, "zhalGetSourceRoute");

    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "request", "getSourceRoute");
    setAddress(eui64, request);

    cJSON *response;
    if (sendRequest(0, request, &response) == 0)
    {
        if (response != NULL)
        {
            sourceRouteList = linkedListCreate();

            cJSON *hops = cJSON_GetObjectItem(response, "hops");
            if (hops != NULL)
            {
                for (int i = 0; i < cJSON_GetArraySize(hops); ++i)
                {
                    cJSON *hopEui64Str = cJSON_GetArrayItem(hops, i);
                    if (cJSON_IsString(hopEui64Str))
                    {
                        // Extract the eui64 and convert to a uin64_t
                        uint64_t *hopEui64 = (uint64_t *) malloc(sizeof(uint64_t));
                        sscanf(hopEui64Str->valuestring, "%" PRIx64, hopEui64);
                        linkedListAppend(sourceRouteList, hopEui64);
                    }
                }
            }

            cJSON_Delete(response);
        }
    }

    return sourceRouteList;
}

/**
 * Get the lqi table for the given eui64
 *
 * @return linked list of zhalLqiData or NULL if fails
 */
icLinkedList *zhalGetLqiTable(uint64_t eui64)
{
    icLinkedList *lqiTable = NULL;

    icLogDebug(LOG_TAG, "zhalGetLqiTable");

    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "request", "getLqiTable");
    setAddress(eui64, request);

    cJSON *response;
    if (sendRequest(0, request, &response) == 0)
    {
        if (response != NULL)
        {
            lqiTable = linkedListCreate();

            cJSON *entries = cJSON_GetObjectItem(response, "entries");
            if (entries != NULL)
            {
                for (int i = 0; i < cJSON_GetArraySize(entries); ++i)
                {
                    cJSON *entry = cJSON_GetArrayItem(entries, i);
                    zhalLqiData *data = (zhalLqiData *) calloc(1, sizeof(zhalLqiData));
                    // Extract the eui64 and convert to a uint64_t
                    cJSON *euiStr = cJSON_GetObjectItem(entry, "eui");
                    if (euiStr != NULL && cJSON_IsString(euiStr))
                    {
                        sscanf(euiStr->valuestring, "%" PRIx64, &data->eui64);
                    }
                    else
                    {
                        icLogWarn(LOG_TAG, "Missing eui in getLqiTable response");
                    }
                    // Extract the lqi
                    cJSON *lqiNum = cJSON_GetObjectItem(entry, "lqi");
                    if (lqiNum != NULL && cJSON_IsNumber(lqiNum))
                    {
                        data->lqi = lqiNum->valueint;
                    }
                    else
                    {
                        icLogWarn(LOG_TAG, "Missing lqi in getLqiTable response");
                    }
                    linkedListAppend(lqiTable, data);
                }
            }

            cJSON_Delete(response);
        }
    }

    return lqiTable;
}

/**
 * Get the monitored devices info
 *
 * @return linked list of zhalLpmMonitoredDeviceInfo or NULL if fails
 */
icLinkedList *zhalGetMonitoredDevicesInfo(void)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *monitoredDevicesInfo = NULL;

    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "request", "getMonitoredDevices");

    cJSON *response = NULL;
    if (sendRequest(0, request, &response) == 0)
    {
        if (response != NULL)
        {
            cJSON *monitoredDeviceEntries = cJSON_GetObjectItem(response, "monitoredDeviceInfos");
            if (monitoredDeviceEntries != NULL)
            {
                for (int i = 0; i < cJSON_GetArraySize(monitoredDeviceEntries); i++)
                {
                    cJSON *monitoredDeviceEntry = cJSON_GetArrayItem(monitoredDeviceEntries, i);
                    // Extract the eui64 and convert to a uint64_t
                    //
                    cJSON *eui64Str = cJSON_GetObjectItem(monitoredDeviceEntry, "eui64");
                    // Extract the timerSeconds
                    //
                    cJSON *timerSeconds = cJSON_GetObjectItem(monitoredDeviceEntry, "timerSeconds");
                    if (eui64Str != NULL && cJSON_IsString(eui64Str) == true && timerSeconds != NULL &&
                        cJSON_IsNumber(timerSeconds) == true)
                    {
                        if (monitoredDevicesInfo == NULL)
                        {
                            monitoredDevicesInfo = linkedListCreate();
                        }
                        zhalLpmMonitoredDeviceInfo *deviceInfo =
                            (zhalLpmMonitoredDeviceInfo *) calloc(1, sizeof(zhalLpmMonitoredDeviceInfo));
                        if (stringToUnsignedNumberWithinRange(
                                eui64Str->valuestring, &deviceInfo->eui64, 16, 0, UINT64_MAX) == true)
                        {
                            deviceInfo->timeoutSeconds = timerSeconds->valueint;
                            linkedListAppend(monitoredDevicesInfo, deviceInfo);
                        }
                        else
                        {
                            icLogError(LOG_TAG, "monitored device info response missing/invalid 'eui64'");
                            free(deviceInfo);
                        }
                    }
                    else
                    {
                        icLogWarn(LOG_TAG, "Missing values in getMonitoredDevices response");
                    }
                }
            }
            cJSON_Delete(response);
        }
    }

    return monitoredDevicesInfo;
}


int zhalUpgradeDeviceFirmwareLegacy(uint64_t eui64,
                                    uint64_t routerEui64,
                                    const char *appFilename,
                                    const char *bootloaderFilename)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (appFilename == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return -1;
    }

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "upgradeDeviceFirmwareLegacy");

    setAddress(eui64, request);

    char buf[21]; // max uint64_t is 18446744073709551615
    sprintf(buf, "%016" PRIx64, routerEui64);
    cJSON_AddStringToObject(request, "routerAddress", buf);

    cJSON_AddStringToObject(request, "appFilename", appFilename);

    cJSON_AddNumberToObject(request, "authChallengeResponse", 0); // unused but required

    if (bootloaderFilename != NULL)
    {
        cJSON_AddStringToObject(request, "bootloaderFilename", bootloaderFilename);
    }

    return sendRequestNoResponse(eui64, request);
}

/**
 * Tells Zigbee core that system needs to go into LPM
 * passes the list of LPMMonitoredDeviceInfo objects
 *
 * @param deviceList - the list of devices to wake up LPM
 * @return 0 on success
 */
int zhalEnterLowPowerMode(icLinkedList *deviceList)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // create the basic request info
    //
    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "request", "enterLowPowerMode");
    cJSON *monitorDevices = cJSON_AddArrayToObject(request, "monitoredDeviceInfos");

    // only add devices to request if list exists,
    // in the case where there are no devices for the request
    // send the empty JSON array
    //
    if (deviceList != NULL)
    {
        icLinkedListIterator *listIter = linkedListIteratorCreate(deviceList);
        while (linkedListIteratorHasNext(listIter))
        {
            zhalLpmMonitoredDeviceInfo *deviceInfo = (zhalLpmMonitoredDeviceInfo *) linkedListIteratorGetNext(listIter);

            cJSON *arrayItem = cJSON_CreateObject();

            // add eui64 value to item which needs to be a string
            char eui64Buf[21];
            sprintf(eui64Buf, "%016" PRIx64, deviceInfo->eui64);

            cJSON_AddStringToObject(arrayItem, "eui64", eui64Buf);

            // add timeout seconds to item
            cJSON_AddNumberToObject(arrayItem, "timeoutSeconds", deviceInfo->timeoutSeconds);

            // add message handling value to item
            cJSON_AddNumberToObject(arrayItem, "messageHandling", deviceInfo->messageHandling);

            // add item to the array
            cJSON_AddItemToArray(monitorDevices, arrayItem);
        }

        // cleanup
        linkedListIteratorDestroy(listIter);
    }

    return sendRequestNoResponse(0, request);
}

/**
 * Tells Zigbee core that systems is now out of low power mode
 *
 * @return 0 on success
 */
int zhalExitLowPowerMode(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "exitLowPowerMode");

    return sendRequestNoResponse(0, request);
}

/**
 * Set a device communication lost timeout for Zigbee
 *
 * @param timeoutSeconds - number of seconds before device is considered to be in comm failure
 * @return 0 on success
 */
int zhalSetCommunicationFailTimeout(uint32_t timeoutSeconds)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "setCommunicationFailTimeout");
    cJSON_AddNumberToObject(request, "timeoutSeconds", timeoutSeconds);

    return sendRequestNoResponse(0, request);
}

/**
 * Get the firmware version
 * @return the firmware version, or NULL on failure.  Caller must free.
 */
char *zhalGetFirmwareVersion(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *version = NULL;

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "getFirmwareVersion");

    cJSON *response;
    int result = sendRequest(0, request, &response);
    if (result == 0 && response != NULL)
    {
        cJSON *item = cJSON_GetObjectItem(response, "version");
        if (item != NULL && cJSON_IsString(item))
        {
            version = strdup(item->valuestring);
        }
        else
        {
            icLogError(LOG_TAG, "getFirmwareVersion response missing 'version'");
        }

        cJSON_Delete(response);
    }

    return version;
}

// Helper to send the request, parse out the resultCode, and if successful return the response (otherwise it will be
// cleaned up if result code was nonzero)
//  also deletes the request json object
static int sendRequestWithTimeout(uint64_t eui64, cJSON *request, int timeoutSecs, cJSON **response)
{
    int result = -1;

    cJSON *resp = NULL;

    int retries = 0;
    while (retries++ < MAX_NETWORK_BUSY_RETRIES && zhalIsInitialized())
    {
        resp = zhalSendRequest(eui64, request, timeoutSecs);

        if (resp == NULL)
        {
            icLogError(LOG_TAG, "%s: zhalSendRequest returned NULL", __FUNCTION__);
            break;
        }

        cJSON *resultCode = cJSON_GetObjectItem(resp, "resultCode");
        if (resultCode != NULL)
        {
            result = resultCode->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "%s: missing resultCode from response", __FUNCTION__);
            break;
        }

        if (result == ZHAL_STATUS_OK)
        {
            // good result, we are done
            break;
        }
        else if (result == ZHAL_STATUS_NETWORK_BUSY)
        {
            // we got a network busy error from the stack (too many messages in flight at a time).
            // Remove the previously attached requestId (added by zhalSendRequest), wait a bit,
            // then try again
            cJSON_Delete(resp);
            resp = NULL;
            cJSON_DeleteItemFromObject(request, "requestId");
            icLogWarn(LOG_TAG, "%s: network busy, retrying", __FUNCTION__);
            usleep(NETWORK_BUSY_RETRY_DELAY_MILLIS * 1000);
        }
        else
        {
            // clean up for any other error (dont return the JSON response)
            cJSON_Delete(resp);
            resp = NULL;
            break;
        }
    }

    if (resp != NULL && response != NULL)
    {
        // the caller wanted a response json and we have one...
        *response = resp;
    }
    else
    {
        // The caller didn't want the response, we better clean it up
        cJSON_Delete(resp);
    }

    cJSON_Delete(request);

    return result;
}

int zhalAddZigbeeUart(uint64_t eui64, uint8_t endpointId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "addZigbeeUart");
    setAddress(eui64, request);

    cJSON_AddNumberToObject(request, "endpoint", endpointId);

    return sendRequestNoResponse(eui64, request);
}

// FIXME: This needs endpointId to be able find the correct port in ZigbeeCore. Devices might have > 1 UART ep.
int zhalRemoveZigbeeUart(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "removeZigbeeUart");
    setAddress(eui64, request);

    return sendRequestNoResponse(eui64, request);
}

int zhalSyncZigbeeUart(uint64_t eui64, uint8_t endpointId)
{
    icLogDebug(LOG_TAG, __func__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "syncZigbeeUart");
    setAddress(eui64, request);
    cJSON_AddNumberToObject(request, "endpoint", endpointId);

    return sendRequestNoResponse(eui64, request);
}

int zhalSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "setOtaUpgradeDelay");
    cJSON_AddNumberToObject(request, "delaySeconds", delaySeconds);

    return sendRequestNoResponse(0, request);
}

icLinkedList *zhalPerformEnergyScan(const uint8_t *channelsToScan,
                                    uint8_t numChannelsToScan,
                                    uint32_t scanDurationMillis,
                                    uint32_t numScans)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *result = NULL;

    if (channelsToScan == NULL || numChannelsToScan == 0)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return NULL;
    }

    uint32_t channelMask = 0;
    for (int i = 0; i < numChannelsToScan; i++)
    {
        channelMask |= 1u << channelsToScan[i];
    }

    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "request", "energyScan");
    cJSON_AddNumberToObject(request, "channelMask", channelMask);
    cJSON_AddNumberToObject(request, "scanDurationMillis", scanDurationMillis);
    cJSON_AddNumberToObject(request, "scanCount", numScans);

    cJSON *response = NULL;

    if (sendRequest(0, request, &response) == 0 && response != NULL)
    {
        cJSON *entries = cJSON_GetObjectItem(response, "entries");
        if (entries != NULL)
        {
            result = linkedListCreate();

            cJSON *iterator = NULL;
            cJSON_ArrayForEach(iterator, entries)
            {
                cJSON *chan = cJSON_GetObjectItem(iterator, "chan");
                cJSON *max = cJSON_GetObjectItem(iterator, "max");
                cJSON *min = cJSON_GetObjectItem(iterator, "min");
                cJSON *avg = cJSON_GetObjectItem(iterator, "avg");
                cJSON *score = cJSON_GetObjectItem(iterator, "score");

                if (chan != NULL && max != NULL && min != NULL && avg != NULL && score != NULL)
                {
                    zhalEnergyScanResult *scanResult = calloc(1, sizeof(*scanResult));

                    scanResult->channel = chan->valueint;
                    scanResult->maxRssi = max->valueint;
                    scanResult->minRssi = min->valueint;
                    scanResult->averageRssi = avg->valueint;
                    scanResult->score = score->valueint;

                    linkedListAppend(result, scanResult);
                }
                else
                {
                    icLogWarn(LOG_TAG, "%s: invalid scan entry JSON", __FUNCTION__);
                }
            }
        }

        cJSON_Delete(response);
    }

    return result;
}

bool zhalIncrementNetworkCounters(int32_t nonce, int32_t frame)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "incrementNetworkCounters");
    cJSON_AddNumberToObject(request, "nonce", nonce);
    cJSON_AddNumberToObject(request, "frame", frame);

    return sendRequestNoResponse(0, request) == 0;
}

bool zhalConfigureNetworkHealthCheck(uint32_t intervalMillis,
                                     int8_t ccaThreshold,
                                     uint32_t ccaFailureThreshold,
                                     uint32_t restoreThreshold,
                                     uint32_t delayBetweenThresholdRetriesMillis)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "networkHealthCheckConfigure");
    cJSON_AddNumberToObject(request, "intervalMillis", intervalMillis);
    cJSON_AddNumberToObject(request, "ccaThreshold", ccaThreshold);
    cJSON_AddNumberToObject(request, "ccaFailureThreshold", ccaFailureThreshold);
    cJSON_AddNumberToObject(request, "restoreThreshold", restoreThreshold);
    cJSON_AddNumberToObject(request, "delayBetweenThresholdRetriesMillis", delayBetweenThresholdRetriesMillis);

    return sendRequestNoResponse(0, request) == 0;
}

bool zhalDefenderConfigure(uint8_t panIdChangeThreshold,
                           uint32_t panIdChangeWindowMillis,
                           uint32_t panIdChangeRestoreMillis)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "defenderConfigure");
    cJSON_AddNumberToObject(request, "panIdChangeThreshold", panIdChangeThreshold);
    cJSON_AddNumberToObject(request, "panIdChangeWindowMillis", panIdChangeWindowMillis);
    cJSON_AddNumberToObject(request, "panIdChangeRestoreMillis", panIdChangeRestoreMillis);

    return sendRequestNoResponse(0, request) == 0;
}

bool zhalSetProperty(const char *key, const char *value)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "setProperty");
    cJSON_AddStringToObject(request, "key", key);

    cJSON *valueItem = NULL;
    if (value != NULL)
    {
        valueItem = cJSON_CreateString(value);
    }
    else
    {
        valueItem = cJSON_CreateNull();
    }

    cJSON_AddItemToObject(request, "value", valueItem);

    return sendRequestNoResponse(0, request) == 0;
}

char *zhalTest(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *testResult = NULL;

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "request", "test");

    cJSON *response = NULL;
    int result = sendRequest(0, request, &response);
    if (result == 0 && response != NULL)
    {
        cJSON *item = cJSON_GetObjectItem(response, "testResult");
        if (item != NULL)
        {
            testResult = cJSON_PrintUnformatted(item);
        }
        else
        {
            icLogError(LOG_TAG, "test: response missing 'testResult'");
        }
    }

    cJSON_Delete(response);

    return testResult;
}

// Helper to send the request, parse out the resultCode, and if successful return the response (otherwise it will be
// cleaned up if result code was nonzero)
//  also deletes the request json object
static int sendRequest(uint64_t eui64, cJSON *request, cJSON **response)
{
    return sendRequestWithTimeout(eui64, request, DEFAULT_REQUEST_TIMEOUT_SECONDS, response);
}

// Helper to send a request that should not have a response payload other than resultCode (which is returned as an int)
static int sendRequestNoResponse(uint64_t eui64, cJSON *request)
{
    return sendRequest(eui64, request, NULL);
}

// Helper to set the 'address' field to the provided eui64
static void setAddress(uint64_t eui64, cJSON *request)
{
    char buf[21]; // max uint64_t is 18446744073709551615
    sprintf(buf, "%016" PRIx64, eui64);
    cJSON_AddStringToObject(request, "address", buf);
}

// Helper function for linkedListDestroy to delete entry of address table.
void zhalAddressTableEntryDestroy(zhalAddressTableEntry *entry)
{
    if (entry != NULL)
    {
        free(entry->strEui64);
        free(entry);
    }
}

// Get address Table of Zigbee network.
icLinkedList *zhalGetAddressTable(void)
{
    icLinkedList *addressTable = NULL;
    cJSON *request = cJSON_CreateObject();
    cJSON *output = NULL;

    cJSON_AddStringToObject(request, "request", "getAddressTable");

    if (sendRequest(0, request, &output) == ZHAL_STATUS_OK)
    {
        addressTable = linkedListCreate();
        cJSON *addressTableArray = cJSON_GetObjectItem(output, "addressTable");

        cJSON *obj = NULL;
        cJSON_ArrayForEach(obj, addressTableArray)
        {
            if (obj != NULL)
            {
                cJSON *euiItem = cJSON_GetObjectItem(obj, "eui");
                if (cJSON_IsString(euiItem) == true)
                {
                    cJSON *idItem = cJSON_GetObjectItem(obj, "nodeId");
                    if (cJSON_IsNumber(idItem) == true)
                    {
                        zhalAddressTableEntry *entry = calloc(1, sizeof(zhalAddressTableEntry));
                        entry->nodeId = idItem->valueint;
                        entry->strEui64 = strdup(euiItem->valuestring);
                        linkedListAppend(addressTable, entry);
                    }
                }
            }
        }
    }
    cJSON_Delete(output);

    return addressTable;
}
