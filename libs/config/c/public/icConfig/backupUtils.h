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
    ORIGINAL_FILE,     // use original file
    BACKUP_FILE,       // use .bak file
    FILE_NOT_PRESENT   // need to create the file (a readable version is not present)
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

#endif //IC_BACKUPUTILS_H
