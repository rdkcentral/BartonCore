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
// Created by mkoch201 on 7/31/20.
//

#include "jsonPointer/jsonPointer.h"

#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct JSONPointer
{
    icLinkedList *keys;
    char *pointer;
};

/**
 * Compile a json pointer
 *
 * @param jsonPointer the string representing the json pointer
 * @return the pointer object or null if pointer is invalid
 */
JSONPointer *jsonPointerCompile(const char *jsonPointer)
{
    JSONPointer *result = NULL;
    if (jsonPointer != NULL)
    {
        result = (JSONPointer *) calloc(1, sizeof(JSONPointer));
        result->keys = linkedListCreate();
        result->pointer = strdup(jsonPointer);
        size_t len = strlen(jsonPointer);
        if (len > 0)
        {
            char *token;
            // Create a copy, since we want to allow constant strings
            // Skip the leading /
            char *orig = strdup(jsonPointer + 1);
            char *rest = orig;

            while ((token = strtok_r(rest, "/", &rest)) != NULL)
            {
                if (strchr(token, '~') == NULL)
                {
                    linkedListAppend(result->keys, strdup(token));
                }
                else
                {
                    // ~ is escape character with these values
                    // ~0 = ~
                    // ~1 = /

                    // Do it in this order to avoid ~01 becoming /, when it should be ~1
                    char *newToken = stringReplace(token, "~1", "/");
                    char *newToken2 = stringReplace(newToken, "~0", "~");

                    linkedListAppend(result->keys, newToken2);
                    newToken2 = NULL;
                    free(newToken);
                }
            }

            // Special case if ends with / then the final key is an empty string
            if (jsonPointer[len - 1] == '/')
            {
                linkedListAppend(result->keys, strdup(""));
            }

            free(orig);
        }
        // If the string is empty that is a valid pointer, it just refers to the root document.  So we still return
        // a valid pointer and it will resolve to the root.
    }

    return result;
}

/**
 * Destroy and free memory for a json pointer
 *
 * @param pointer the pointer to destroy
 */
void jsonPointerDestroy(JSONPointer *pointer)
{
    if (pointer != NULL)
    {
        linkedListDestroy(pointer->keys, NULL);
        free(pointer->pointer);
        free(pointer);
    }
}

/**
 * Get the string representing the compiled json pointer
 * @param jsonPointer the pointer object
 * @return the immutable string representing the compiled json pointer
 */
const char *jsonPointerToString(const JSONPointer *pointer)
{
    const char *str = NULL;
    if (pointer != NULL)
    {
        str = pointer->pointer;
    }

    return str;
}

/**
 * Helper to access an element by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @return the json element or NULL if not found
 */
cJSON *jsonPointerResolve(const cJSON *json, const JSONPointer *jsonPointer)
{
    cJSON *result = NULL;

    if (jsonPointer != NULL)
    {
        // Resolve away the const, required due to C limitations(no overloading to provide const and non-const
        // versions). In this case the result can be modified however.  We use const on the argument to indicate we
        // won't modify it ourselves
        result = (cJSON *) json;
        icLinkedListIterator *iter = linkedListIteratorCreate(jsonPointer->keys);
        while (result != NULL && linkedListIteratorHasNext(iter))
        {
            char *item = (char *) linkedListIteratorGetNext(iter);
            if (cJSON_IsArray(result) == true)
            {
                uint64_t index;
                if (stringToUnsignedNumberWithinRange(item, &index, 10, 0, INT_MAX) == true)
                {
                    result = cJSON_GetArrayItem(result, index);
                }
                else
                {
                    result = NULL;
                    break;
                }
            }
            else
            {
                result = cJSON_GetObjectItem(result, item);
            }
        }
        linkedListIteratorDestroy(iter);
    }

    return result;
}

/**
 * Helper to access an element by uncompiled JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @return the json element or NULL if not found
 */
cJSON *jsonPointerStringResolve(const cJSON *json, const char *jsonPointerString)
{
    cJSON *result = NULL;
    if (json != NULL && jsonPointerString != NULL)
    {
        JSONPointer *pointer = jsonPointerCompile(jsonPointerString);
        result = jsonPointerResolve(json, pointer);
        jsonPointerDestroy(pointer);
    }

    return result;
}

/**
 * Helper to access a string value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param stringValue the storage for the string.  The value will be populated with a pointer to memory from the JSON
 * object and as such should not be freed.
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveString(const cJSON *json, const JSONPointer *jsonPointer, char **stringValue)
{
    bool success = false;
    if (stringValue != NULL)
    {
        cJSON *result = jsonPointerResolve(json, jsonPointer);
        if (result != NULL && cJSON_IsString(result) == true)
        {
            *stringValue = result->valuestring;
            success = true;
        }
    }

    return success;
}

/**
 * Helper to access a string value by uncompiled JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param stringValue the storage for the string.  The value will be populated with a pointer to memory from the JSON
 * object and as such should not be freed.
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveString(const cJSON *json, const char *jsonPointerString, char **stringValue)
{
    bool success = false;
    if (stringValue != NULL)
    {
        cJSON *result = jsonPointerStringResolve(json, jsonPointerString);
        if (result != NULL && cJSON_IsString(result) == true)
        {
            *stringValue = result->valuestring;
            success = true;
        }
    }

    return success;
}

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param boolValue the storage for the bool
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveBool(const cJSON *json, const JSONPointer *jsonPointer, bool *boolValue)
{
    bool success = false;
    if (boolValue != NULL)
    {
        cJSON *result = jsonPointerResolve(json, jsonPointer);
        if (result != NULL && cJSON_IsBool(result) == true)
        {
            *boolValue = cJSON_IsTrue(result) == true;
            success = true;
        }
    }

    return success;
}

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param boolValue the storage for the bool
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveBool(const cJSON *json, const char *jsonPointerString, bool *boolValue)
{
    bool success = false;
    if (boolValue != NULL)
    {
        cJSON *result = jsonPointerStringResolve(json, jsonPointerString);
        if (result != NULL && cJSON_IsBool(result) == true)
        {
            *boolValue = cJSON_IsTrue(result) == true;
            success = true;
        }
    }

    return success;
}

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param intValue the storage for the int
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveInt(const cJSON *json, const JSONPointer *jsonPointer, int *intValue)
{
    bool success = false;
    if (intValue != NULL)
    {
        cJSON *result = jsonPointerResolve(json, jsonPointer);
        if (result != NULL && cJSON_IsNumber(result) == true)
        {
            *intValue = result->valueint;
            success = true;
        }
    }

    return success;
}

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param intValue the storage for the int
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveInt(const cJSON *json, const char *jsonPointerString, int *intValue)
{
    bool success = false;
    if (intValue != NULL)
    {
        cJSON *result = jsonPointerStringResolve(json, jsonPointerString);
        if (result != NULL && cJSON_IsNumber(result) == true)
        {
            *intValue = result->valueint;
            success = true;
        }
    }

    return success;
}

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param doubleValue the storage for the double
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveDouble(const cJSON *json, const JSONPointer *jsonPointer, double *doubleValue)
{
    bool success = false;
    if (doubleValue != NULL)
    {
        cJSON *result = jsonPointerResolve(json, jsonPointer);
        if (result != NULL && cJSON_IsNumber(result) == true)
        {
            *doubleValue = result->valuedouble;
            success = true;
        }
    }

    return success;
}

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param doubleValue the storage for the double
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveDouble(const cJSON *json, const char *jsonPointerString, double *doubleValue)
{
    bool success = false;
    if (doubleValue != NULL)
    {
        cJSON *result = jsonPointerStringResolve(json, jsonPointerString);
        if (result != NULL && cJSON_IsNumber(result) == true)
        {
            *doubleValue = result->valuedouble;
            success = true;
        }
    }

    return success;
}

/**
 * Populate a JSON object using the structure described by the pointer.  The object structure
 * will only be created if the value is not null
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add. This memory is owned by jsonPointerCreateObject upon passing as an argument and will
 * be cleaned up in an error scenario.
 * @return the base object
 */
cJSON *jsonPointerCreateObject(cJSON *baseObject, const JSONPointer *pointer, cJSON *value)
{
    // Whether or not value was added or replaced an item in baseObject. We own the memory, so if we fail
    // to add it, we gotta clean it up.
    bool added = false;

    if (value != NULL && pointer != NULL)
    {
        if (baseObject == NULL)
        {
            baseObject = cJSON_CreateObject();
        }
        int keyCount = linkedListCount(pointer->keys);
        icLinkedListIterator *iter = linkedListIteratorCreate(pointer->keys);
        int i = 1;
        cJSON *parent = baseObject;

        while (linkedListIteratorHasNext(iter) == true)
        {
            char *item = (char *) linkedListIteratorGetNext(iter);
            cJSON *child = cJSON_GetObjectItem(parent, item);
            if (child == NULL)
            {
                // Member along pointer doesn't exist yet, create and add it to parent
                if (i < keyCount)
                {
                    child = cJSON_CreateObject();
                    added = cJSON_AddItemToObject(parent, item, child);
                }
                else
                {
                    added = cJSON_AddItemToObject(parent, item, value);
                }
            }
            else if (i == keyCount)
            {
                // Member exists but is the last member. Replace the value.
                added = cJSON_ReplaceItemInObject(parent, item, value);
            }

            // else this member already exists, continue on
            parent = child;
            ++i;
        }
        linkedListIteratorDestroy(iter);
    }

    if (added == false)
    {
        cJSON_Delete(value);
    }

    return baseObject;
}

/*
 * Handy macro for casting non-double types to double before passing to jsonPointerCreateNumber
 */
#define createNumberWrapper(object, pointer, value)                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((value) != NULL)                                                                                           \
        {                                                                                                              \
            double temp = *(value);                                                                                    \
            (object) = jsonPointerCreateNumber(object, pointer, &temp);                                                \
        }                                                                                                              \
        return object;                                                                                                 \
    } while (0)

/**
 * Populate a JSON object with the given number value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 * @see jsonPointerCreateNumber
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateInt8(cJSON *baseObject, const JSONPointer *pointer, const int8_t *value)
{
    createNumberWrapper(baseObject, pointer, value);
}

/**
 * Populate a JSON object with the given number value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 * @see jsonPointerCreateNumber
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateUint8(cJSON *baseObject, const JSONPointer *pointer, const uint8_t *value)
{
    createNumberWrapper(baseObject, pointer, value);
}

/**
 * Populate a JSON object with the given number value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 * @see jsonPointerCreateNumber
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateInt32(cJSON *baseObject, const JSONPointer *pointer, const int32_t *value)
{
    createNumberWrapper(baseObject, pointer, value);
}

/**
 * Populate a JSON object with the given number value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 * @see jsonPointerCreateNumber
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateUint32(cJSON *baseObject, const JSONPointer *pointer, const uint32_t *value)
{
    createNumberWrapper(baseObject, pointer, value);
}

/**
 * Populate a JSON object with the given number value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 * @see jsonPointerCreateNumber
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateUint64(cJSON *baseObject, const JSONPointer *pointer, const uint64_t *value)
{
    createNumberWrapper(baseObject, pointer, value);
}

/**
 * Populate a JSON object with the given number value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateNumber(cJSON *baseObject, const JSONPointer *pointer, const double *value)
{
    if (value != NULL)
    {
        baseObject = jsonPointerCreateObject(baseObject, pointer, cJSON_CreateNumber(*value));
    }

    return baseObject;
}

/**
 * Populate a JSON object with the given bool value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateBoolean(cJSON *baseObject, const JSONPointer *pointer, const bool *value)
{
    if (value != NULL)
    {
        baseObject = jsonPointerCreateObject(baseObject, pointer, cJSON_CreateBool(*value));
    }

    return baseObject;
}

/**
 * Populate a JSON object with the given string value using the structure described by the pointer.
 * The object structure will only be created if the value is not null
 *
 * NOTE: Does not work correctly if the pointer contains array indices
 * @param baseObject the base object to add items to.  Can be NULL in which case the base object is created if necessary
 * @param pointer the pointer to base the structure on
 * @param value the value to add
 * @return the base object
 */
cJSON *jsonPointerCreateString(cJSON *baseObject, const JSONPointer *pointer, const char *value)
{
    if (value != NULL)
    {
        baseObject = jsonPointerCreateObject(baseObject, pointer, cJSON_CreateString(value));
    }

    return baseObject;
}
