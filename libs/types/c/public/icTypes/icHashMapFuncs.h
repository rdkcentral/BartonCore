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
// Created by mkoch201 on 11/2/18.
//

#ifndef ZILKER_ICHASHMAPFUNCS_H
#define ZILKER_ICHASHMAPFUNCS_H

/*
 * function prototype for freeing 'keys' and 'values'
 * within the HashMap.   used during deleteFromHashMap()
 * and hashMapDestroy()
 *
 * @param key - the current 'key' that needs to be freed
 * @param value - the current 'value' that needs to be freed
 */
typedef void (*hashMapFreeFunc)(void *key, void *value);

typedef void (*hashMapCloneFunc)(void *key, void *value, void **clonedKey, void **clonedValue, void *context);

/*
 * common implementation of the hashMapFreeFunc function that can be
 * used in situations where the 'key' and 'value' are not freed at all.
 */
void standardDoNotFreeHashMapFunc(void *key, void *value);

#endif // ZILKER_ICHASHMAPFUNCS_H
