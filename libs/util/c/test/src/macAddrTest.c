//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2017 iControl Networks, Inc.
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
 * macAddrTest.c
 *
 * TODO: define file here...
 *
 * Author: John Elderton  -  1/18/17
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <icUtil/macAddrUtils.h>
#include "macAddrTest.h"

#define MAC_ADDRESS1    "e0:60:66:d:2a:2e"
#define MAC_ADDRESS2    "e0:60:66:0d:2a:2e"
#define MAC_ADDRESS3    "E0:60:66:0d:2A:2e"
#define MAC_ADDRESS4    "a0:60:66:0d:aA:2e"

static void printMacBytes(uint8_t *array)
{
    int i = 0;
    for (i = 0 ; i < 6 ; i++)
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
