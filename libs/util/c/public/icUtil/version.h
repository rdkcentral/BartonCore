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
 * version.h
 *
 * Model the standard iControl Version.
 * When represented as a string, looks similar to:
 *   7_2_0_0_201505221523
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_VERSION_H
#define IC_VERSION_H

#include <stdbool.h>
#include <stdint.h>

// the parsed representation of a "version"
typedef struct _icVersion
{
    uint8_t releaseNumber;
    uint8_t serviceUpdateNumber;
    uint8_t maintenanceReleaseNumber;
    uint64_t hotfixNumber;
    int64_t buildNumber;
    int32_t buildNumTolerance; // used in Upgrade scenarios.  has to be manually assigned
    bool isSnapshot;
} icVersion;

typedef struct
{
    icVersion *version;
    icVersion *lastCompatibleVersion;
    // Can add others at a later time if needed
} icVersionFile;

typedef enum
{
    VERSION_FILE_PARSE_SUCCESS,
    VERSION_FILE_PARSE_GENERAL_FAILURE,
    VERSION_FILE_PARSE_BAD_ARGS,
    VERSION_FILE_PARSE_MISSING_FILE,
    VERSION_FILE_PARSE_FAILED_READ
} icVersionFileParseStatus;

/*
 * Create an icVersionFile type
 */
icVersionFile *createVersionFile(void);

/*
 * Cleanup an icVersionFile
 */
void destroyVersionFile(icVersionFile *versionFile);

/*
 * Try to parse a firmware version file at the specified path into an icVersionFile container.
 * Returns an enum indicating the status of the routine. On success, VERSION_FILE_PARSE_SUCCESS
 * will be returned
 */
icVersionFileParseStatus parseVersionFile(const char *versionFilePath, icVersionFile *output);

/*
 * parse the "versionStr" string, and populate a "version" structure
 * returns if the parse was successful or not
 */
extern bool parseVersionString(const char *versionStr, icVersion *target);

/*
 * create the "versionStr" (what could be parsed).  caller
 * MUST free the returned string
 */
extern char *produceVersionString(icVersion *info);

/**
 * create a version string for display only (not meant to be parsed). Caller must
 * free the returned string
 * @param version the version to produce the string for
 * @return the resulting string
 */
char *produceDisplayVersionSrtring(icVersion *version);

/*
 * compares two version structures to see which is more recent
 * returns:
 *   -1 if 'left' is newer
 *    1 if 'right' is newer
 *    0 if they are the same
 */
extern int compareVersions(icVersion *left, icVersion *right);

/*
 * returns true if the version object is empty (all values set to 0)
 */
extern bool isVersionEmpty(icVersion *info);


#endif // IC_VERSION_H
