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
 * legacyProtectConfig.c
 *
 * the legacy (Java) configuration decrypt ported to C
 * mainly used for migration from older firmware.
 *
 * Author: kgandhi
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icConfig/legacyProtectConfig.h>
#include <icLog/logging.h>

// allow enable/disable support of legacy encryption
#ifdef CONFIG_LIB_LEGACY_ENCRYPTION

#define LOG_TAG "decode"

#include <icTypes/sbrm.h>
#include <icUtil/base64.h>
#include <icUtil/stringUtils.h>
#include <openssl/des.h>
#include <openssl/md5.h>

#define SALT_SIZE      8
#define DES_BLOCK_SIZE 8
#define PASSWORD_SIZE  26
#define ITERATIONS     20
#define RESULT_SIZE    16

/**
 * @brief Get Derived Key, 21st MD5 of Password & SALT
 *
 * @param md5 : Derived Key
 *
 * @return Derived Key
 */
static unsigned char *getDerivedKey(unsigned char *md5)
{
    // TODO: Obfuscate/configparamgen? e2 encrypt passwd/salt?
    unsigned char salt[SALT_SIZE] = {0xde, 0x33, 0x10, 0x12, 0xde, 0x33, 0x10, 0x12};
    /* Most important key. We must never share this key to MSO or Partner. */
    unsigned char passwd[PASSWORD_SIZE] = {0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6f,
                                           0x70, 0x61, 0x73, 0x64, 0x66, 0x67, 0x68, 0x6a, 0x6b,
                                           0x6c, 0x7a, 0x78, 0x63, 0x76, 0x62, 0x6e, 0x6d};
    unsigned char result[RESULT_SIZE] = {0};
    unsigned char inresult[RESULT_SIZE] = {0};
    MD5_CTX mdContext, mdContextFor;
    int index = 0;

    // Append salt to the password and MD5 Hash it
    MD5_Init(&mdContext);
    MD5_Update(&mdContext, passwd, PASSWORD_SIZE);
    MD5_Update(&mdContext, salt, SALT_SIZE);
    MD5_Final(result, &mdContext);

    // MD5 Hash it, and hash the result, hash the result 20 times to get derived key
    for (index = 1; index < ITERATIONS; index++)
    {
        MD5_Init(&mdContextFor);
        MD5_Update(&mdContextFor, result, RESULT_SIZE);
        MD5_Final(inresult, &mdContextFor);
        memcpy(result, inresult, RESULT_SIZE);
        memset(inresult, 0x00, RESULT_SIZE);
    }

    // copy to the return buffer
    memcpy(md5, result, RESULT_SIZE);
    return md5;
}

/**
 * @brief Decryption Function implemented Based on PBEwithMD5andDES algorithm
 *
 * @param input : Input String
 * @param out : Decrypted Output String
 * @param len : Length
 *
 * @note Messages are PKCS#5 (RFC2898) PBE1 (obsolete) conformant.
 *
 * @return SUCCESS or FAILURE
 */
bool decryptConfigString(const char *input, unsigned char **out, size_t *len)
{
    // sanity check
    //
    if (input == NULL)
    {
        icLogWarn(LOG_TAG, "invalid input to %s", __FUNCTION__);
        return false;
    }

    /* The stored messages strip the padding, fix it so b64 is valid */
    char *correctInput = (char *) input;
    bool freeCorrectInput = false;
    if (stringEndsWith((char *) input, "==", false) == false)
    {
        // append the '==' to the input string
        //
        correctInput = stringBuilder("%s==", input);
        freeCorrectInput = true;
    }

    // first, base64 decode the input string
    //
    uint8_t *outB64 = NULL;
    size_t size = 0;
    uint16_t tmp = 0;
    if (icDecodeBase64(correctInput, &outB64, &tmp) == false)
    {
        icLogWarn(LOG_TAG, "unable to base64 decode input in %s", __FUNCTION__);
        free(outB64);
        if (freeCorrectInput == true)
        {
            free(correctInput);
        }
        return false;
    }
    size = tmp;

    // done with input, free if needed
    if (freeCorrectInput == true)
    {
        free(correctInput);
    }
    correctInput = NULL;

    if (size % DES_BLOCK_SIZE != 0)
    {
        icLogWarn(LOG_TAG, "Input is not a valid PKCS#5 PBES1 message!");
        free(outB64);
        return false;
    }

    // load the key we'll need for the decrypting.  this is an MD5 hash
    // of the salt + private string (copied from the legacy Java code)
    //
    unsigned char md5[MD5_DIGEST_LENGTH] = {0};
    if (getDerivedKey(md5) == NULL)
    {
        icLogWarn(LOG_TAG, "unable to create digest key in %s", __FUNCTION__);
        free(outB64);
        return false;
    }

    // now the fun part...  create the DES "schedule" and "vector"
    // for some reason, we have to set the first 8 bytes to 'parity'
    // or else the digest won't properly decrypt the data
    //
    DES_key_schedule schedule;
    DES_set_odd_parity((DES_cblock *) &md5);
    if (DES_set_key_checked((DES_cblock *) &md5, &schedule) < 0)
    {
        icLogWarn(LOG_TAG, "error validating digest key in %s", __FUNCTION__);
        free(outB64);
        return false;
    }

    // allocate the output memory (add 1 for a NULL char), then
    // perform the DES CBC decryption via openssl using the md5 and schedule
    //
    *out = (unsigned char *) calloc(size + 1, sizeof(unsigned char));
    DES_cbc_encrypt(outB64, *out, size, &schedule, (unsigned char(*)[8]) & md5 + 1, DES_DECRYPT);

    // cleanup
    //
    free(outB64);
    outB64 = NULL;

    bool ok = true;
    unsigned char pad = (*out)[size - 1];
    if (pad == 0 || pad > 8)
    {
        icLogWarn(LOG_TAG, "Message does not have a valid padding string!");
        ok = false;
    }
    else
    {
        unsigned char *padString[pad];
        memset(padString, pad, pad);

        if (memcmp(padString, &(*out)[size - pad], pad) != 0)
        {
            icLogWarn(LOG_TAG, "Message does not have a valid padding string!");
            ok = false;
        }
        else
        {
            *len = size - pad;
            (*out)[*len] = '\0';
        }
    }

    return ok;
}

#endif // CONFIG_LIB_LEGACY_ENCRYPTION
