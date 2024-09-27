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
 * backupUtils.c
 *
 * functions for safely reading/writing files
 *   to avoid corrupted files
 *
 * Author: jgleason - 7/5/16
 *-----------------------------------------------*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <cjson/cJSON.h>
#include <libxml/parser.h>

#include <icLog/logging.h>
#include <icConfig/backupUtils.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>

#define LOG_TAG                     "backupUtil"

/*
 * Backup Utility Functions
 */


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
void safeFileSave(char *tempFile, char *originalFile, char *backupFile)
{
    // rename config file to .bak if it exists
    //
    if (rename(originalFile, backupFile) != 0)
    {
        if (errno == ENOENT)
        {
            // no file to backup, remove the .bak file if it exists (stale)
            //
            if (unlink(backupFile) != 0 && errno != ENOENT)
            {
                char *errStr = strerrorSafe(errno);
                icLogWarn(LOG_TAG, "%s: Failed to remove file '%s': %s", __FUNCTION__, backupFile, errStr);
                free(errStr);
            }
        }
        else
        {
            // This should not happen, but attempt to put the new file into service anyway
            char *errStr = strerrorSafe(errno);
            icLogWarn(LOG_TAG, "%s: Failed to rename file '%s' to '%s': %s", __FUNCTION__, originalFile, backupFile, errStr);
            free(errStr);
        }
    }

    if (rename(tempFile, originalFile) != 0)
    {
        char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "%s: Failed to rename file '%s' to '%s': %s", __FUNCTION__, tempFile, originalFile, errStr);
        free(errStr);
    }
}

/*
 * chooseFileToRead() - determines if Original File exists with size, if not determine if Backup File exists with size
 *
 * returns the fileToRead enum result (ORIGINAL_FILE, BACKUP_FILE, or FILE_NOT_PRESENT)
*
 */
fileToRead chooseFileToRead(const char *originalFile, const char *backupFile, const char *configDir)
{
    if (doesNonEmptyFileExist(originalFile) == true)
    {
        // original file is present and readable
        //
        icLogDebug(LOG_TAG, "File is safe to read, %s", originalFile);
        return ORIGINAL_FILE;
    }
    else if (doesNonEmptyFileExist(backupFile) == true)
    {
        // .bak file exists with at least 1 byte, so rename it to config then parse it
        //
        icLogDebug(LOG_TAG, "File does not exist for reading, using backup file - %s", backupFile);
        return BACKUP_FILE;
    }

    // create the directory (if provided) so that 'write' works later on
    //
    if (configDir != NULL && mkdir(configDir, 0755) != 0)
    {
        scoped_generic char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "error creating directory %s - %d %s", configDir, errno, errStr);
    }

    // neither original nor backup file exist - file must be created
    //
    icLogWarn(LOG_TAG, "Original and backup files are not present - must create a new default %s", originalFile);
    return FILE_NOT_PRESENT;
}

/*
 * rename a file with a .bad extension.  used to prevent re-parsing a corrupted file
 */
static void renameBadFile(const char *fileName)
{
    scoped_generic char *badFileName = stringBuilder("%s.bad", fileName);
    if (rename(fileName, badFileName) != 0)
    {
        if (isIcLogPriorityTrace() == true)
        {
            scoped_generic char *errStr = strerrorSafe(errno);
            icLogTrace(LOG_TAG, "%s: unable to rename %s to %s: %s", __func__, fileName, badFileName, errStr);
        }
    }
}

/*
 * returns if the supplied file can be parsed (simple) in the defined format
 */
static bool isFileValidForFormat(const char *fileName, configFileFormat format)
{
    bool retVal = false;

    // first read the contents of the file
    //
    scoped_generic char *content = readFileContents(fileName);
    if (content != NULL)
    {
        // parse based on format
        //
        if (format == CONFIG_FILE_FORMAT_XML)
        {
            // simple parse of XML
            xmlDoc *doc = xmlReadMemory(content, (int) strlen(content), NULL, "UTF-8", 0);
            if (doc != NULL)
            {
                xmlFreeDoc(doc);
                retVal = true;
            }
            else
            {
                // add .bad extension to the file so it doesn't get parsed again
                icLogWarn(LOG_TAG, "%s: %s is not valid XML!", __func__, fileName);
                renameBadFile(fileName);
            }
        }
        else if (format == CONFIG_FILE_FORMAT_JSON)
        {
            // simple parse of JSON
            cJSON *json = cJSON_Parse(content);
            if (json != NULL)
            {
                cJSON_Delete(json);
                retVal = true;
            }
            else
            {
                // add .bad extension to the file so it doesn't get parsed again
                icLogWarn(LOG_TAG, "%s: %s is not valid JSON!", __func__, fileName);
                renameBadFile(fileName);
            }
        }
        // TODO: support additional file formats?
    }

    return retVal;
}

/*
 * similar to chooseFileToRead, but will perform a simple parse of the file to ensure it is not corrupted
 */
fileToRead chooseValidFileToRead(const char *originalFile, const char *backupFile, configFileFormat fileFormat)
{
    // TODO: ideally this should work in conjunction with storage.c as it has similar functionality

    // first check the original file
    if (doesNonEmptyFileExist(originalFile) == true)
    {
        // parse based on the supplied format
        if (isFileValidForFormat(originalFile, fileFormat) == true)
        {
            icLogTrace(LOG_TAG, "%s: returning ORIGINAL_FILE %s", __func__, originalFile);
            return ORIGINAL_FILE;
        }
    }

    // original no good, so try backup file
    if (doesNonEmptyFileExist(backupFile) == true)
    {
        // parse based on the supplied format
        if (isFileValidForFormat(backupFile, fileFormat) == true)
        {
            icLogTrace(LOG_TAG, "%s: returning BACKUP_FILE %s", __func__, backupFile);
            return BACKUP_FILE;
        }
    }

    // nether file is valid
    icLogWarn(LOG_TAG, "%s: returning FILE_NOT_PRESENT", __func__);
    return FILE_NOT_PRESENT;
}