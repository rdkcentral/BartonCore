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

#ifndef ZILKER_PKCS12_H
#define ZILKER_PKCS12_H

#include "x509.h"
#include <icTypes/icLinkedList.h>
#include <stdbool.h>

typedef struct P12Store P12Store;

/**
 * Load a PKCS#12 store from a file.
 * @return a heap allocated P12 store (free when done)
 * @see p12StoreDestroy to release memory
 */
P12Store *p12StoreLoad(const char *path, const char *passphrase);


/**
 * Get the client certificate
 * @param store
 * @return a heap allocated certificate object (free when done)
 * @seee X509CertDestroy to release memory
 */
X509Cert *p12StoreGetCert(const P12Store *store);

/**
 * Get the private key in PEM format
 * @param store
 * @return a heap allocated string (free when done)
 */
char *p12StoreGetPEMKey(const P12Store *store);

/**
 * Get the other certificates (typically CA chain) in PEM format
 * @param store
 * @param includeRoot Set to true to include the self-signed root CA certificate.
 *        Otherwise, only the intermediates are included in the bundle.
 * @note some HTTP clients, e.g., cURL, should be given the full chain in the client certificate configuration.
 *       For those clients, prepend the client certificate to this collection.
 * @return a heap allocated string (free when done)
 */
char *p12StoreGetPEMCACerts(const P12Store *store, bool includeRoot);

/**
 * Free a P12 keystore object
 * @param store
 */
void p12StoreDestroy(P12Store *store);

/**
 * Scope-bound resource management helper for P12Store
 * @param store
 * @see AUTO_CLEAN
 */
inline void p12StoreDestroy__auto(P12Store **store)
{
    p12StoreDestroy(*store);
}

#define scoped_P12Store AUTO_CLEAN(p12StoreDestroy__auto) P12Store

#endif // ZILKER_PKCS12_H
