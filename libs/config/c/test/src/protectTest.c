//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2016 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * configTest.c
 *
 * test utility for libicConfig.  also serves as an
 * example for protecting portions of the file.
 *
 * Author: jelderton -  8/30/16.
 * FIXME: upgrade to cmocka testcase
 *-----------------------------------------------*/

#include "tests.h"
#include <icConfig/obfuscation.h>
#include <icConfig/protectedConfig.h>
#include <icUtil/base64.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_STRING "this is my test of crud to encrypt"
#define DECRYPT_UPG_SIG                                                                                                \
    "FgAAADMAAAATS5m5bm9pVHk5Bi8dTBcL7Up3FRx9ZCpFesEVFVYJCYJOQXpKJEcWHUVsMhPINXpHURFV2G03DZ1lG200JANAfjE0QYFoB3I6dB2M" \
    "cVJAORx7CGM3AGkQXwVOGu5tUxgXCPZse2FXNqhge0BANXGdfnk5L3pGN1ggNmtBEj/"                                              \
    "kLKEzGjMNaXFbX14UIKYjdAwBLjO1SWpNaRo5Iy9wdFswKW1jLlhGCzAeH0gqHj0uIGRhFC5FWRlX"
#define DECRYPT_UPG_CT "SaNYwxz7EGvwneOMSFXS8g=="
#define DECRYPT_UPG_PT "4321"

bool testProtectConfig()
{
    const char *inputMsg = INPUT_STRING;
    char *ob = NULL;
    char *bob = NULL;
    char *upgKey = NULL;
    bool worked = false;
    uint32_t obLen = 0;

    char badCT[2] = {'\30', '\31'};
    pcData badInput = {.data = badCT, .length = 1};
    pcData *testPT = NULL;
    pcData *badEnc = NULL;
    pcData badPass = {.data = "", .length = 1};
    pcData *encr = NULL;

    pcData upgPass = {0, 0};
    pcData upgInput = {.data = DECRYPT_UPG_CT, .length = (uint32_t) strlen(DECRYPT_UPG_CT)};
    pcData bad = {0, 0};
    pcData *decr = NULL;

    printf("encoding '%s'\n", inputMsg);

    // make a random key
    //
    if (!openProtectConfigSession())
    {
        puts("Unable to start protected config session!");
        return false;
    }
    pcData *pass = generateProtectPassword();
    if (pass == NULL)
    {
        printf("error generating 'key'\n");
        goto cleanup;
    }

    // obfuscate the random key (as an example)
    // then base64 encode the obfuscated key so it
    // could be saved into a config file
    //
    obLen = 0;
    ob = obfuscate("123456", 6, (const char *) pass->data, pass->length, &obLen);
    bob = icEncodeBase64((uint8_t *) ob, obLen);
    printf("obfuscated key='%s' len=%d (passlen=%d)\n", bob, obLen, pass->length);
    free(ob);
    free(bob);

    // encrypt something using this key
    //
    pcData input;
    input.data = (unsigned char *) inputMsg;
    input.length = (uint32_t) strlen(inputMsg);
    printf("input string='%s' len=%d\n", input.data, input.length);

    badEnc = protectConfigData(&input, &badPass);
    if (badEnc != NULL)
    {
        puts("protectConfigData accpeted an invalid key");
        destroyProtectConfigData(badEnc, true);
        goto cleanup;
    }

    // coverity[string_null]
    // badCT is intentionally not NULL terminated to test validation
    badEnc = unprotectConfigData(&badInput, pass);
    if (badEnc != NULL)
    {
        puts("protectConfigData accpeted invalid ciphertext");
        destroyProtectConfigData(badEnc, true);
        goto cleanup;
    }

    encr = protectConfigData(&input, pass);
    if (encr == NULL)
    {
        printf("error encrpypting\n");
        goto cleanup;
    }
    printf("encrypted string='%s' len=%d\n", encr->data, encr->length);

    // decrypt what we just encrypted
    //
    decr = unprotectConfigData(encr, pass);
    if (decr == NULL)
    {
        printf("error decrpypting\n");
        worked = false;
        goto cleanup;
    }
    printf("decrypted string='%s' len=%d\n", decr->data, decr->length);

    // compare to ensure we got the same value
    //
    if (strcmp((const char *) decr->data, (const char *) input.data) != 0)
    {
        printf("FAILED!  decrypt string is different then input\n");
        worked = false;
        goto cleanup;
    }

    bad.data = strdup(encr->data);
    bad.length = encr->length;
    bad.data[strlen(bad.data) - 1] = '*';
    destroyProtectConfigData(encr, true);
    encr = NULL;
    destroyProtectConfigData(decr, true);

    decr = unprotectConfigData(&bad, pass);
    if (decr != NULL)
    {
        puts("Invalid data accepted by decrypt");
        free(bad.data);
        goto cleanup;
    }
    free(bad.data);
    worked = false;

    /* Test that a simulated downgrade/noop is ignored */
    if (forceProtectVersion(PROTECT_AES_CBC_NO_IV, false))
    {
        puts("Nonforced downgrade was not ignored!");
        goto cleanup;
    }

    /*
     * Test that a simulated firmware upgrade successfully reads an old ciphertext and
     * writes the same ciphertext instead of a new style value
     */
    forceProtectVersion(PROTECT_AES_CBC_NO_IV, true);

    uint16_t len;
    uint32_t klen;
    uint8_t *tmp;

    if (!icDecodeBase64(DECRYPT_UPG_SIG, &tmp, &len))
    {
        puts("Unable to decode base64 fixture!");
        goto cleanup;
    }

    upgKey = unobfuscate("security", 8, tmp, len, &klen);
    free(tmp);

    upgPass.data = upgKey;
    upgPass.length = klen;
    testPT = unprotectConfigData(&upgInput, &upgPass);
    if (!testPT || !testPT->data)
    {
        puts("Decrypting test data failed!");
        goto cleanup;
    }

    if (strcmp(testPT->data, DECRYPT_UPG_PT) != 0)
    {
        printf("Expected " DECRYPT_UPG_PT " but got %s\n", testPT->data);
        goto cleanup;
    }

    encr = protectConfigData(testPT, &upgPass);
    if (encr == NULL)
    {
        puts("encrypt failed!");
        goto cleanup;
    }

    if (strcmp(encr->data, DECRYPT_UPG_CT) != 0)
    {
        puts("Re-encrypt did not use old format!");
        goto cleanup;
    }
    destroyProtectConfigData(encr, true);
    encr = NULL;

    /* Test that invalid upgrades are ignored */
    if (forceProtectVersion(UINT8_MAX, false))
    {
        puts("Upgrade to undefined encryption version ID accepted");
        goto cleanup;
    }

    /* Test that a forced upgrade works */
    if (!forceProtectVersion(PROTECT_ID_LATEST, false))
    {
        puts("Upgrade to latest version ID failed");
        goto cleanup;
    }

    encr = protectConfigData(testPT, &upgPass);
    if (encr == NULL || strcmp(DECRYPT_UPG_CT, encr->data) == 0)
    {
        puts("Encryption with latest verison ID failed");
        goto cleanup;
    }

    decr = unprotectConfigData(encr, &upgPass);
    if (strcmp(decr->data, DECRYPT_UPG_PT) != 0 || decr->version != PROTECT_ID_LATEST)
    {
        printf("Decrypting value with latest version ID failed!\n"
               "Expected: '%s' ID: %d, received: '%s' ID: %d\n",
               DECRYPT_UPG_PT,
               PROTECT_ID_LATEST,
               decr->data,
               decr->version);
        goto cleanup;
    }
    destroyProtectConfigData(decr, true);
    destroyProtectConfigData(encr, true);
    encr = NULL;
    decr = NULL;

    /* Test that a noop upgrade is reported as success */
    if (!forceProtectVersion(PROTECT_ID_LATEST, false))
    {
        puts("Noop upgrade did not report success");
        goto cleanup;
    }

    /* All tests passed! */
    worked = true;

cleanup:
    free(upgKey);
    destroyProtectConfigData(pass, true);
    destroyProtectConfigData(testPT, true);
    destroyProtectConfigData(encr, true);
    destroyProtectConfigData(decr, true);
    closeProtectConfigSession();
    return worked;
}