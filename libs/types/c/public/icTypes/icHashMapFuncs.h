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
// Created by mkoch201 on 11/2/18.
//

#pragma once

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
