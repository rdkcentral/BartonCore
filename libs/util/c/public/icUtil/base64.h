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

/*-----------------------------------------------
 * base64.h
 *
 * Helper functions for performing Base64
 * encode/decode operations.
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IC_BASE64_H
#define IC_BASE64_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Encode a binary array into a Base-64 string.  If the return
 * is not-NULL, then the caller should free the memory.
 *
 * @return encoded string otherwise NULL
 */
extern char *icEncodeBase64(uint8_t *array, uint16_t arrayLen);

/**
 * Decode the base64 encoded string 'src'.  If successful,
 * this will return true and allocate/populate the 'array' and 'arrayLen'.
 * in that case, the caller must free the array.
 */
extern bool icDecodeBase64(const char *srcStr, uint8_t **array, uint16_t *arrayLen);

/**
 * Decode the base64 encoded url string 'src'.  If successful,
 * this will return true and allocate/populate the 'array' and 'arrayLen'.
 * in that case, the caller must free the array.
 */
extern bool icDecodeUrlBase64(const char *srcStr, uint8_t **array, uint16_t *arrayLen);

#endif
