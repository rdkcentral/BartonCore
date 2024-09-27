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


#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <cmocka.h>
#include <icCrypto/digest.h>
#include <icCrypto/x509.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>

#ifndef RES_DIR
#error "no RES_DIR defined"
#endif

#define TEST_CERT   RES_DIR "/sanCert.pem"
#define TEST_DIGEST RES_DIR "/hashMe"

static void test_x509SubjectAltNames(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    icLinkedList *names = x509GetSubjectAltNames(cert);

    sbIcLinkedListIterator *it = linkedListIteratorCreate(names);
    bool foundDN = false;
    while (linkedListIteratorHasNext(it))
    {
        X509GeneralName *name = linkedListIteratorGetNext(it);
        if (name->type == CRYPTO_X509_NAME_DIRNAME)
        {
            assert_non_null(name->dirName);
            assert_non_null(name->dirName->org);
            assert_string_equal(name->dirName->org, "Comcast");

            assert_non_null(name->dirName->orgUnit);
            assert_string_equal(name->dirName->orgUnit, "Xfinity Back Office");

            assert_non_null(name->dirName->commonName);
            assert_string_equal(name->dirName->commonName, "142587");
            foundDN = true;
            break;
        }
    }

    assert_true(foundDN);

    linkedListDestroy(names, (linkedListItemFreeFunc) x509GeneralNameDestroy);
}

static void test_x509SubjectName(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    AUTO_CLEAN(x509GeneralNameDestroy__auto)
    X509GeneralName *subjName = x509GetSubject(cert);
    assert_int_equal(subjName->type, CRYPTO_X509_NAME_DIRNAME);

    const X509DirName *subject = subjName->dirName;

    assert_non_null(subject);
    assert_non_null(subject->commonName);
    assert_string_equal(subject->commonName, "nodeAddressTest.xfinityhome.com");

    assert_non_null(subject->org);
    assert_string_equal(subject->org, "Comcast");

    assert_non_null(subject->orgUnit);
    assert_string_equal(subject->orgUnit, "1E9VSCiCbutgExdd9kBAg76BaR2BGfXWMr");

    assert_non_null(subject->country);
    assert_string_equal(subject->country, "US");

    assert_non_null(subject->userId);
    assert_string_equal(subject->userId, "test-system-ID");

    assert_non_null(subject->state);
    assert_string_equal(subject->state, "PA");

    assert_non_null(subject->locality);
    assert_string_equal(subject->locality, "Philadelphia");
}

static void test_x509IssuerName(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    AUTO_CLEAN(x509GeneralNameDestroy__auto)
    X509GeneralName *issName = x509GetIssuer(cert);
    assert_int_equal(issName->type, CRYPTO_X509_NAME_DIRNAME);

    const X509DirName *issuer = issName->dirName;

    assert_non_null(issuer);
    assert_non_null(issuer->commonName);
    assert_string_equal(issuer->commonName, "STAGE ONLY - Xfinity Subscriber Issuing RSA ICA");
}

static void test_x509FormatSANs(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    icLinkedList *sans = x509GetSubjectAltNames(cert);
    assert_non_null(sans);

    AUTO_CLEAN(free_generic__auto) char *formattedSans = x509GeneralNamesFormat(sans);

    linkedListDestroy(sans, (linkedListItemFreeFunc) x509GeneralNameDestroy);

    printf("SANs:\n%s\n", formattedSans);
}

static void test_x509GetPEM(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);
    AUTO_CLEAN(free_generic__auto) char *testCertPEM = readFileContents(TEST_CERT);

    AUTO_CLEAN(free_generic__auto) char *reEncoded = x509GetPEM(cert);
    assert_string_equal(reEncoded, testCertPEM);
}

static void test_sha1File(void **state)
{
    scoped_generic char *sha1 = digestFileHex(TEST_DIGEST, CRYPTO_DIGEST_SHA1);
    assert_string_equal(sha1, "7547221f52c28daf038071b26b4156b12bd61331");
}

static void test_md5File(void **state)
{
    scoped_generic char *sha1 = digestFileHex(TEST_DIGEST, CRYPTO_DIGEST_MD5);
    assert_string_equal(sha1, "3ee584a40f2ee907231df94509289730");
}

static void test_sha256File(void **state)
{
    scoped_generic char *sha1 = digestFileHex(TEST_DIGEST, CRYPTO_DIGEST_SHA256);
    assert_string_equal(sha1, "517857fbe75fca952a3084362b23f7945b2ea9ee7cafaf0936192f3bd532a9ca");
}

static void test_sha512File(void **state)
{
    scoped_generic char *sha1 = digestFileHex(TEST_DIGEST, CRYPTO_DIGEST_SHA512);
    assert_string_equal(sha1,
                        "e67ddd7cc207c66efaf925accc7983b23ba849642b6eaf1ab1aa1e2ee6232"
                        "f3871c0046a83d47b0d19f6235ce71fc4f2e1b1f6460fee50213bc5e4452d1d393b");
}

static void test_digestBogusFile(void **state)
{
    scoped_generic char *sha1 = digestFileHex(RES_DIR "/doesnotexist", CRYPTO_DIGEST_SHA1);
    assert_null(sha1);
}

int main(int argc, char *argv[])
{
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_x509SubjectAltNames),
                                       cmocka_unit_test(test_x509SubjectName),
                                       cmocka_unit_test(test_x509IssuerName),
                                       cmocka_unit_test(test_x509FormatSANs),
                                       cmocka_unit_test(test_x509GetPEM),
                                       cmocka_unit_test(test_sha1File),
                                       cmocka_unit_test(test_md5File),
                                       cmocka_unit_test(test_sha256File),
                                       cmocka_unit_test(test_sha512File),
                                       cmocka_unit_test(test_digestBogusFile)};

    int rc = cmocka_run_group_tests(tests, NULL, NULL);

    return rc;
}
