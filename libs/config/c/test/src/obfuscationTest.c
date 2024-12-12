//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 6/10/19.
//

#include <icConfig/obfuscation.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool obfuscateTest()
{
    bool retVal = true;

    // obfuscate something easy, then see if we can decode
    //
    const char *pass = "johancameupwiththis";
    const char *input = "this is an obfuscation test with ASCII chars";
    uint32_t passLen = (uint32_t) strlen(pass);
    uint32_t inputLen = (uint32_t) strlen(input);

    // encode into a buffer
    //
    uint32_t buffLen = 0;
    char *buff = obfuscate(pass, passLen, input, inputLen, &buffLen);

    // decode what we just obfuscated
    //
    uint32_t outLen = 0;
    char *out = unobfuscate(pass, passLen, buff, buffLen, &outLen);
    printf("%s: unobfuscated string = '%s'\n", __func__, out);

    if (strcmp(input, out) != 0)
    {
        printf("%s: FAILED to decode properly\n", __func__);
        retVal = false;
    }
    else
    {
        printf("%s: SUCCESS!\n", __func__);
    }

    // cleanup
    //
    free(buff);
    free(out);
    return retVal;
}
