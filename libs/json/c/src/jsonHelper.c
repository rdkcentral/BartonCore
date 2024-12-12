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
// Created by mkoch201 on 5/14/18.
//

#include <jsonHelper/jsonHelper.h>
#include <string.h>

extern inline void cJSON_Delete__auto(cJSON **json);

/**
 * Helper method to extract the string value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @return the value string, which the caller must free, or NULL if not a string or the key is not present
 */
char *getCJSONString(const cJSON *json, const char *key)
{
    cJSON *value = cJSON_GetObjectItem(json, key);
    if (value != NULL && cJSON_IsString(value))
    {
        return strdup(value->valuestring);
    }

    return NULL;
}

/**
 * Helper method to extract the string value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @return the value string, which the caller must NOT free, or NULL if not a string or the key is not present
 */
const char *getCJSONStringRef(const cJSON *json, const char *key)
{
    cJSON *value = cJSON_GetObjectItem(json, key);
    if (value != NULL && cJSON_IsString(value))
    {
        return value->valuestring;
    }

    return NULL;
}

/**
 * Helper method to extract the int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the int value
 * @return true if success, or false if key not present or not a number
 */
bool getCJSONInt(const cJSON *json, const char *key, int *value)
{
    bool retval = false;
    cJSON *valueJson = cJSON_GetObjectItem(json, key);
    if (valueJson != NULL && cJSON_IsNumber(valueJson))
    {
        *value = valueJson->valueint;
        retval = true;
    }

    return retval;
}

/**
 * Helper method to extract the int value from a json object, and validate its within a certain range
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the int value
 * @param minVal the minimum value
 * @param maxVal the maximum value
 * @return true if success, or false if key not present or not a number
 */
bool getCJSONIntInRange(const cJSON *json, const char *key, int *value, int minVal, int maxVal)
{
    bool retval = false;
    if (value != NULL)
    {
        if (getCJSONInt(json, key, value) == true)
        {
            if (*value <= maxVal && *value >= minVal)
            {
                retval = true;
            }
        }
    }
    return retval;
}

/**
 * Helper method to extract the int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the int8 value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONInt8(const cJSON *json, const char *key, int8_t *value)
{
    bool retval = false;
    if (value != NULL)
    {
        int temp;
        if (getCJSONIntInRange(json, key, &temp, INT8_MIN, INT8_MAX) == true)
        {
            *value = temp;
            retval = true;
        }
    }
    return retval;
}

/**
 * Helper method to extract the int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the uint8 value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONUInt8(const cJSON *json, const char *key, uint8_t *value)
{
    bool retval = false;
    if (value != NULL)
    {
        int temp;
        if (getCJSONIntInRange(json, key, &temp, 0, UINT8_MAX) == true)
        {
            *value = temp;
            retval = true;
        }
    }
    return retval;
}

/**
 * Helper method to extract the unsigned int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the uint32_t value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONUInt32(const cJSON *json, const char *key, uint32_t *value)
{
    bool retval = false;
    if (value != NULL)
    {
        double temp;
        if (getCJSONDouble(json, key, &temp) == true)
        {
            if (temp <= UINT32_MAX && temp >= 0)
            {
                *value = (uint32_t) temp;
                retval = true;
            }
        }
    }
    return retval;
}

/**
 * Helper method to extract the unsigned int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the uint64_t value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONUInt64(const cJSON *json, const char *key, uint64_t *value)
{
    bool retval = false;
    if (value != NULL)
    {
        double temp;
        if (getCJSONDouble(json, key, &temp) == true)
        {
            if (temp <= UINT64_MAX && temp >= 0)
            {
                *value = (uint64_t) temp;
                retval = true;
            }
        }
    }
    return retval;
}

/**
 * Helper method to extract the double value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the double value
 * @return true if success, or false if key not present or not a number
 */
bool getCJSONDouble(const cJSON *json, const char *key, double *value)
{
    bool retval = false;
    cJSON *valueJson = cJSON_GetObjectItem(json, key);
    if (valueJson != NULL && cJSON_IsNumber(valueJson))
    {
        *value = valueJson->valuedouble;
        retval = true;
    }

    return retval;
}

/**
 * Helper method to extract the bool value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the bool value
 * @return true if success, or false if key not present or not a bool
 */
bool getCJSONBool(const cJSON *json, const char *key, bool *value)
{
    bool retval = false;
    cJSON *valueJson = cJSON_GetObjectItem(json, key);
    if (valueJson != NULL && cJSON_IsBool(valueJson))
    {
        *value = (bool) cJSON_IsTrue(valueJson);
        retval = true;
    }

    return retval;
}

/**
 * Helper method to set the bool value within a json object
 *
 * @param json the json object
 * @param value the new value to set the json object to.
 */
void setCJSONBool(cJSON *json, bool value)
{
    if (cJSON_IsBool(json))
    {
        json->type = (json->type & ~0xFF) | (value ? cJSON_True : cJSON_False);

        json->valueint = value ? 1 : 0;
        json->valuedouble = value ? 1 : 0;
    }
}

/**
 * Add a json item to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the element
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONItem(cJSON *parent, const char *name, cJSON *value)
{
    if (name != NULL && value != NULL)
    {
        if (parent == NULL)
        {
            parent = cJSON_CreateObject();
        }
        cJSON_AddItemToObject(parent, name, value);
    }

    return parent;
}

/**
 * Add a number to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the number
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONNumber(cJSON *parent, const char *name, const double *value)
{
    if (name != NULL && value != NULL)
    {
        parent = addCJSONItem(parent, name, cJSON_CreateNumber(*value));
    }

    return parent;
}

/**
 * Add a number to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the number
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONUInt32(cJSON *parent, const char *name, const uint32_t *value)
{
    if (name != NULL && value != NULL)
    {
        parent = addCJSONItem(parent, name, cJSON_CreateNumber(*value));
    }

    return parent;
}

/**
 * Add a bool to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the bool
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONBoolean(cJSON *parent, const char *name, const bool *value)
{
    if (name != NULL && value != NULL)
    {
        parent = addCJSONItem(parent, name, cJSON_CreateBool(*value));
    }

    return parent;
}

/**
 * Add a string to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the string
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONString(cJSON *parent, const char *name, const char *value)
{
    if (name != NULL && value != NULL)
    {
        parent = addCJSONItem(parent, name, cJSON_CreateString(value));
    }

    return parent;
}
