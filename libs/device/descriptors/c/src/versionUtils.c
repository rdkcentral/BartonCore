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
 * versionUtils.c
 *
 * set of helper functions for parsing versions that
 * are specific to device descriptors
 *
 * Author: jelderton -  6/16/16.
 *-----------------------------------------------*/

#include "versionUtils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_ARRAY_LEN 16
#define DEFAULT_BUFF_LEN  32

/*
 * parse a version string, breaking it into an
 * array of integers.  each non-digit character
 * will be treated as a separator.
 * for example:
 *    1.0.3.R13 --> 1, 0, 3, 13 : arrayLen=4
 * caller must free the returned array (even if arrayLen == 0)
 */
uint32_t *versionStringToInt(const char *versionStr, uint16_t *arrayLen)
{
    size_t inputLen = strlen(versionStr);
    // Check for zigbee versions, which are hex unsigned 32 bit numbers
    if (inputLen == 10 && versionStr[0] == '0' && (versionStr[1] == 'x' || versionStr[1] == 'X'))
    {
        // Easy case, just a single uint32 in the array
        uint32_t *retVal = (uint32_t *) malloc(sizeof(uint32_t));
        if (versionStr[1] == 'x')
        {
            sscanf(versionStr, "%x", retVal);
        }
        else
        {
            sscanf(versionStr, "%X", retVal);
        }

        *arrayLen = 1;
        return retVal;
    }

    // start out with a decent-sized array, and set to all 0s
    //
    uint32_t *retVal = (uint32_t *) malloc(sizeof(uint32_t) * DEFAULT_ARRAY_LEN);
    memset(retVal, 0, sizeof(uint32_t) * DEFAULT_ARRAY_LEN);

    // get bounds and a buffer to append characters
    //
    char buffer[DEFAULT_BUFF_LEN];
    int bufferLen = 0;

    // loop through the string, looking at each char
    //
    int i = 0;
    *arrayLen = 0;
    while (i < inputLen && *arrayLen < DEFAULT_ARRAY_LEN)
    {
        // examine the character at 'i'
        //
        if (isdigit(versionStr[i]) != 0)
        {
            // append to buffer, keeping it NULL terminated
            //
            buffer[bufferLen] = versionStr[i];
            buffer[bufferLen + 1] = '\0';
            bufferLen++;
        }
        else
        {
            // if buffer has digits, convert to int
            // and append to the return array
            //
            if (bufferLen > 0)
            {
                // save in array
                //
                retVal[*arrayLen] = (uint32_t) atoi(buffer);
                *arrayLen += 1;
                bufferLen = 0;
            }
        }

        // go to next char
        //
        i++;
    }

    // out of loop, see if anything remaining in the buffer
    //
    if (bufferLen > 0)
    {
        // save in array
        //
        retVal[*arrayLen] = (uint32_t) atoi(buffer);
        *arrayLen += 1;
    }

    return retVal;
}

/*
 * compare two version arrays (assumed created via parseVersionString)
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal
 * positive-num - if 'right' is greater.
 */
int8_t compareVersionArrays(uint32_t *left, uint16_t leftLen, uint32_t *right, uint16_t rightLen)
{
    // compare each item - up-to leftLen
    // this allows the right to be longer, potentially with trailing 0s
    //
    int i;
    for (i = 0; i < leftLen; i++)
    {
        if (i >= rightLen)
        {
            // cannot compare to the right.  unless left value is 0, we're done
            //
            if (left[i] != 0)
            {
                // has to be something like:
                //   1.1 vs 1
                //
                return -1;
            }
            continue;
        }

        if (left[i] > right[i])
        {
            return -1;
        }
        else if (left[i] < right[i])
        {
            return 1;
        }
    }

    // equal so far, see if the right still has items to compare
    //
    if (i < rightLen)
    {
        // each must be '0' or else right is larger
        //
        while (i < rightLen)
        {
            if (right[i] != 0)
            {
                return 1;
            }
            i++;
        }
    }

    // must be equal
    //
    return 0;
}

/*
 * compare two version strings
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal
 * positive-num - if 'right' is greater.
 */
int8_t compareVersionStrings(const char *leftVersion, const char *rightVersion)
{
    uint16_t leftLen, rightLen;
    uint32_t *arrayL = NULL;
    uint32_t *arrayR = NULL;

    arrayL = versionStringToInt(leftVersion, &leftLen);
    arrayR = versionStringToInt(rightVersion, &rightLen);
    int8_t rc = compareVersionArrays(arrayL, leftLen, arrayR, rightLen);
    free(arrayL);
    free(arrayR);
    return rc;
}
