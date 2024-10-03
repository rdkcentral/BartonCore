//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast
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
// Created by mkoch201 on 7/31/20.
//

#ifndef ZILKER_JSONPOINTER_H
#define ZILKER_JSONPOINTER_H

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * RFC6901 JSON Pointer implementation on top of cJSON
 * @link https://tools.ietf.org/html/rfc6901
 */

typedef struct JSONPointer JSONPointer;

/**
 * Compile a json pointer
 *
 * @param jsonPointer the string representing the json pointer
 * @return the pointer object or null if pointer is invalid
 */
JSONPointer *jsonPointerCompile(const char *jsonPointer);

/**
 * Destroy and free memory for a json pointer
 *
 * @param pointer the pointer to destroy
 */
void jsonPointerDestroy(JSONPointer *pointer);

/**
 * Get the string representing the compiled json pointer
 * @param pointer the pointer object
 * @return the immutable string representing the compiled json pointer
 */
const char *jsonPointerToString(const JSONPointer *pointer);

/**
 * Helper to access an element by JSON pointer
 * @link https://tools.ietf.org/html/rfc6901
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @return the json element or NULL if not found
 */
cJSON *jsonPointerResolve(const cJSON *json, const JSONPointer *jsonPointer);

/**
 * Helper to access an element by uncompiled JSON pointer
 * @link https://tools.ietf.org/html/rfc6901
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @return the json element or NULL if not found
 */
cJSON *jsonPointerStringResolve(const cJSON *json, const char *jsonPointerString);

/**
 * Helper to access a string value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param stringValue the storage for the string.  The value will be populated with a pointer to memory from the JSON
 * object and as such should not be freed.
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveString(const cJSON *json, const JSONPointer *jsonPointer, char **stringValue);

/**
 * Helper to access a string value by uncompiled JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param stringValue the storage for the string.  The value will be populated with a pointer to memory from the JSON
 * object and as such should not be freed.
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveString(const cJSON *json, const char *jsonPointerString, char **stringValue);

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param boolValue the storage for the bool
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveBool(const cJSON *json, const JSONPointer *jsonPointer, bool *boolValue);

/**
 * Helper to access a bool value by uncompiled JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param boolValue the storage for the bool
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveBool(const cJSON *json, const char *jsonPointerString, bool *boolValue);

/**
 * Helper to access a int value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param intValue the storage for the int
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveInt(const cJSON *json, const JSONPointer *jsonPointer, int *intValue);

/**
 * Helper to access a int value by uncompiled JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param intValue the storage for the int
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveInt(const cJSON *json, const char *jsonPointerString, int *intValue);

/**
 * Helper to access a bool value by JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointer the json pointer
 * @param doubleValue the storage for the double
 * @return true if successful, false otherwise
 */
bool jsonPointerResolveDouble(const cJSON *json, const JSONPointer *jsonPointer, double *doubleValue);

/**
 * Helper to access a bool value by uncompiled JSON pointer
 *
 * @param json the json against which to evaluate
 * @param jsonPointerString the json pointer string
 * @param doubleValue the storage for the double
 * @return true if successful, false otherwise
 */
bool jsonPointerStringResolveDouble(const cJSON *json, const char *jsonPointerString, double *doubleValue);

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
cJSON *jsonPointerCreateObject(cJSON *baseObject, const JSONPointer *pointer, cJSON *value);

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
cJSON *jsonPointerCreateInt8(cJSON *baseObject, const JSONPointer *pointer, const int8_t *value);

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
cJSON *jsonPointerCreateUint8(cJSON *baseObject, const JSONPointer *pointer, const uint8_t *value);

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
cJSON *jsonPointerCreateInt32(cJSON *baseObject, const JSONPointer *pointer, const int32_t *value);

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
cJSON *jsonPointerCreateUint32(cJSON *baseObject, const JSONPointer *pointer, const uint32_t *value);

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
cJSON *jsonPointerCreateUint64(cJSON *baseObject, const JSONPointer *pointer, const uint64_t *value);

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
cJSON *jsonPointerCreateNumber(cJSON *baseObject, const JSONPointer *pointer, const double *value);

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
cJSON *jsonPointerCreateBoolean(cJSON *baseObject, const JSONPointer *pointer, const bool *value);

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
cJSON *jsonPointerCreateString(cJSON *baseObject, const JSONPointer *pointer, const char *value);


#endif // ZILKER_JSONPOINTER_H
