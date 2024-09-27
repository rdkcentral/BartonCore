//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
 * version.c
 *
 * Model the standard iControl Version.
 * When represented as a string, looks similar to:
 *   7_2_0_0_201505221523
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <icUtil/version.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>
#include <icLog/logging.h>
#include <icUtil/array.h>

#define LOG_TAG    "VERSION"

#define UNDERSCORE_DELIMITER   "_"
#define PERIOD_DELIMITER "."
#define SNAPSHOT_BUILD_NUMBER   -99

// Version file keys
#define VERSION_FILE_KEY_LONG_VERSION "LONG_VERSION"
#define VERSION_FILE_KEY_VERSION "version"
#define VERSION_FILE_KEY_RELEASE_VER "release_ver"
#define VERSION_FILE_KEY_SERVICE_VER "service_ver"
#define VERSION_FILE_KEY_MAINTENANCE_VER "maintenance_ver"
#define VERSION_FILE_KEY_HOT_FIX_VER "hot_fix_ver"
#define VERSION_FILE_KEY_DISPLAY_VERSION "display_version"
#define VERSION_FILE_KEY_SERVER_VERSION "server_version"
#define VERSION_FILE_KEY_LAST_COMPATIBLE_VERSION "lastCompatibleVersion"
#define VERSION_FILE_KEY_SVN_BUILD "svn_build"
#define VERSION_FILE_KEY_SVN_DATE "svn_date"
#define VERSION_FILE_KEY_BUILD_ID "build_id"
#define VERSION_FILE_KEY_BUILD_DATE "build_date"
#define VERSION_FILE_KEY_BUILD_BY "build_by"

// Enum for easy switch-case of keys. Keep in sync with above list
typedef enum {
    VersionFileKeyLongVersion,
    VersionFileKeyVersion,
    VersionFileKeyReleaseVer,
    VersionFileKeyServiceVer,
    VersionFileKeyMaintenanceVer,
    VersionFileKeyHotFixVer,
    VersionFileKeyDisplayVer,
    VersionFileKeyServerVer,
    VersionFileKeyLastCompatibleVersion,
    VersionFileKeySvnBuild,
    VersionFileKeySvnDate,
    VersionFileKeyBuildId,
    VersionFileKeyBuildDate,
    VersionFileKeyBuildBy,
} VersionFileKeyIndex;

typedef struct {
    VersionFileKeyIndex index;
    const char *key;
} VersionFileKeyEntry;

static const VersionFileKeyEntry knownVersionFileKeys[] = {
        {VersionFileKeyLongVersion, VERSION_FILE_KEY_LONG_VERSION},
        {VersionFileKeyVersion, VERSION_FILE_KEY_VERSION},
        {VersionFileKeyReleaseVer, VERSION_FILE_KEY_RELEASE_VER},
        {VersionFileKeyServiceVer, VERSION_FILE_KEY_SERVICE_VER},
        {VersionFileKeyMaintenanceVer, VERSION_FILE_KEY_MAINTENANCE_VER},
        {VersionFileKeyHotFixVer, VERSION_FILE_KEY_HOT_FIX_VER},
        {VersionFileKeyDisplayVer, VERSION_FILE_KEY_DISPLAY_VERSION},
        {VersionFileKeyServerVer, VERSION_FILE_KEY_SERVER_VERSION},
        {VersionFileKeyLastCompatibleVersion, VERSION_FILE_KEY_LAST_COMPATIBLE_VERSION},
        {VersionFileKeySvnBuild, VERSION_FILE_KEY_SVN_BUILD},
        {VersionFileKeySvnDate, VERSION_FILE_KEY_SVN_DATE},
        {VersionFileKeyBuildId, VERSION_FILE_KEY_BUILD_ID},
        {VersionFileKeyBuildDate, VERSION_FILE_KEY_BUILD_DATE},
        {VersionFileKeyBuildBy, VERSION_FILE_KEY_BUILD_BY}
};

static void parseVersionFileLine(icVersionFile *versionFile, const char *line);
static bool parseVersionFileEntry(icVersionFile *versionFile, const char *key, const char *value);
static bool parseVersionStringWithDelimiter(const char *versionStr, icVersion *target, const char *delimiter);

/*
 * Create an icVersionFile type
 */
icVersionFile *createVersionFile(void)
{
    icVersionFile *retVal = calloc(1, sizeof(icVersionFile));
    retVal->version = calloc(1, sizeof(icVersion));
    retVal->lastCompatibleVersion = calloc(1, sizeof(icVersion));

    return retVal;
}

/*
 * Cleanup an icVersionFile
 */
void destroyVersionFile(icVersionFile *versionFile)
{
    if (versionFile != NULL)
    {
        free(versionFile->version);
        free(versionFile->lastCompatibleVersion);
    }

    free(versionFile);
}

/*
 * Try to parse a firmware version file at the specified path into an icVersionFile container.
 * Returns an enum indicating the status of the routine. On success, VERSION_FILE_PARSE_SUCCESS
 * will be returned
 */
icVersionFileParseStatus parseVersionFile(const char *versionFilePath, icVersionFile *output)
{
    /*
     * File structure looks like:
     *  LONG_VERSION: 10.04.00.000000 (Build 200805-zilker)
     *  version: 10.04.00.000000
     *  release_ver: 10
     *  service_ver: 04
     *  maintenance_ver: 00
     *  hot_fix_ver: 000000
     *  display_version: 10_04_00_000000_0200805
     *  server_version: 10_04_00_000000_200805
     *  lastCompatibleVersion: 10_00_00_000000_0
     *  svn_build: 200805
     *  svn_date: 2020-08-05 19:16:31
     *  build_id:
     *  build_date: 2020-08-05 19:16:31
     *  build_by: cleith065
     */

    if (versionFilePath == NULL || output == NULL)
    {
        return VERSION_FILE_PARSE_BAD_ARGS;
    }

    icVersionFileParseStatus retVal = VERSION_FILE_PARSE_GENERAL_FAILURE;

    if (doesNonEmptyFileExist(versionFilePath) == true)
    {
        scoped_generic char *versionFileContents = readFileContentsWithTrim(versionFilePath);
        if (versionFileContents != NULL)
        {
            size_t maxFileLength = strlen(versionFileContents) + 1;

            // Prime a current line and a remainder buffer;
            char curr[maxFileLength];
            memset(curr, 0, maxFileLength);
            char remainder[maxFileLength];
            memset(remainder, 0, maxFileLength);

            // Start with the versionFileContents in remainder
            safeStringCopy(remainder, maxFileLength, versionFileContents);

            // Iterate line by line, putting the next line into curr. This won't process the last line
            while (stringSplitOnToken(remainder, '\n', curr, remainder) == true)
            {
                parseVersionFileLine(output, curr);
            }

            // Now do the final line
            if (safeStringCopy(curr, maxFileLength, remainder) == true)
            {
                parseVersionFileLine(output, curr);
            }

            retVal = VERSION_FILE_PARSE_SUCCESS;
        }
        else
        {
            icLogError(LOG_TAG, "%s: Failed to read contents of file %s", __func__, versionFilePath);
            retVal = VERSION_FILE_PARSE_FAILED_READ;
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: File %s does not exist or is empty", __func__, versionFilePath);
        retVal = VERSION_FILE_PARSE_MISSING_FILE;
    }

    return retVal;
}

/*
 * parse the "versionStr" string, and populate a "version" structure
 * returns if the parse was successful or not
 */
bool parseVersionString(const char *versionStr, icVersion *target)
{
    bool retVal = parseVersionStringWithDelimiter(versionStr, target, UNDERSCORE_DELIMITER);

    if (retVal == false)
    {
        // Try with periods
        retVal = parseVersionStringWithDelimiter(versionStr, target, PERIOD_DELIMITER);
    }

    return retVal;
}

/**
 * create a version string for display only (not meant to be parsed). Caller must
 * free the returned string
 * @param version the version to produce the string for
 * @return the resulting string
 */
char *produceDisplayVersionSrtring(icVersion *version)
{
    // returns a string that looks similar to:
    //  10.07.00.000000
    char *retval = stringBuilder("%02" PRIu8 ".%02" PRIu8 ".%02" PRIu8 ".%06" PRIu64,
                                 version->releaseNumber, version->serviceUpdateNumber,
                                 version->maintenanceReleaseNumber, version->hotfixNumber);
    return retval;
}

/*
 * create the "versionStr" (what could be parsed).  caller
 * MUST free the returned string
 */
char *produceVersionString(icVersion *info)
{
    // return a string that looks similar to:
    //    7_2_0_0_201505221523
    //
    char retVal[1024];

    if (info->isSnapshot == true)
    {
        // add numbers + SNAPSHOT
        //
        sprintf(retVal, "%" PRIu8 "_%" PRIu8 "_%" PRIu8 "_%" PRIu64 "_SNAPSHOT", info->releaseNumber,
                info->serviceUpdateNumber, info->maintenanceReleaseNumber, info->hotfixNumber);
    }
    else
    {
        // add basic numbers
        //
        sprintf(retVal, "%" PRIu8 "_%" PRIu8 "_%" PRIu8 "_%" PRIu64 "_%"PRIi64, info->releaseNumber,
                info->serviceUpdateNumber, info->maintenanceReleaseNumber, info->hotfixNumber, info->buildNumber);
    }

    // return copy
    //
    return strdup(retVal);
}

/*
 * compares two version structures to see which is more recent
 * returns:
 *   -1 if 'left' is newer
 *    1 if 'right' is newer
 *    0 if they are the same
 */
int compareVersions(icVersion *left, icVersion *right)
{
    // start at the top, and compare each number
    // first, release number
    //
    if (left->releaseNumber > right->releaseNumber)
    {
        return -1;
    }
    else if (right->releaseNumber > left->releaseNumber)
    {
        return 1;
    }

    // compare SU number
    //
    if (left->serviceUpdateNumber > right->serviceUpdateNumber)
    {
        return -1;
    }
    else if (right->serviceUpdateNumber > left->serviceUpdateNumber)
    {
        return 1;
    }

    // compare MR number
    //
    if (left->maintenanceReleaseNumber > right->maintenanceReleaseNumber)
    {
        return -1;
    }
    else if (right->maintenanceReleaseNumber > left->maintenanceReleaseNumber)
    {
        return 1;
    }

    // compare HF number
    //
    if (left->hotfixNumber > right->hotfixNumber)
    {
        return -1;
    }
    else if (right->hotfixNumber > left->hotfixNumber)
    {
        return 1;
    }

    // if 'either' are snapshots then allow the update.
    // this way we can go to/from a SNAPSHOT build
    //
    if (left->isSnapshot == true || right->isSnapshot == true)
    {
        // claim left is greater (allow it to win)
        //
        return -1;
    }

    // now the build number, incorporating the 'tolerance' to the left
    // (this is how the Java implementation did it)
    //
    if ((left->buildNumber + left->buildNumTolerance) > right->buildNumber)
    {
        return -1;
    }
    else if ((left->buildNumber + left->buildNumTolerance) < right->buildNumber)
    {
        return 1;
    }

    // must be the same
    //
    return 0;
}

/*
 * returns true if the version object is empty (all values set to 0)
 */
bool isVersionEmpty(icVersion *info)
{
    if (info->releaseNumber == 0 &&
        info->serviceUpdateNumber == 0 &&
        info->maintenanceReleaseNumber == 0 &&
        info->hotfixNumber == 0 &&
        (info->buildNumber == 0 || info->buildNumber == SNAPSHOT_BUILD_NUMBER))
    {
        return true;
    }

    return false;
}

/*
 * Processes a single line in a version file and populates the icVersionFile provided with the line details
 * if they are recognized.
 */
static void parseVersionFileLine(icVersionFile *versionFile, const char *line)
{
    size_t maxLineLength = strlen(line) + 1;
    char left[maxLineLength];
    memset(left, 0, maxLineLength);
    char right[maxLineLength];
    memset(right, 0, maxLineLength);

    // Try to split the line (left and right will both be trimmed). If we failed to split, it may be
    // that the value to the right of the token is an empty string. That's okay, we will let that
    // through as long as the key exists.
    if (stringSplitOnToken(line, ':', left, right) == true || strlen(left) > 0)
    {
        if (parseVersionFileEntry(versionFile, left, right) == false)
        {
            icLogWarn(LOG_TAG, "%s: Unrecognized key - %s", __func__, left);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: Encountered line with unexpected syntax", __func__ );
    }
}

/*
 * Tries to match 'key' to a known key name. If it matches, 'value' will be duplicated and set for
 * the corresponding member in 'versionFile'. Returns true if this happens, false otherwise
 */
static bool parseVersionFileEntry(icVersionFile *versionFile, const char *key, const char *value)
{
    bool retVal = false;

    if (versionFile != NULL && stringIsEmpty(key) == false && value != NULL)
    {
        // Loop over known keys and try to match one
        for (size_t i = 0; i < ARRAY_LENGTH(knownVersionFileKeys); i++)
        {
            const VersionFileKeyEntry *entry = &knownVersionFileKeys[i];
            if (stringCompare(key, entry->key, false) == 0)
            {
                // We are going to return true for any recognized keys, and
                // only populate versionFile if it has a corresponding member.
                switch (entry->index)
                {
                    case VersionFileKeyVersion:
                        retVal = parseVersionString(value, versionFile->version);
                        break;
                    case VersionFileKeyLastCompatibleVersion:
                        retVal = parseVersionString(value, versionFile->lastCompatibleVersion);
                        break;
                    case VersionFileKeyLongVersion:
                    case VersionFileKeyReleaseVer:
                    case VersionFileKeyServiceVer:
                    case VersionFileKeyMaintenanceVer:
                    case VersionFileKeyHotFixVer:
                    case VersionFileKeyDisplayVer:
                    case VersionFileKeyServerVer:
                    case VersionFileKeySvnBuild:
                    case VersionFileKeySvnDate:
                    case VersionFileKeyBuildId:
                    case VersionFileKeyBuildDate:
                    case VersionFileKeyBuildBy:
                    default:
                        // Don't currently have members
                        retVal = true;
                }
            }
        }
    }

    return retVal;
}

static bool parseVersionStringWithDelimiter(const char *versionStr, icVersion *target, const char *delimiter)
{
    int tokensProcessed = 0;

    // strings are formted as R_SU_MR_HF_build
    // so tokenize on underscore and populate the values
    //
    if (versionStr != NULL && target != NULL && delimiter != NULL)
    {
        // zero out all of the numbers within 'target'
        //
        memset(target, 0, sizeof(icVersion));

        // ensure string ends with our token so we don't have
        // do perform some crazy logic to grab the last piece
        //
        char *copy = (char *)malloc(strlen(versionStr) + strlen(delimiter) + 1);
        sprintf(copy, "%s%s", versionStr, delimiter);

        // being getting tokens from the input string
        //
        char *ctx;
        char *token = strtok_r(copy, delimiter, &ctx);
        uint64_t tmp;
        if (token != NULL)
        {
            // extract R, then go to next token
            stringToUnsignedNumberWithinRange((const char *)token, &tmp, 10, 0, UINT8_MAX);
            target->releaseNumber = tmp;
            token = strtok_r(NULL, delimiter, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // extract SU, then go to next token
            stringToUnsignedNumberWithinRange((const char *)token, &tmp, 10, 0, UINT8_MAX);
            target->serviceUpdateNumber = tmp;
            token = strtok_r(NULL, delimiter, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // extract MR, then go to next token
            stringToUnsignedNumberWithinRange((const char *)token, &tmp, 10, 0, UINT8_MAX);
            target->maintenanceReleaseNumber = tmp;
            token = strtok_r(NULL, delimiter, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // extract HF, then go to next token
            stringToUnsignedNumberWithinRange((const char *)token, &target->hotfixNumber, 10, 0, UINT64_MAX);
            token = strtok_r(NULL, delimiter, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // special case for SNAPSHOT
            //
            if (strcmp(token, "SNAPSHOT") == 0)
            {
                target->isSnapshot = true;
                target->buildNumber = SNAPSHOT_BUILD_NUMBER;
            }
            else
            {
                // extract buildNumber
                stringToNumberWithinRange((const char *)token, &target->buildNumber, 10, 0, INT64_MAX);
            }
            tokensProcessed++;
        }

        // mem cleanup
        //
        free(copy);
    }

    // success if we got at least 2 tokens processed
    //
    return (tokensProcessed >= 2);
}