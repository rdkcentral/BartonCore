//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast
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

/*-----------------------------------------------
 * versionTest.c
 *
 * Implement functions that were stubbed from the
 * generated IPC Handler.  Each will be called when
 * IPC requests are made from various clients.
 *
 * Author: jelderton -  6/21/18.
 *-----------------------------------------------*/

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "versionTest.h"
#include <icUtil/stringUtils.h>
#include <icUtil/version.h>
#include <zconf.h>

#define BAD_INPUT  -99
#define LEFT_WINS  -1
#define RIGHT_WINS 1
#define BOTH_WINS  0

static bool validateVersionFile(void)
{
    static const char *goodFilePath = "etc/goodVersion";
    static const char *badFilePath = "etc/badVersion";
    static const char *fakeFilePath = "etc/phantomFile";
    static icVersion emptyVersion;

    icVersionFile *fileContents = createVersionFile();
    icVersionFileParseStatus parseStatus = parseVersionFile(fakeFilePath, fileContents);
    if (parseStatus != VERSION_FILE_PARSE_MISSING_FILE)
    {
        printf("Incorrect return value from parseVersionFile: expected missing file, got %d\n", parseStatus);
        destroyVersionFile(fileContents);
        return false;
    }
    destroyVersionFile(fileContents);

    // Verify good contents
    fileContents = createVersionFile();
    parseStatus = parseVersionFile(goodFilePath, fileContents);
    if (parseStatus != VERSION_FILE_PARSE_SUCCESS)
    {
        printf("Incorrect return value from parseVersionFile: expected success, got %d\n", parseStatus);
        destroyVersionFile(fileContents);
        return false;
    }

    icVersion expectedVersion = {0};
    icVersion expectedLastCompatibleVersion = {0};
    if (parseVersionString("10.04.00.000000", &expectedVersion) == false ||
        parseVersionString("10_00_00_000000_0", &expectedLastCompatibleVersion) == false)
    {
        return false;
    }

    if (fileContents == NULL || compareVersions(fileContents->version, &expectedVersion) != 0 ||
        compareVersions(fileContents->lastCompatibleVersion, &expectedLastCompatibleVersion) != 0)
    {
        printf("Memory file contents do not match good version file contents\n");
        destroyVersionFile(fileContents);
        return false;
    }
    destroyVersionFile(fileContents);

    // And bad contents
    fileContents = createVersionFile();
    parseStatus = parseVersionFile(badFilePath, fileContents);
    if (parseStatus != VERSION_FILE_PARSE_SUCCESS)
    {
        printf("Incorrect return value from parseVersionFile: expected success, got %d\n", parseStatus);
        destroyVersionFile(fileContents);
        return false;
    }

    if (fileContents == NULL || compareVersions(fileContents->version, &expectedVersion) != 0 ||
        compareVersions(fileContents->lastCompatibleVersion, &emptyVersion) != 0)
    {
        printf("Memory file contents do not match bad version file contents\n");
        destroyVersionFile(fileContents);
        return false;
    }
    destroyVersionFile(fileContents);

    return true;
}

/*
 * returns -99 if unable to proceed
 */
static int compareVersionStrings(const char *left, const char *right)
{
    icVersion leftVer;
    icVersion rightVer;

    // parse 'left'
    memset(&leftVer, 0, sizeof(icVersion));
    if (parseVersionString(left, &leftVer) == false)
    {
        // unable to parse
        //
        printf("version: FAILED to parse '%s' as version\n", left);
        return BAD_INPUT;
    }

    // parse 'right'
    memset(&rightVer, 0, sizeof(icVersion));
    if (parseVersionString(right, &rightVer) == false)
    {
        // unable to parse
        //
        printf("version: FAILED to parse '%s' as version\n", right);
        return BAD_INPUT;
    }

    // do the compare
    return compareVersions(&leftVer, &rightVer);
}

bool runVersionTests()
{
    // compare versions:   1_2_3_4_1234
    //                     1_2
    if (compareVersionStrings("1_2_3_4_1234", "1_2") != LEFT_WINS)
    {
        printf("version: FAILED to compare mismatch\n");
        return false;
    }

    // compare versions:   1_3 & 1_3
    //
    if (compareVersionStrings("1_3", "1_3") != BOTH_WINS)
    {
        printf("version: FAILED to compare equality\n");
        return false;
    }

    // Install snapshot over a release build:   1_2_3_4_SNAPSHOT
    //                                          1_2_3_4_5000
    if (compareVersionStrings("1_2_3_4_SNAPSHOT", "1_2_3_4_5000") != LEFT_WINS)
    {
        printf("version: FAILED to compare snapshot\n");
        return false;
    }

    // realistic test.  install prod version on-top of a SNAPSHOT build
    //
    if (compareVersionStrings("9_9_0_0_1", "9_9_0_0_SNAPSHOT") != LEFT_WINS)
    {
        printf("version: FAILED to compare snapshot #2\n");
        return false;
    }

    // XHFW-2875: make sure we do not get an octal number
    //
    if (compareVersionStrings("10_08_0_0_0", "10_0_0_0_0") != LEFT_WINS)
    {
        printf("version: FAILED to compare Octal\n");
        return false;
    }

    return validateVersionFile();
}
