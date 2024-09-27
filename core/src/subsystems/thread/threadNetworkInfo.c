//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
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
        free(threadInfo->networkName);
    }

    free(threadInfo);
}

cJSON *threadNetworkInfoToJson(ThreadNetworkInfo *threadInfo)
{
    cJSON *retVal = NULL;

    g_return_val_if_fail(threadInfo, NULL);

    scoped_generic char *networkKeyEncoded = g_base64_encode(threadInfo->networkKey, threadInfo->networkKeyLen);
    scoped_generic char *datasetEncoded = g_base64_encode(threadInfo->dataset, threadInfo->datasetLen);
    // Express pan id and extended pan id as hex strings for presentation (clients can parse out if they need to)
    g_autoptr(GString) panIdStr = g_string_new(NULL);
    g_string_printf(panIdStr, "%" PRIx16, threadInfo->panId);
    g_autoptr(GString) extPanIdStr = g_string_new(NULL);
    g_string_printf(extPanIdStr, "%" PRIx64, threadInfo->extendedPanId);

    retVal = cJSON_CreateObject();

    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_NETWORK_NAME_KEY, threadInfo->networkName);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_NETWORK_KEY_KEY, networkKeyEncoded);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_DATASET_KEY, datasetEncoded);
    cJSON_AddNumberToObject(retVal, THREAD_NETWORK_INFO_CHANNEL_KEY, threadInfo->channel);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_PAN_ID_KEY, panIdStr->str);
    cJSON_AddStringToObject(retVal, THREAD_NETWORK_INFO_EXT_PAN_ID_KEY, extPanIdStr->str);

    return retVal;
}
