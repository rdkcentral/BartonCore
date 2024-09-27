//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 3/6/19.
//

#include <icUtil/stringUtils.h>
#include <icLog/logging.h>
#include "zigbeeClusters/zigbeeCluster.h"
#include "string.h"

#define TRUE_STR "true"
#define FALSE_STR "false"
#define LOG_TAG "zigbeeCluster"

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

    return strcmp(value,TRUE_STR) == 0;
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
    char *valueStr = stringBuilder("%"PRIu64, value);

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

