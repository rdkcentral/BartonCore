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
 * obfuscation.c
 *
 * attempt to hide data into a larger buffer.
 *
 * NOTE: this is NOT encryption, so use at your own risk!
 *
 * Author: jelderton -  8/18/16.
 *-----------------------------------------------*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <icConfig/obfuscation.h>
#include <icConcurrent/threadUtils.h>

// private functions
static char *defaultObfuscateImpl(const char *passphrase, uint32_t passLen,
                                  const char *input, uint32_t inputLen,
                                  uint32_t *outputLen);
static char *defaultUnobfuscateImpl(const char *passphrase, uint32_t passLen,
                                    const char *input, uint32_t inputLen,
                                    uint32_t *outputLen);

// private variables
static pthread_mutex_t MTX = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static obfDelegate encodeImpl = defaultObfuscateImpl;
static obfDelegate decodeImpl = defaultUnobfuscateImpl;

/*
 * provide custom solution for the obfuscation logic.
 */
void setObfuscateCustomImpl(obfDelegate implementation)
{
    mutexLock(&MTX);
    encodeImpl = implementation;
    mutexUnlock(&MTX);
}

/*
 * provide custom solution for the obfuscation logic.
 */
void setUnobfuscateCustomImpl(obfDelegate implementation)
{
    mutexLock(&MTX);
    decodeImpl = implementation;
    mutexUnlock(&MTX);
}

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
char *obfuscate(const char *passphrase, uint32_t passLen, const char *input, uint32_t inputLen, uint32_t *outputLen)
{
    // sanity check.  cannot look at the input string
    // as it could start with a NULL character
    if (passLen == 0 || inputLen == 0)
    {
        return NULL;
    }

    // forward to the delegate implementation
    pthread_mutex_lock(&MTX);
    obfDelegate delegate = encodeImpl;
    pthread_mutex_unlock(&MTX);

    if (delegate != NULL)
    {
        return delegate(passphrase, passLen, input, inputLen, outputLen);
    }
    return NULL;
}

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
char *unobfuscate(const char *passphrase, uint32_t passLen, const char *input, uint32_t inputLen, uint32_t *outputLen)
{
    // sanity check.  cannot look at the input string
    // as it could start with a NULL character
    if (passLen == 0 || inputLen == 0)
    {
        return NULL;
    }

    // forward to the delegate implementation
    pthread_mutex_lock(&MTX);
    obfDelegate delegate = decodeImpl;
    pthread_mutex_unlock(&MTX);

    if (delegate != NULL)
    {
        return delegate(passphrase, passLen, input, inputLen, outputLen);
    }
    return NULL;
}

/*
 * generate a buffer of 'outputLen', then replicate 'input' as many
 * times as possible to fill the buffer.
 * (ex:  abc123 --> abc123abc123abc12)
 */
static char *fillByDuplication(const char *input, uint32_t inputLen, uint32_t outputLen)
{
    // get the length of the input string, and truncate
    // if already larger then the target length
    //
    if (inputLen > outputLen)
    {
        inputLen = outputLen;
    }

    // create the buffer to write into (leave room for NULL char)
    //
    char *buffer = (char *) calloc(outputLen + 1, sizeof(char));
    char *start = buffer;
    int offset = 0;

    // loop as much as necessary to duplicate 'input'
    // until we reach the desired length
    // (ex:  input=abc123 len=18 --> abc123abc123abc123)
    //
    while (offset < outputLen)
    {
        // see how much room we have
        //
        uint32_t remain = (outputLen - offset);
        if (remain > inputLen)
        {
            remain = inputLen;
        }

        // copy that many characters, starting at where
        // we left off the last copy
        //
        strncpy(start, input, remain);

        // update counters/pointers before looping again
        //
        offset += remain;
        start += remain;
    }

    return buffer;
}

/*
 * bitwise magic of 'key' and 'input'.  returns a buffer with
 * the same size as 'inputLen' (assumes input is longer then key)
 */
static char *mask(const char *key, uint32_t keyLen, const char *input, uint32_t inputLen)
{
    // dup 'key' to make as-long-as 'input'
    //
    char *adjusted = fillByDuplication(key, keyLen, inputLen);

    // now that we have 2 character arrays of the same
    // length, loop through and bitwise XOR them together
    //
    char *buffer = (char *)calloc(inputLen+1, sizeof(char));
    uint32_t x = 0;
    for (x = 0 ; x < inputLen ; x++)
    {
        buffer[x] = (adjusted[x] ^ input[x]);
    }

    // cleanup
    //
    free(adjusted);
    return buffer;
}

/**
 * Simplistic obfuscation implementation to use as a default.
 * NOTE: This is not secure!!!  Consumers are encouraged to
 *       create their own delegate implementation
 **/
static char *defaultObfuscateImpl(const char *passphrase, uint32_t passLen,
                                  const char *input, uint32_t inputLen,
                                  uint32_t *outputLen)
{
    // create buffer with repeated passphrase chars to match
    // length of the input buffer, then xor the 2 buffers
    // (as noted above, 'simplistic')
    char *munge = mask(passphrase, passLen, input, inputLen);
    *outputLen = inputLen;

    return munge;
}

/**
 * Simplistic obfuscation implementation to use as a default.
 * NOTE: This is not secure!!!  Consumers are encouraged to
 *       create their own delegate implementation
 **/
static char *defaultUnobfuscateImpl(const char *passphrase, uint32_t passLen,
                                    const char *input, uint32_t inputLen,
                                    uint32_t *outputLen)
{
    // create buffer with repeated passphrase chars to match
    // length of the input buffer, then xor the 2 buffers
    // (as noted above, 'simplistic')
    char *munge = mask(passphrase, passLen, input, inputLen);
    *outputLen = inputLen;

    return munge;
}
