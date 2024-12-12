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
 * base64.c
 *
 * Helper functions for performing Base64
 * encode/decode operations.
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icUtil/stringUtils.h>

#include <mbedtls/base64.h>

#include <icUtil/base64.h>

/**
 * Encode a binary array into a Base-64 string.  If the return
 * is not-NULL, then the caller should free the memory.
 *
 * @return encoded string otherwise NULL
 */
char *icEncodeBase64(uint8_t *array, uint16_t arrayLen)
{
    if (array == NULL || arrayLen == 0)
    {
        return NULL;
    }

    size_t olen = 0;
    /*
     * Base64 has 4-byte output blocks, so output length must be a multiple of 4. Adding 3 and clearing the last
     * two bits effectively rounds up to the next block size (only a multiple of 4 bit can possibly be set).
     * Add one byte for NUL.
     */
    const size_t dlen = (((arrayLen * 4 / 3) + 3u) & ~3u) + 1;
    uint8_t *dst = calloc(dlen, 1);

    if (mbedtls_base64_encode(dst, dlen, &olen, array, arrayLen))
    {
        free(dst);
        return NULL;
    }
    else
    {
        return dst;
    }
}

/**
 * Decode the base64 encoded string 'src'.  If successful,
 * this will return true and allocate/populate the 'array' and 'arrayLen'.
 * in that case, the caller must free the array.
 */
bool icDecodeBase64(const char *srcStr, uint8_t **array, uint16_t *arrayLen)
{
    if (srcStr == NULL)
    {
        *arrayLen = 0;
        *array = NULL;
        return false;
    }

    size_t olen = 0;
    const size_t dlen = strlen(srcStr) * 3 / 4;
    *array = calloc(dlen, 1);
    *arrayLen = 0;

    if (mbedtls_base64_decode(*array, dlen, &olen, srcStr, strlen(srcStr)))
    {
        free(*array);
        *array = NULL;
        return false;
    }

    *arrayLen = (uint16_t) olen;
    return true;
}

/**
 * Decode the base64 encoded url string 'src'.  If successful,
 * this will return true and allocate/populate the 'array' and 'arrayLen'.
 * in that case, the caller must free the array.
 */
bool icDecodeUrlBase64(const char *srcStr, uint8_t **array, uint16_t *arrayLen)
{
    bool retVal = false;

    if (srcStr == NULL)
    {
        *arrayLen = 0;
        *array = NULL;
        return retVal;
    }

    // url base decoding uses different chars than traditional
    // need to get the source string ready for normal decoding

    char *strConverted = NULL;
    char *strToReplace = strdup(srcStr);

    // replace '-' with '+'
    char *replaced1 = stringReplace(strToReplace, "-", "+");
    if (stringIsEmpty(replaced1) == false)
    {
        free(strToReplace);
        strToReplace = replaced1;
        replaced1 = NULL;
    }
    free(replaced1);

    // replace '_' with '/'
    char *replaced2 = stringReplace(strToReplace, "_", "/");
    if (stringIsEmpty(replaced2) == false)
    {
        free(strToReplace);
        strToReplace = replaced2;
        replaced2 = NULL;
    }
    free(replaced2);

    // pad with trailing '='(s)
    size_t len = strlen(strToReplace);
    switch (len % 4)
    {
        case 0:
            strConverted = strToReplace;
            strToReplace = NULL;
            break;
        case 2:
            strConverted = stringBuilder("%s==", strToReplace);
            break;
        case 3:
            strConverted = stringBuilder("%s=", strToReplace);
            break;
        default:
            // do nothing
            break;
    }
    free(strToReplace);

    // pass to standard base64 decoder
    retVal = icDecodeBase64(strConverted, array, arrayLen);
    free(strConverted);

    return retVal;
}
