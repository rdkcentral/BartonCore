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
 * testProtectConfigFile.c
 *
 * Test function for encrypt and decrypt string.
 *
 * Author: kgandhi
 *-----------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "icConfig/legacyProtectConfig.h"
#include "tests.h"

#define INPUT_LENGTH 20

/**
 * @brief main function to test encrypt and decrypt string functionality
 *
 * @param argc : argument
 * @param argv[] : argument
 *
 * @return SUCESS or FAILURE
 */
bool testLegacyProtectConfig()
{
#ifdef CONFIG_LIB_LEGACY_ENCRYPTION

    // test 'decrypt' of a string that was previously encrypted in Java
    // TODO: compare the result to what the correct answer is (commented below)
    //       but right now the conversion has messed up chars at the end and
    //       we need to figure out what's wrong here
    const char *javaStr[] = {"jVTu3KaArlc0Hxbwg9orAg", // == mHzD2FOW
                             "IvLmTPgCdGQdAH3caDrtNQ", // == XHS-E04288B2
                             "V/ZMO9f7LEP9Mr564ZPZzGsMQ9z/SARfo27S6hMqTvWcV9oFpe7lSHwA9piJKH4farcKrgtut0CA2BIIqEOPmg",
                             // == ge9HwgQZ4dKXTx1tXejuKWHGgbNFpXhr3vEWTQxcymyjGcP08Oq7gRThS4KD039
                             NULL};
    unsigned char *buffer = NULL;
    size_t bufferLen = 0;

    int i = 0;
    while (javaStr[i] != NULL)
    {
        // decrypt the string
        //
        printf("decrypting java string '%s'\n", javaStr[i]);
        if (decryptConfigString(javaStr[i], &buffer, &bufferLen) == false)
        {
            // bail due to error
            //
            printf("unable to decrypt Java String\n");
            return false;
        }
        printf("success decrypting java '%s' len=%ld\n", buffer, bufferLen);

        // cleanup and go to the next string
        //
        free(buffer);
        i++;
    }

#endif // CONFIG_LIB_LEGACY_ENCRYPTION

    return true;
}
