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

#pragma once

#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <stdint.h>

typedef enum CryptoDigest
{
    CRYPTO_DIGEST_MD5,
    CRYPTO_DIGEST_SHA1,
    CRYPTO_DIGEST_SHA256,
    CRYPTO_DIGEST_SHA512
} CryptoDigest;

/**
 * Calculate a binary digest using a file.
 * @param filename
 * @param algorithm the digest to load
 * @param digestLength (output) the length of the digest, depending on algorithm. 0 on error.
 * @return A heap allocated digest value (binary). Read up to digestLength; NULL on error.
 * @sa stringBin2hex to convert to a hexadecimal string
 */
uint8_t *digestFile(const char *filename, CryptoDigest algorithm, uint8_t *digestLength);

/**
 * Convenience function to digest a file, encoding the digest in hex.
 * @param filename
 * @param algorithm the digest to use
 * @return hex-encoded digest (lowercase, no embellishments, like <algorithm>sum); NULL on error.
 */
inline char *digestFileHex(const char *filename, CryptoDigest algorithm)
{
    uint8_t len;
    scoped_generic uint8_t *digest = digestFile(filename, algorithm, &len);

    return stringBin2hex(digest, len);
}
