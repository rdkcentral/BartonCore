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
 * versionUtils.h
 *
 * set of helper functions for parsing versions that
 * are specific to device descriptors
 *
 * Author: jelderton -  6/16/16.
 *-----------------------------------------------*/

#ifndef FLEXCORE_VERSIONUTILS_H
#define FLEXCORE_VERSIONUTILS_H

#include <stdint.h>

/*
 * parse a version string, breaking it into an
 * array of integers.  each non-digit character
 * will be treated as a separator.
 * for example:
 *    1.0.3.R13 --> 1, 0, 3, 13 : arrayLen=4
 * caller must free the returned array (even if arrayLen == 0)
 */
uint32_t *versionStringToInt(const char *versionStr, uint16_t *arrayLen);

/*
 * compare two version arrays (assumed created via versionStringToInt)
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal
 * positive-num - if 'right' is greater.
 */
int8_t compareVersionArrays(uint32_t *left, uint16_t leftLen, uint32_t *right, uint16_t rightLen);

/*
 * compare two version strings
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal
 * positive-num - if 'right' is greater.
 */
int8_t compareVersionStrings(const char *leftVersion, const char *rightVersion);

#endif // FLEXCORE_VERSIONUTILS_H
