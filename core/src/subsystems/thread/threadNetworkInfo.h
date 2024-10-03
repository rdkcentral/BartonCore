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
 * Created by Thomas Lea on 12/7/21.
 */

#ifndef ZILKER_THREADNETWORKINFO_H
#define ZILKER_THREADNETWORKINFO_H

#include "cjson/cJSON.h"
#include <glib.h>
#include <stdint.h>

#define THREAD_OPERATIONAL_DATASET_MAX_LEN   254
#define THREAD_NETWORK_KEY_MAX_LEN           32

#define THREAD_NETWORK_INFO_CHANNEL_KEY      "channel"
#define THREAD_NETWORK_INFO_PAN_ID_KEY       "panId"
#define THREAD_NETWORK_INFO_EXT_PAN_ID_KEY   "extendedPanId"
#define THREAD_NETWORK_INFO_NETWORK_KEY_KEY  "networkKey"
#define THREAD_NETWORK_INFO_DATASET_KEY      "dataset"
#define THREAD_NETWORK_INFO_NETWORK_NAME_KEY "networkName"

typedef struct
{
    uint16_t channel;
    uint16_t panId;
    uint64_t extendedPanId;
    unsigned char networkKey[THREAD_NETWORK_KEY_MAX_LEN + 1]; // 16 hex digits plus \0
    uint8_t networkKeyLen;
    unsigned char dataset[THREAD_OPERATIONAL_DATASET_MAX_LEN + 1];
    uint8_t datasetLen;
    char *networkName;
} ThreadNetworkInfo;

/**
 * @brief Create a ThreadNetworkInfo
 *
 * @return ThreadNetworkInfo*
 */
ThreadNetworkInfo *threadNetworkInfoCreate(void);

/**
 * @brief Destroy a ThreadNetworkInfo
 *
 * @param threadInfo
 */
void threadNetworkInfoDestroy(ThreadNetworkInfo *threadInfo);

/**
 * @brief Encode a ThreadNetworkInfo to cJSON, or NULL on error.
 *
 * @note member `extendedPanId` is encoded into a string to preserve precision.
 *
 * @param threadInfo
 * @return cJSON*
 */
cJSON *threadNetworkInfoToJson(ThreadNetworkInfo *threadInfo);

// TODO fromJson

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ThreadNetworkInfo, threadNetworkInfoDestroy);

#endif // ZILKER_THREADNETWORKINFO_H
