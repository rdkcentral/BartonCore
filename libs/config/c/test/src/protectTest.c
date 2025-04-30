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

bool testProtectConfig()
{
    const char *inputMsg = INPUT_STRING;
    char *ob = NULL;
    char *bob = NULL;
    bool worked = false;
    uint32_t obLen = 0;

    char badCT[2] = {'\30', '\31'};
    pcData badInput = {.data = badCT, .length = 1};
    pcData *testPT = NULL;
    pcData *badEnc = NULL;
    pcData badPass = {.data = "", .length = 1};
    pcData *encr = NULL;

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

    /* All tests passed! */
    worked = true;

cleanup:
    destroyProtectConfigData(pass, true);
    destroyProtectConfigData(testPT, true);
    destroyProtectConfigData(encr, true);
    destroyProtectConfigData(decr, true);
    closeProtectConfigSession();
    return worked;
}
