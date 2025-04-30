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
 * Created by Christian Leithner on 2/5/24.
 */

#include "threadNetworkInfo.h"
#include "cjson/cJSON.h"
#include "icTypes/sbrm.h"
#include <asm-generic/errno.h>
#include <inttypes.h>


ThreadNetworkInfo *threadNetworkInfoCreate(void)
{
    ThreadNetworkInfo *retVal = (ThreadNetworkInfo *) calloc(1, sizeof(ThreadNetworkInfo));
    return retVal;
}

void threadNetworkInfoDestroy(ThreadNetworkInfo *threadInfo)
{
    if (threadInfo)
    {
        free(threadInfo->activeDataset);
        free(threadInfo->pendingDataset);
        free(threadInfo->networkName);
        free(threadInfo->role);
        free(threadInfo);
    }
}

cJSON *threadNetworkInfoToJson(ThreadNetworkInfo *threadInfo)
{
    cJSON *retVal = NULL;

    g_return_val_if_fail(threadInfo, NULL);

    scoped_generic char *networkKeyEncoded = g_base64_encode(threadInfo->networkKey, sizeof(threadInfo->networkKey));
    scoped_generic char *activeDatasetEncoded = g_base64_encode(threadInfo->activeDataset, threadInfo->activeDatasetLen);
    scoped_generic char *pendingDatasetEncoded = g_base64_encode(threadInfo->pendingDataset, threadInfo->pendingDatasetLen);
    scoped_generic char *borderAgentIdEncoded = g_base64_encode(threadInfo->borderAgentId, sizeof(threadInfo->borderAgentId));
    // Express pan id and extended pan id as hex strings for presentation (clients can parse out if they need to)
    g_autoptr(GString) panIdStr = g_string_new(NULL);
    g_string_printf(panIdStr, "%" PRIx16, threadInfo->panId);
    g_autoptr(GString) extPanIdStr = g_string_new(NULL);
    g_string_printf(extPanIdStr, "%" PRIx64, threadInfo->extendedPanId);
    g_autoptr(GString) threadVersionStr = g_string_new(NULL);
    g_string_printf(threadVersionStr, "%" PRIx16, threadInfo->threadVersion);

    retVal = cJSON_CreateObject();

    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_NETWORK_NAME_KEY, threadInfo->networkName);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_NETWORK_KEY_KEY, networkKeyEncoded);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_ROLE_KEY, threadInfo->role);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_ACTIVE_DATASET_KEY, activeDatasetEncoded);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_PENDING_DATASET_KEY, pendingDatasetEncoded);
    cJSON_AddNumberToObject(retVal, THREAD_NETWORK_INFO_CHANNEL_KEY, threadInfo->channel);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_PAN_ID_KEY, panIdStr->str);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_EXT_PAN_ID_KEY, extPanIdStr->str);
    cJSON_AddBoolToObject(retVal, THREAD_NETWORK_INFO_NAT64_ENABLED_KEY, threadInfo->nat64Enabled);
    cJSON_AddStringToObject(retVal, THREAD_BORDER_AGENT_ID_KEY, borderAgentIdEncoded);
    cJSON_AddStringToObject(retVal, THREAD_VERSION_KEY, threadVersionStr->str);
    cJSON_AddBoolToObject(retVal, THREAD_INTERFACE_UP_KEY, threadInfo->interfaceUp);

    return retVal;
}
