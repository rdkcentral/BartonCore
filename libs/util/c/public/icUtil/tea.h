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
 * tea.h
 *
 * TEA encrypt/decrypt functions that are carryover
 * from QNX days.  No idea where this code originally
 * came from, but we do know it works.
 *
 * Author: unknown
 *-----------------------------------------------*/

#ifndef IC_TEA_H_
#define IC_TEA_H_

#include <stdint.h>

#define KEY_SIZE 4
#define TRUE     1
#define FALSE    0

extern int teaEncrypt(unsigned char *data, char *keyStr, int dataSize);

extern int teaDecrypt(unsigned char *data, char *keyStr, int dataSize);

/* decryptBlock
 *   Decrypts byte array data of length len with key using TEA
 * Arguments:
 *   data - pointer to 8 bit data array to be decrypted
 *   len - length of array
 *   key - Pointer to four integer array (16 bytes) holding TEA key
 * Returns:
 *   data - decrypted data held here
 * Side effects:
 *   Modifies data
 */
int teaDecryptBlock(uint8_t *data, uint32_t *key, int len);

#endif /* IC_TEA_H_*/
