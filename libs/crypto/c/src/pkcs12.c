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

#include "icCrypto/pkcs12.h"
#include "cryptoPrivate.h"
#include "util.h"
#include <errno.h>
#include <icCrypto/x509.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <stdbool.h>
#include <string.h>

#define LOG_TAG "crypto/pkcs12"

extern inline void p12StoreDestroy__auto(P12Store **store);

struct P12Store
{
    PKCS12 *store;
    EVP_PKEY *key;
    X509 *cert;
    STACK_OF(X509) * ca;
};

void p12StoreDestroy(P12Store *store)
{
    if (store == NULL)
    {
        return;
    }

    PKCS12_free(store->store);
    EVP_PKEY_free(store->key);
    X509_free(store->cert);
    sk_X509_pop_free(store->ca, X509_free);
    free(store);
}

P12Store *p12StoreLoad(const char *path, const char *passphrase)
{
    if (stringIsEmpty(path) == true)
    {
        icLogWarn(LOG_TAG, "'path' parameter must not be empty");
        return NULL;
    }

    P12Store *store = calloc(1, sizeof(P12Store));

    {
        scoped_FILE *p12File = fopen(path, "r");
        if (p12File != NULL)
        {
            store->store = d2i_PKCS12_fp(p12File, NULL);
        }
        else
        {
            scoped_generic char *errstr = strerrorSafe(errno);
            icLogWarn(LOG_TAG, "Unable to open file at %s: %s", path, errstr);
        }
    }

    if (store->store == NULL)
    {
        icLogError(LOG_TAG, "Unable to decode p12 file '%s'", path);
    }

    if (store->store == NULL || PKCS12_parse(store->store, passphrase, &store->key, &store->cert, &store->ca) == false)
    {
        if (store->store != NULL)
        {
            char err[150];
            ERR_error_string(ERR_get_error(), err);
            icLogError(LOG_TAG, "Unable to parse p12 file '%s' error: %s", path, err);
        }

        p12StoreDestroy(store);
        store = NULL;
    }

    return store;
}

X509Cert *p12StoreGetCert(const P12Store *store)
{
    if (store == NULL || store->cert == NULL)
    {
        return NULL;
    }

    X509Cert *cert = malloc(sizeof(X509Cert));
    cert->cert = X509_dup(store->cert);

    return cert;
}

char *p12StoreGetPEMKey(const P12Store *store)
{
    if (store == NULL || store->key == NULL)
    {
        return NULL;
    }

    char *pemKey = NULL;

    BIO *mem = BIO_new(BIO_s_mem());

    if (PEM_write_bio_PrivateKey(mem, store->key, NULL, NULL, 0, NULL, NULL) == true)
    {
        pemKey = getMemBIOString(mem);
    }
    else
    {
        icLogWarn(LOG_TAG, "Unable to write private key to PEM");
    }

    BIO_free(mem);

    return pemKey;
}

char *p12StoreGetPEMCACerts(const P12Store *store, bool includeRoot)
{
    if (store == NULL || store->ca == NULL || sk_X509_num(store->ca) == 0)
    {
        return NULL;
    }

    BIO *mem = BIO_new(BIO_s_mem());

    char *pemCert = NULL;
    bool writeOk = true;

    /* Print other certs in reverse order: chains should be in leaf-[intermediate]*-root order */
    for (int i = sk_X509_num(store->ca) - 1; i >= 0; i--)
    {
        X509 *cert = sk_X509_value(store->ca, i);
        if (includeRoot == false && X509_NAME_cmp(X509_get_issuer_name(cert), X509_get_subject_name(cert)) == 0)
        {
            /* This certificate is self-signed (it is a root) */
            icLogDebug(LOG_TAG, "Skipping root cert at %d", i);
            continue;
        }

        if (PEM_write_bio_X509(mem, cert) == false)
        {
            icLogError(LOG_TAG, "Unable to write PEM data for certificate #%d", i + 1);
            writeOk = false;
            break;
        }
    }

    if (writeOk == true)
    {
        pemCert = getMemBIOString(mem);
    }

    BIO_free(mem);

    return pemCert;
}
