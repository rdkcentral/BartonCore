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
 * Created by Thomas Lea on 12/7/21.
 */

#pragma once

#include "cjson/cJSON.h"
#include <glib.h>
#include <stdint.h>

#define THREAD_NETWORK_KEY_LEN                  16
#define THREAD_BORDER_AGENT_ID_LEN              16

// JSON key names
#define THREAD_NETWORK_INFO_CHANNEL_KEY         "channel"
#define THREAD_NETWORK_INFO_PAN_ID_KEY          "panId"
#define THREAD_NETWORK_INFO_EXT_PAN_ID_KEY      "extendedPanId"
#define THREAD_NETWORK_INFO_NETWORK_KEY_KEY     "networkKey"
#define THREAD_NETWORK_INFO_ROLE_KEY            "role"
#define THREAD_NETWORK_INFO_ACTIVE_DATASET_KEY  "activeDataset"
#define THREAD_NETWORK_INFO_PENDING_DATASET_KEY "pendingDataset"
#define THREAD_NETWORK_INFO_NETWORK_NAME_KEY    "networkName"
#define THREAD_NETWORK_INFO_NAT64_ENABLED_KEY   "nat64Enabled"
#define THREAD_BORDER_AGENT_ID_KEY              "borderAgentId"
#define THREAD_VERSION_KEY                      "threadVersion"
#define THREAD_INTERFACE_UP_KEY                 "interfaceUp"

typedef struct
{
    uint16_t channel;
    uint16_t panId;
    uint64_t extendedPanId;
    uint8_t networkKey[THREAD_NETWORK_KEY_LEN];
    uint8_t *activeDataset;
    uint8_t activeDatasetLen;
    uint8_t *pendingDataset;
    uint8_t pendingDatasetLen;
    char *networkName;
    char *role;
    gboolean nat64Enabled;
    uint8_t borderAgentId[THREAD_BORDER_AGENT_ID_LEN];
    uint16_t threadVersion;
    gboolean interfaceUp;
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

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ThreadNetworkInfo, threadNetworkInfoDestroy);
