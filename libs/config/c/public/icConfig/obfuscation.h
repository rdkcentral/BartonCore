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
 * obfuscation.h
 *
 * attempt to hide data into a larger buffer.
 *
 * NOTE: this is NOT encryption, so use at your own risk!
 *
 * Author: jelderton -  8/18/16.
 *-----------------------------------------------*/

#ifndef FLEXCORE_OBFUSCATION_H
#define FLEXCORE_OBFUSCATION_H

#include <stdint.h>


/*
 * allow custom mechanism to perform the obfuscate and unobfuscate operations.
 */
typedef char *(
    *obfDelegate)(const char *passphrase, uint32_t passLen, const char *input, uint32_t inputLen, uint32_t *outputLen);

/*
 * provide custom solution for the obfuscation logic.
 */
void setObfuscateCustomImpl(obfDelegate implementation);
void setUnobfuscateCustomImpl(obfDelegate implementation);

/*
 * facade to obfuscate some data into a buffer that can be saved or used
 * for other purposes (note: may want to base64 or uuencode the
 * result as the output will be binary).
 * requires a 'passphrase' to serve as a key - which is combined with
 * the input data to produce an obfuscated result.
 *
 * caller must free the returned buffer
 *
 * NOTE: Will utilize a default implementation of obfDelegate unless
 *       pre-provided with a different function via setObfuscateCustomImpl().
 *       The default impl is not very secure, so consumers are encouraged to
 *       provide their own implementation!
 *
 * @param passphrase - well-known password to use while encoding 'input'
 * @param passLen - length of the password string
 * @param input - data to obfuscate
 * @param inputLen - length of the data to obfuscate
 * @param outputLen - length of the returned buffer.
 */
char *obfuscate(const char *passphrase, uint32_t passLen, const char *input, uint32_t inputLen, uint32_t *outputLen);

/*
 * facade to decode an obfuscated buffer; returning the original value.
 * requires the same 'passphrase' used when encoding the buffer.
 *
 * caller must free the returned buffer
 *
 * NOTE: Will utilize a default implementation of obfDelegate unless
 *       pre-provided with a different function via setUnobfuscateCustomImpl().
 *       The default impl is not very secure, so consumers are encouraged to
 *       provide their own implementation!
 *
 * @param passphrase - well-known password used during obfuscation
 * @param passLen - length of the password string
 * @param input - data to unobfuscate
 * @param inputLen - length of the data to unobfuscate
 * @param outputLen - length of the returned buffer.
 */
char *unobfuscate(const char *passphrase, uint32_t passLen, const char *input, uint32_t inputLen, uint32_t *outputLen);

#endif // FLEXCORE_OBFUSCATION_H
