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
// Created by mkoch201 on 5/14/18.
//

#ifndef ZILKER_JSONHELPER_H
#define ZILKER_JSONHELPER_H

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdint.h>
#include <icTypes/sbrm.h>

/**
 * Helper method to extract the string value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @return the value string, which the caller must free, or NULL if not a string or the key is not present
 */
char *getCJSONString(const cJSON *json, const char *key);

/**
 * Helper method to extract the string value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @return the value string, which the caller must NOT free, or NULL if not a string or the key is not present
 */
const char *getCJSONStringRef(const cJSON *json, const char *key);

/**
 * Helper method to extract the int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the int value
 * @return true if success, or false if key not present or not a number
 */
bool getCJSONInt(const cJSON *json, const char *key, int *value);

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
bool getCJSONIntInRange(const cJSON *json, const char *key, int *value, int minVal, int maxVal);

/**
 * Helper method to extract the int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the int8 value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONInt8(const cJSON *json, const char *key, int8_t *value);

/**
 * Helper method to extract the int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the uint8 value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONUInt8(const cJSON *json, const char *key, uint8_t *value);

/**
 * Helper method to extract the unsigned int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the uint32_t value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONUInt32(const cJSON *json, const char *key, uint32_t *value);

/**
 * Helper method to extract the unsigned int value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the uint64_t value
 * @return true if success, or false if key not present or not a number or out of range
 */
bool getCJSONUInt64(const cJSON *json, const char *key, uint64_t *value);

/**
 * Helper method to extract the double value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the double value
 * @return true if success, or false if key not present or not a number
 */
bool getCJSONDouble(const cJSON *json, const char *key, double *value);

/**
 * Helper method to extract the bool value from a json object
 *
 * @param json the json object
 * @param key the key for which value we want
 * @param value output location for the bool value
 * @return true if success, or false if key not present or not a bool
 */
bool getCJSONBool(const cJSON *json, const char *key, bool *value);

/**
 * Helper method to set the bool value within a json object
 *
 * @param json the json object
 * @param value the new value to set the json object to.
 */
void setCJSONBool(cJSON *json, bool value);

inline void cJSON_Delete__auto(cJSON **json)
{
    cJSON_Delete(*json);
}

#define scoped_cJSON AUTO_CLEAN(cJSON_Delete__auto) cJSON

/**
 * Add a json item to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the element
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONItem(cJSON *parent, const char *name, cJSON *value);

/**
 * Add a number to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the number
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONNumber(cJSON *parent, const char *name, const double *value);

/**
 * Add a number to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the number
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONUInt32(cJSON *parent, const char *name, const uint32_t *value);

/**
 * Add a bool to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the bool
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONBoolean(cJSON *parent, const char *name, const bool *value);

/**
 * Add a string to a parent (potentially creating the parent), if the value is not null.
 *
 * @param parent the parent object or NULL to have the parent object created
 * @param name the name of the element
 * @param value the value of the string
 * @return the parent object or NULL if invalid values are passed
 */
cJSON *addCJSONString(cJSON *parent, const char *name, const char *value);

#endif //ZILKER_JSONHELPER_H
