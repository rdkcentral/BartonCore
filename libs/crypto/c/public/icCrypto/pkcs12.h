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
