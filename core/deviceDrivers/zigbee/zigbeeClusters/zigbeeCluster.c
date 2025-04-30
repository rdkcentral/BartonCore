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
//
// Created by mkoch201 on 3/6/19.
//

#include "zigbeeClusters/zigbeeCluster.h"
#include "string.h"
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>

#define TRUE_STR  "true"
#define FALSE_STR "false"
#define LOG_TAG   "zigbeeCluster"

/**
 * Add a boolean value to configuration metadata
 *
 * @param configurationMetadata the metadata to write to
 * @param key the key to add
 * @param value the value to add
 */
void addBoolConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, bool value)
{
    // Clear out in case it existed
    stringHashMapDelete(configurationMetadata, key, NULL);
    stringHashMapPutCopy(configurationMetadata, key, value ? TRUE_STR : FALSE_STR);
}

/**
 * Get a boolean value from configuration metadata
 * @param configurationMetadata the metadata to get from
 * @param key the key to get
 * @param defaultValue the default value if its not found
 * @return the value in the map, or defaultValue if not found
 */
bool getBoolConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, bool defaultValue)
{
    char *value = stringHashMapGet(configurationMetadata, key);
    if (value == NULL)
    {
        return defaultValue;
    }

    return strcmp(value, TRUE_STR) == 0;
}

/**
 * Add a number value to configuration metadata
 *
 * @param configurationMetadata the metadata to write to
 * @param key the key to add
 * @param value the value to add
 */
void addNumberConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, uint64_t value)
{
    // Clear out in case it existed
    //
    stringHashMapDelete(configurationMetadata, key, NULL);
    char *valueStr = stringBuilder("%" PRIu64, value);

    stringHashMapPut(configurationMetadata, strdup(key), valueStr);
}

/**
 * Get a number value from configuration metadata
 *
 * @param configurationMetadata the metadata to get from
 * @param defaultValue the default value if its not found
 * @return the value in the map, or defaultValue if not found
 */
uint64_t getNumberConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, uint64_t defaultValue)
{
    char *value = stringHashMapGet(configurationMetadata, key);
    uint64_t retVal = defaultValue;
    if (value != NULL)
    {
        if (stringToUint64(value, &retVal) == false)
        {
            icLogWarn(LOG_TAG, "Unable to convert '%s' to uint64", value);
        }
    }

    return retVal;
}
