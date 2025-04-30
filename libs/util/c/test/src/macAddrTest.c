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
 * macAddrTest.c
 *
 * TODO: define file here...
 *
 * Author: John Elderton  -  1/18/17
 *-----------------------------------------------*/

#include "macAddrTest.h"
#include <icUtil/macAddrUtils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAC_ADDRESS1 "e0:60:66:d:2a:2e"
#define MAC_ADDRESS2 "e0:60:66:0d:2a:2e"
#define MAC_ADDRESS3 "E0:60:66:0d:2A:2e"
#define MAC_ADDRESS4 "a0:60:66:0d:aA:2e"

static void printMacBytes(uint8_t *array)
{
    int i = 0;
    for (i = 0; i < 6; i++)
    {
        printf("%02x ", array[i]);
    }
}

static bool convertBytes(const char *macAddrString)
{
    uint8_t destArray[6];
    if (macAddrToBytes(macAddrString, destArray, true) == false)
    {
        return false;
    }

    // print both
    printf("  convert: %s\n", macAddrString);
    printf("       to: ");
    printMacBytes(destArray);
    printf("\n");

    return true;
}

static int compareMacStrings(const char *left, const char *right)
{
    // convert each to bytes
    //
    uint8_t leftArray[6];
    uint8_t rightArray[6];

    if (macAddrToBytes(left, leftArray, true) == false)
    {
        return -2;
    }
    if (macAddrToBytes(right, rightArray, true) == false)
    {
        return -2;
    }

    // do the compare, returning the result
    //
    return compareMacAddrs(leftArray, rightArray);
}

/*
 * called by unitTest.c
 */
bool runMacAddrTests()
{
    if (convertBytes(MAC_ADDRESS1) == false)
    {
        printf("macAddr convert test FAILED\n");
        return false;
    }
    if (convertBytes(MAC_ADDRESS2) == false)
    {
        printf("macAddr convert test FAILED\n");
        return false;
    }
    if (compareMacStrings(MAC_ADDRESS1, MAC_ADDRESS2) != 0)
    {
        printf("macAddr compare test FAILED\n");
        return false;
    }
    if (compareMacStrings(MAC_ADDRESS1, MAC_ADDRESS3) != 0)
    {
        printf("macAddr compare test FAILED\n");
        return false;
    }
    if (compareMacStrings(MAC_ADDRESS2, MAC_ADDRESS3) != 0)
    {
        printf("macAddr compare test FAILED\n");
        return false;
    }
    if (compareMacStrings(MAC_ADDRESS3, MAC_ADDRESS4) == 0)
    {
        printf("macAddr compare test FAILED\n");
        return false;
    }

    // no errors, good to go
    return true;
}
