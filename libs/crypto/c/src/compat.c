//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
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
