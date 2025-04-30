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
 * versionUtils.h
 *
 * set of helper functions for parsing versions that
 * are specific to device descriptors
 *
 * Author: jelderton -  6/16/16.
 *-----------------------------------------------*/

#ifndef FLEXCORE_VERSIONUTILS_H
#define FLEXCORE_VERSIONUTILS_H

#include <stdint.h>

/*
 * parse a version string, breaking it into an
 * array of integers.  each non-digit character
 * will be treated as a separator.
 * for example:
 *    1.0.3.R13 --> 1, 0, 3, 13 : arrayLen=4
 * caller must free the returned array (even if arrayLen == 0)
 */
uint32_t *versionStringToInt(const char *versionStr, uint16_t *arrayLen);

/*
 * compare two version arrays (assumed created via versionStringToInt)
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal
 * positive-num - if 'right' is greater.
 */
int8_t compareVersionArrays(uint32_t *left, uint16_t leftLen, uint32_t *right, uint16_t rightLen);

/*
 * compare two version strings
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal
 * positive-num - if 'right' is greater.
 */
int8_t compareVersionStrings(const char *leftVersion, const char *rightVersion);

#endif // FLEXCORE_VERSIONUTILS_H
