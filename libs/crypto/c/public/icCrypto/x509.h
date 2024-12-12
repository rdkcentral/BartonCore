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

#ifndef ZILKER_X509_H
#define ZILKER_X509_H

#include <icTypes/icLinkedList.h>
#include <stdlib.h>
#include <sys/socket.h>

typedef struct X509Cert X509Cert;

typedef struct X509DirName
{
    /**
     * Country
     */
    const char *const country;

    /**
     * Organization name
     */
    const char *const org;

    /**
     * Organizational unit name
     */
    const char *const orgUnit;

    /**
     * Distinguished name qualifier (i.e., namespace)
     * @ref https://tools.ietf.org/html/rfc4519#section-2.8
     */
    const char *const dnQualifier;

    /**
     * State, province, prefecture, or other major geopolitical subdivision
     */
    const char *const state;

    /**
     * The common name (e.g., your host name)
     */
    const char *const commonName;

    /**
     * The locality name (e.g., your city)
     */
    const char *const locality;

    /**
     * The subject/issuer serial number (device SN).
     * Not to be confused with the certificate serial number.
     * @ref https://tools.ietf.org/html/rfc4519#section-2.31
     */
    const char *const serialNumber;

    /**
     *
     * @ref https://tools.ietf.org/html/rfc4519#section-2.4
     */
    const char *const domainComponent;

    /**
     * Optional user identifier for this certificate.
     * @ref https://tools.ietf.org/html/rfc1274#section-9.3.1
     * @ref https://tools.ietf.org/html/rfc4519#section-2.39
     *
     * Not to be confused with RFC5820 (X509) uniqueId.
     */
    const char *const userId;

    /*
     * Some SHOULD support attributes not (yet) supported here; they are not common for
     * machine-issued m2m certs.
     * (e.g., personal nouns like surname, generation, etc.)
     * @ref: https://tools.ietf.org/html/rfc5280#section-4.1.2.4
     */
} X509DirName;

typedef enum X509GeneralNameType
{
    CRYPTO_X509_NAME_INVALID = -1,

    /**
     * The X509 name is a structured directoryName.
     * Structured data is available in in X509DirName.dirName
     * and the RFC2253 formatted name is availble in X509DirName.printableName.
     */
    CRYPTO_X509_NAME_DIRNAME,

    /**
     * The X509 name is an email (rfc822) address.
     */
    CRYPTO_X509_NAME_EMAIL,

    /**
     * The X509 name is a DNS fully qualified domain name
     */
    CRYPTO_X509_NAME_DNS,

    /**
     * The X509 name is a URI
     */
    CRYPTO_X509_NAME_URI,

    /* CRYPTO_X509_NAME_IPADDR, */
    /* CRYPTO_X509_NAME_OTHERNAME, */
    CRYPTO_X509_NAME_UNSUPPORTED
} X509GeneralNameType;

typedef struct X509GeneralName
{
    const X509GeneralNameType type;
    union
    {
        const X509DirName *const dirName;
    };
    const char *const printableName;
} X509GeneralName;

/**
 * Get the human readable certificate validity period start date
 * @param x509
 * @return a heap allocated certificate (free when done)
 * @see x509CertDestroy to release memory
 */
char *x509GetValidNotBefore(const X509Cert *x509);

/**
 * Get the human readable certificate validity period end date
 * @param x509
 * @return a heap allocated string (free when done)
 */
char *x509GetValidNotAfter(const X509Cert *x509);

/**
 * Get the subject common name
 * @param x509
 * @return a heap allocated string (free when done)
 */
char *x509GetSubjectCN(const X509Cert *x509);

/**
 * Get the subject distinguished name in RFC2253 (LDAP) format
 * @param x509
 * @return a heap allocated string (free when done)
 */
char *x509GetSubjectName(const X509Cert *x509);

/**
 * Get the issuer distinguished name in RFC2253 (LDAP) format
 * @param x509
 * @return a heap allocated string (free when done)
 */
char *x509GetIssuerName(const X509Cert *x509);

/**
 * Convert an X509 certificate object to a PEM encoded X509 certificate
 * @param x509
 * @return a heap allocated string (free when done)
 */
char *x509GetPEM(const X509Cert *x509);

/**
 * Load a PEM encoded x509 certificate from a file
 * @param path the path to the file
 * @return a heap allocated X509Cert (free when done)
 */
X509Cert *x509CertLoadPEM(const char *path);

/**
 * Release an X509 certificate object
 * @param cert
 */
void x509CertDestroy(X509Cert *cert);

/**
 * Scope-bound resource management helper to release an X509 object
 * @param cert
 * @see AUTO_CLEAN
 */
inline void x509CertDestroy__auto(X509Cert **cert)
{
    x509CertDestroy(*cert);
}

#define scoped_X509Cert AUTO_CLEAN(x509CertDestroy__auto) X509Cert

/**
 * Release an X509GeneralName object
 * @param name
 */
void x509GeneralNameDestroy(X509GeneralName *name);

/**
 * Scope-bound resource management helper to release an X509GeneralName object
 * @param name
 * @see AUTO_CLEAN
 */
inline void x509GeneralNameDestroy__auto(X509GeneralName **name)
{
    x509GeneralNameDestroy(*name);
}

#define scoped_X509GeneralName AUTO_CLEAN(x509GeneralNameDestroy__auto) X509GeneralName

/**
 * Get the structured certificate subject (as a directoryName).
 * @param cert
 * @return A heap allocated X509GeneralName containing a dirName.
 *         Caller must free with x509GeneralNameDestroy
 */
X509GeneralName *x509GetSubject(const X509Cert *cert);

/**
 * Get the structured certificate issuer (as a directoryName).
 * @param cert
 * @return A heap allocated X509GeneralName containing a dirName.
 *         Caller must free with x509GeneralNameDestroy
 */
X509GeneralName *x509GetIssuer(const X509Cert *cert);

/**
 * Get all x509v3 subject alternative names
 * @param cert
 * @return A list containing all X509GeneralName SANs (may be NULL).
 *         Caller must free contents with x509GeneralNameDestroy.
 * @note unsupported name types will not be included
 */
icLinkedList *x509GetSubjectAltNames(const X509Cert *cert);

/**
 * Format a list of X509GeneralName values.
 * @param names
 * @return a heap allocated string describing the list (caller must free when finished).
 *
 */
char *x509GeneralNamesFormat(const icLinkedList *names);

/**
 * Format a single X509GeneralName value
 * @return a printable string (caller must free when finished)
 */
char *x509GeneralNameFormat(const X509GeneralName *name);

/**
 * Convert the X509GeneralName type to string
 * @param type
 * @return a string constant describing type
 */
const char *x509GeneralNameTypeToString(const X509GeneralNameType type);

#endif // ZILKER_X509_H
