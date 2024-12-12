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
 * backupUtils.h
 *
 * File backup utility functions
 *
 * Author: jgleason
 *-----------------------------------------------*///

#ifndef IC_BACKUPUTILS_H
#define IC_BACKUPUTILS_H

typedef enum
{
    ORIGINAL_FILE,   // use original file
    BACKUP_FILE,     // use .bak file
    FILE_NOT_PRESENT // need to create the file (a readable version is not present)
} fileToRead;

typedef enum
{
    CONFIG_FILE_FORMAT_XML,
    CONFIG_FILE_FORMAT_JSON
} configFileFormat;

/*
 * safeFileSave() - safely save a file by renaming the temporary file
 *   to the desired file and creating a backup if possible.
 *   The temporary file should contain the updated information.
 *   The temporary file will be removed before returning.
 *   The backup file will be created if the desired file existed before the call.
 *
 *  Note: ALL FILENAMES MUST INCLUDE COMPLETE PATH TO FILE.
 *
 *  FLOW:
 *  if (desiredFile exists), desiredFile-->backupFile
 *  else, remove backupFile (it is stale)
 *  finally, tempFile-->desiredFile
 *
 */
void safeFileSave(char *tempFile, char *desiredFile, char *backupFile);

/*
 * chooseFileToRead() - determines if Original File exists with size, if not determine if Backup File exists with size
 *
 * returns the fileToRead enum result (ORIGINAL_FILE, BACKUP_FILE, or FILE_NOT_PRESENT)
 */
fileToRead chooseFileToRead(const char *originalFile, const char *backupFile, const char *configDir);

/*
 * similar to chooseFileToRead, but will perform a simple parse of the file to ensure it is not corrupted
 */
fileToRead chooseValidFileToRead(const char *originalFile, const char *backupFile, configFileFormat fileFormat);

#endif // IC_BACKUPUTILS_H
