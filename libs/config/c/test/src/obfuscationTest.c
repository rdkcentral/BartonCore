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
