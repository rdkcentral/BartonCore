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

#include "openssl/evp.h"
#include <icCrypto/digest.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <openssl/err.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_TAG               "icCrypto/digest"
#define logDebug(format, ...) icLogDebug(LOG_TAG, "%s: " format, __func__, __VA_ARGS__)
#define logWarn(format, ...)  icLogWarn(LOG_TAG, "%s: " format, __func__, __VA_ARGS__)

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#include <openssl/crypto.h>
#include <string.h>

static inline EVP_MD_CTX *EVP_MD_CTX_new(void)
{
    EVP_MD_CTX *ctx = OPENSSL_malloc(sizeof(EVP_MD_CTX));
    memset(ctx, 0, sizeof(*ctx));

    return ctx;
}

static inline void EVP_MD_CTX_free(EVP_MD_CTX *ctx)
{
    EVP_MD_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
}

typedef struct env_md_st OSSLDigestMethod;
#else
typedef struct evp_md_st OSSLDigestMethod;
#endif

extern inline char *digestFileHex(const char *filename, CryptoDigest algorithm);

uint8_t *digestFile(const char *filename, CryptoDigest algorithm, uint8_t *digestLen)
{
    if (digestLen == NULL)
    {
        icLogWarn(LOG_TAG, "%s: invalid arguments", __func__);
        return NULL;
    }

    *digestLen = 0;

    if (filename == NULL)
    {
        icLogWarn(LOG_TAG, "%s: invalid arguments", __func__);
        return NULL;
    }

    uint8_t *digest = NULL;
    const OSSLDigestMethod *evpAlgo = NULL;

    switch (algorithm)
    {
        case CRYPTO_DIGEST_MD5:
            evpAlgo = EVP_md5();
            break;

        case CRYPTO_DIGEST_SHA1:
            evpAlgo = EVP_sha1();
            break;

        case CRYPTO_DIGEST_SHA256:
            evpAlgo = EVP_sha256();
            break;

        case CRYPTO_DIGEST_SHA512:
            evpAlgo = EVP_sha512();
            break;

        default:
            logWarn("algorithm %d not valid", algorithm);
            return NULL;
    }

    errno = 0;
    scoped_FILE *in = fopen(filename, "rb");

    if (in != NULL)
    {
        char record[1024];
        char sslErr[256];
        size_t reclen = 0;
        unsigned int mdlen = 0;
        bool digestResult;
        EVP_MD_CTX *evp = EVP_MD_CTX_new();

        if (EVP_DigestInit(evp, evpAlgo) == false)
        {
            ERR_error_string_n(ERR_get_error(), sslErr, sizeof(sslErr));
            logWarn("couldn't initialize digest: %s", sslErr);
            EVP_MD_CTX_free(evp);
            return NULL;
        }

        do
        {
            reclen = fread(record, 1, sizeof(record), in);
            digestResult = EVP_DigestUpdate(evp, record, reclen);

            if (digestResult == false)
            {
                ERR_error_string_n(ERR_get_error(), sslErr, sizeof(sslErr));
                logWarn("couldn't update digest: %s", sslErr);
            }
        } while (reclen > 0 && ferror(in) == 0 && feof(in) == 0 && digestResult == true);

        if (ferror(in) == 0 && digestResult == true)
        {
            digest = malloc(EVP_MAX_MD_SIZE);
            if (EVP_DigestFinal(evp, digest, &mdlen) == true)
            {
                *digestLen = mdlen;
            }
            else
            {
                free(digest);
                digest = NULL;
                *digestLen = 0;
                ERR_error_string_n(ERR_get_error(), sslErr, sizeof(sslErr));
                logWarn("couldn't finalize digest: %s", sslErr);
            }
        }
        else if (ferror(in) != 0)
        {
            icLogWarn(LOG_TAG, "%s: could not compute digest: I/O error", __func__);
        }

        EVP_MD_CTX_free(evp);
    }
    else
    {
        scoped_generic char *errStr = strerrorSafe(errno);
        logWarn("Unable to open file at %s: %s", filename, errStr);
    }

    return digest;
}
