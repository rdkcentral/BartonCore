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
#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define OLD_OPENSSL
#endif

#include <icLog/logging.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <pthread.h>

#ifdef OLD_OPENSSL
#include <icConcurrent/threadUtils.h>
static pthread_mutex_t *libcryptoLocks;

static void cryptoLock(int mode, int type, const char *file, int line)
{
    if ((mode & CRYPTO_LOCK) != 0)
    {
        mutexLock(&libcryptoLocks[type]);
    }
    else
    {
        mutexUnlock(&libcryptoLocks[type]);
    }
}

static unsigned long cryptoThreadId(void)
{
    return (unsigned long) pthread_self();
}
#endif /* OLD_OPENSSL */

/* Called automatically when the DSO is loaded */
__attribute__((constructor)) static inline void doInit(void)
{
#ifdef OLD_OPENSSL
    SSL_library_init();
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    ERR_load_CRYPTO_strings();

    libcryptoLocks = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));

    for (int i = 0; i < CRYPTO_num_locks(); i++)
    {
        mutexInitWithType(&libcryptoLocks[i], PTHREAD_MUTEX_ERRORCHECK);
    }

    icLogDebug("crypto/compat", "Setting up %d locks", CRYPTO_num_locks());
    CRYPTO_set_id_callback(cryptoThreadId);
    CRYPTO_set_locking_callback(cryptoLock);
#else
    /* Initializing openSSL also registers its destructors atexit */
    OPENSSL_init_crypto(0, NULL);
    OPENSSL_init_ssl(0, NULL);
#endif /* OLD_OPENSSL */
}
