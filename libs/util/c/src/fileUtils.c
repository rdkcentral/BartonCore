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
 * fileUtils.c
 *
 * Utilities for filesystem or file io
 *
 * Author: mkoch - 4/26/18
 *-----------------------------------------------*/

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#ifdef CONFIG_LIB_TAR
#include <libtar.h>
#endif // CONFIG_LIB_TAR


#include <icLog/logging.h>
#include <icTypes/icSortedLinkedList.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>

#ifdef CONFIG_OS_DARWIN
#include <sys/syslimits.h> // for PATH_MAX
#endif
#define LOG_TAG    "FILEUTILS"

#define BUFFER_LEN 1024

typedef struct FileStorageInfo
{
    char *path;
    time_t lastUpdatedTimeSecs;
    uint32_t size;
} FileStorageInfo;

static bool fileStorageInfoDeleteFile(FileStorageInfo *info);

/**
 * Delete individual directories/files/symlinks from a directory.
 *
 * @param pathname The directory to delete the incoming file from.
 * @param dname The file/directory/symlink to delete.
 * @param dtype The file type (directory/symlink/file)
 * @param privateData Unused for deletion.
 * @return Zero on success, otherwise the errno will be returned as the value.
 */
int deleteDirHandler(const char *pathname, const char *dname, unsigned char dtype, void *privateData)
{
    int ret;

    if ((pathname == NULL) || (dname == NULL))
    {
        return EINVAL;
    }
    char *path = NULL;

    switch (dtype)
    {
        case DT_DIR:
        {
            path = stringBuilder("%s/%s", pathname, dname);

            if (path)
            {
                ret = listDirectory(path, deleteDirHandler, privateData);

                if (ret == 0 && ((ret = rmdir(path)) < 0))
                {
                    ret = errno;
                }
            }
            else
            {
                ret = ENOMEM;
            }

            break;
        }

        case DT_LNK:
        case DT_REG:
        case DT_BLK:
        case DT_CHR:
        case DT_FIFO:
        case DT_SOCK:
        case DT_UNKNOWN:
        {
            path = stringBuilder("%s/%s", pathname, dname);

            if (path)
            {
                if ((ret = unlink(path)) < 0)
                {
                    ret = errno;
                }
            }
            else
            {
                ret = ENOMEM;
            }

            break;
        }

        default:
            ret = ENOTSUP;
            break;
    }

    free(path);

    return ret;
}

/**
 * Copy individual directories/symlinks from source to destination.
 *
 * @param pathname The source directory to copy from.
 * @param dname The file/directory/symlink to copy.
 * @param dtype The file type (directory/symlink/file)
 * @param privateData The destination directory private data.
 * @return Zero on success, otherwise the errno will be returned as the value.
 */
static int copyDirHandler(const char *pathname, const char *dname, unsigned char dtype, void *privateData)
{
    int ret;

    const char *dst = privateData;

    char *srcpath = NULL;
    char *dstpath = NULL;

    switch (dtype)
    {
        case DT_DIR:
        {
            srcpath = stringBuilder("%s/%s", pathname, dname);
            dstpath = stringBuilder("%s/%s", dst, dname);

            if (srcpath && dstpath)
            {
                struct stat sinfo;

                errno = 0;
                if (stat(srcpath, &sinfo) != 0)
                {
                    char *errStr = strerrorSafe(errno);
                    icLogWarn(LOG_TAG, "%s: Cannot stat %s: %s", __FUNCTION__, srcpath, errStr);
                    free(errStr);
                    ret = errno;
                }
                else
                {
                    errno = 0;
                    ret = mkdir(dstpath, sinfo.st_mode);
                    if ((ret == 0) || (errno == EEXIST))
                    {
                        ret = listDirectory(srcpath, copyDirHandler, dstpath);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Error: Failed to create directory. [%s]", dstpath);
                        ret = errno;
                    }
                }
            }
            else
            {
                icLogError(LOG_TAG, "Error: Failed to allocate memory for paths");
                ret = ENOMEM;
            }

            break;
        }

        case DT_REG:
        case DT_UNKNOWN:
        {
            srcpath = stringBuilder("%s/%s", pathname, dname);
            dstpath = stringBuilder("%s/%s", dst, dname);

            if (srcpath && dstpath)
            {
                FILE *fin;

                fin = fopen(srcpath, "r");
                if (fin)
                {
                    FILE *fout;

                    fout = fopen(dstpath, "w+");
                    if (fout)
                    {
                        /*
                         * copyFile will close both `fin` and `fout`
                         * for us. Thus once to this point we cannot
                         * handle those pointers ourselves.
                         */
                        if (copyFile(fin, fout) == true)
                        {
                            /*
                             * However, we can only change the permissions if
                             * the file was actually copied.
                             *
                             * Since `copyFile` closes the destination FILE
                             * pointer we can safely perform this check.
                             */
                            ret = 0;
                            struct stat sinfo;

                            /* We know the path exists as we just pulled it from readdir */
                            if (stat(srcpath, &sinfo) == 0)
                            {
                                if (chmod(dstpath, sinfo.st_mode) != 0)
                                {
                                    char *errStr = strerrorSafe(errno);
                                    icLogWarn(LOG_TAG, "Failed to change permissions on [%s]: %s", dstpath, errStr);
                                    free(errStr);
                                }
                            }
                        }
                        else
                        {
                            icLogError(LOG_TAG, "Error: Failed to copy file from [%s] to [%s]", srcpath, dstpath);
                            ret = EINVAL;
                        }
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Failed to open destination [%s]", dstpath);
                        fclose(fin);

                        ret = errno;
                    }
                }
                else
                {
                    icLogError(LOG_TAG, "Failed to open source [%s]", srcpath);
                    ret = errno;
                }
            }
            else
            {
                icLogError(LOG_TAG, "Error: Failed to allocate memory for paths");
                ret = ENOMEM;
            }

            break;
        }

        case DT_LNK:
        {
            struct stat sinfo;
            char *target = NULL;
            size_t target_size = 0;
            ssize_t bytes = 0;

            srcpath = stringBuilder("%s/%s", pathname, dname);
            dstpath = stringBuilder("%s/%s", dst, dname);

            if (srcpath && dstpath)
            {
                if (lstat(srcpath, &sinfo) == 0)
                {
                    target_size = (size_t) ((sinfo.st_size == 0) ? PATH_MAX : (sinfo.st_size + 1));
                    target = malloc(target_size);
                }
                if (target)
                {
                    bytes = readlink(srcpath, target, target_size);

                    if (bytes <= 0)
                    {
                        icLogError(LOG_TAG, "Error: Failed to read symlink location.");
                        ret = errno;
                    }
                    else if (bytes == target_size)
                    {
                        icLogError(LOG_TAG, "Error: The path was too large for the buffer.");
                        ret = E2BIG;
                    }
                    else
                    {
                        /*
                         * `readlink` does not null-terminate the path.
                         */
                        target[bytes] = '\0';

                        /*
                         * Forcibly make sure there is no pre-existing link
                         * there. This way we know we will get our copied link.
                         */
                        unlink(dstpath);

                        ret = symlink(target, dstpath);
                        if (ret != 0)
                        {
                            icLogError(LOG_TAG,
                                       "Error: Failed to create symlink. target [%s], linkpath [%s]",
                                       target,
                                       dstpath);
                            ret = errno;
                        }
                    }

                    free(target);
                }
                else
                {
                    icLogError(LOG_TAG, "Error: Failed to allocate memory for symlink path");
                    ret = ENOMEM;
                }
            }
            else
            {
                icLogError(LOG_TAG, "Error: Failed to allocate memory for path");
                ret = ENOMEM;
            }

            break;
        }

        case DT_BLK:
        case DT_CHR:
        case DT_FIFO:
        case DT_SOCK:
        default:
            ret = ENOTSUP;
            break;
    }

    free(srcpath);
    free(dstpath);

    return ret;
}

/**
 * Create all directories in the path
 * @param path the directory path to create
 * @param mode the permissions on any directories that are created
 * @return 0 if success, < 0 if failed with errno set
 */
int mkdir_p(const char *path, mode_t mode)
{
    // Create a variable that we can modify while we are working
    char *workingPath = (char *) malloc(sizeof(char) * (strlen(path) + 1));
    char *p;
    strcpy(workingPath, path);

    int retval = 0;

    for (p = workingPath + 1; *p; ++p)
    {
        if (*p == '/')
        {
            // Temporarily terminate string for mkdir call
            *p = '\0';

            if (mkdir(workingPath, mode) != 0)
            {
                if (errno != EEXIST)
                {
                    retval = -1;
                    break;
                }
            }

            // Restore separator
            *p = '/';
        }
    }

    if (mkdir(workingPath, mode) != 0)
    {
        if (errno != EEXIST)
        {
            retval = -1;
        }
    }

    free(workingPath);

    return retval;
}

/**
 * Read the full contents of the supplied ASCII file.
 *
 * NOTE: caller is responsible for freeing the returned memory.
 *
 * @param filename - the file name to read from
 * @return - returns the contents of file name or NULL if error occurred
 */
char *readFileContents(const char *filename)
{
    if (filename == NULL)
    {
        icLogError(LOG_TAG, "Unable to read from file, got empty filename");
        return NULL;
    }

    char *retVal = NULL;

    // open the file for reading
    //
    scoped_FILE *fp = fopen(filename, "r");
    if (fp != NULL)
    {
        struct stat fileinfo;
        errno = 0;

        if (fstat(fileno(fp), &fileinfo) != 0)
        {
            scoped_generic char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "Cannot stat '%s':", errStr);
        }

        if (S_ISREG(fileinfo.st_mode) == false)
        {
            icLogError(LOG_TAG, "Can't read '%s': not a regular file!", filename);
            return NULL;
        }

        long length = -1;
        errno = 0;

        // get the length of the file.  first need to seek to the end
        //
        if (fseek(fp, 0, SEEK_END) == 0)
        {
            // get the position we're at
            //
            length = ftell(fp);

            // go back to the beginning
            //
            if (fseek(fp, 0, SEEK_SET) != 0)
            {
                length = -1;
            }
        }

        // make sure the file is not empty
        // FIXME: put an upper limit on this!
        if (length >= 0)
        {
            // allocate correct memory size mem
            // need to + 1 for '/0' char
            //
            retVal = calloc((size_t) length + 1, 1);

            // now read from the file
            //
            size_t len = fread(retVal, 1, (size_t) length, fp);
            if (len != length)
            {
                icLogError(LOG_TAG, "Unable to read file '%s'", filename);
                free(retVal);
                retVal = NULL;
            }
        }
        else
        {
            char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "Unable to determine length of file '%s': %s", filename, errStr);
            free(errStr);
        }
    }
    else
    {
        char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "Unable to open file '%s': %s", filename, errStr);
        free(errStr);
    }

    return retVal;
}

/**
 * Read full contents from filename.
 * Also trims off the extra '\n' char if it exists from contents
 *
 * NOTE: caller is responsible for freeing the returned memory.
 *
 * @param filename - the file name to read from
 * @return - returns the contents but trimmed down or NULL if error occurred
 */
char *readFileContentsWithTrim(const char *filename)
{
    // read from the file name
    //
    char *retVal = readFileContents(filename);
    if (retVal == NULL)
    {
        return NULL;
    }

    size_t len = strlen(retVal);

    // need to trim off the last '\n' character
    //
    if (len > 0 && retVal[len - 1] == '\n')
    {
        retVal[len - 1] = '\0';
    }

    return retVal;
}

/**
 * Writes contents to filename.
 * If file does not exist; it will be created.
 *
 * NOTE: clearing out the contents of the file if it exists
 *
 * @param filename - the file to write to
 * @param contents - the contents to add
 * @return true if successful, false if otherwise
 */
bool writeContentsToFileName(const char *filename, const char *contents)
{
    bool retVal = false;

    // sanity check
    if (filename == NULL || contents == NULL)
    {
        icLogError(LOG_TAG, "Unable to use filename and/or contents; for writing to filename");
        return retVal;
    }

    // open tmp file for writing
    //
    char *tmpFileName = stringBuilder("%s.tmp", filename);
    FILE *fp = fopen(tmpFileName, "w");
    if (fp != NULL)
    {
        errno = 0;

        // get the size to write and write to file
        //
        size_t sizeBuffer = strlen(contents);
        size_t writeOutput = fwrite(contents, 1, sizeBuffer, fp);

        // cleanup
        // run flush on file pointer
        //
        int cleanRC = fflush(fp);
        if (cleanRC != 0)
        {
            char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "When writing to file '%s' failed to run file flush: %s", filename, errStr);
            free(errStr);
        }

        // run fsync on file descriptor
        //
        int fd = fileno(fp); // get file pointer's file descriptor
        cleanRC = fsync(fd);
        if (cleanRC != 0)
        {
            char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "When writing to file '%s' failed to run file sync: %s", filename, errStr);
            free(errStr);
        }

        // finally close file pointer
        //
        fclose(fp);

        // make sure ALL contents were written
        //
        if (writeOutput == sizeBuffer)
        {
            // move tmp file to real fileName
            //
            retVal = moveFile(tmpFileName, filename);
            icLogTrace(
                LOG_TAG, "Moving tmp file '%s' to '%s' worked: %s", tmpFileName, filename, stringValueOfBool(retVal));
        }
        else
        {
            char *errStr = strerrorSafe(errno);
            icLogWarn(LOG_TAG, "Unable to write full contents to file '%s': %s", filename, errStr);
            free(errStr);

            // since failed to write to tmp file, delete it
            //
            bool fileDeleted = deleteFile(tmpFileName);
            icLogDebug(LOG_TAG, "Removing tmp file '%s' worked: %s", tmpFileName, stringValueOfBool(fileDeleted));
        }
    }
    else
    {
        char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "Unable to open file '%s': %s", tmpFileName, errStr);
        free(errStr);
    }

    // cleanup and return
    free(tmpFileName);
    return retVal;
}

/**
 * Copy between file streams (if the streams are not NULL).
 * this will cleanup the streams at the end (regardless of success)
 *
 * @param source the source file stream
 * @param dest the destination file stream
 */
bool copyFile(FILE *source, FILE *dest)
{
    bool retval = false;
    if (source != NULL && dest != NULL)
    {
        char buf[BUFFER_LEN];
        size_t size;

        while (feof(source) == 0)
        {
            size = fread(buf, 1, BUFFER_LEN, source);

            if (size >= 0 && ferror(source) == 0)
            {
                if (size > 0)
                {
                    if ((fwrite(buf, 1, size, dest) != size) || (ferror(dest) != 0))
                    {
                        retval = false;
                        icLogError(LOG_TAG, "%s: failed to write", __func__);
                        clearerr(dest);
                        break;
                    }
                    else
                    {
                        retval = true;
                    }
                }
                else if (feof(source) != 0)
                {
                    retval = true;
                    break;
                }
            }
            else
            {
                retval = false;
                icLogError(LOG_TAG, "%s: failed to read: eof: %d; err: %d", __func__, feof(source), ferror(source));
                clearerr(source);
                break;
            }
        }

        clearerr(source);

        // do fsync, only if fwrite is successful
        if (retval == true)
        {
            int rc = fsync(fileno(dest));
            if (rc != 0)
            {
                retval = false;
                char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "%s: fsync failed: %s", __func__, errStr);
                free(errStr);
            }
        }
    }

    if (source != NULL)
    {
        fclose(source);
    }
    if (dest != NULL)
    {
        fclose(dest);
    }

    return retval;
}

/**
 * Copy a file contents by path
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 */
bool copyFileByPath(const char *sourcePath, const char *destPath)
{
    bool result = false;
    if (sourcePath != NULL && destPath != NULL)
    {
        FILE *source = fopen(sourcePath, "r");
        if (source != NULL)
        {
            // write to the temp file first, then safely rename it to original
            char *tempPath = stringBuilder("%s%s", destPath, ".tmp");
            FILE *temp = fopen(tempPath, "w+");
            if (temp != NULL)
            {
                result = copyFile(source, temp);
                if (result == true)
                {
                    if (rename(tempPath, destPath) != 0)
                    {
                        result = false;
                        char *errStr = strerrorSafe(errno);
                        icLogError(LOG_TAG,
                                   "%s: failed to rename temp file %s to destination file %s and error string : %s ",
                                   __FUNCTION__,
                                   tempPath,
                                   destPath,
                                   errStr);
                        free(errStr);
                    }
                }

                // Here regardless of result value, if tempfile exist then we have to delete the temp file
                if (doesFileExist(tempPath) == true)
                {
                    if (unlink(tempPath) != 0)
                    {
                        char *errStr = strerrorSafe(errno);
                        icLogError(LOG_TAG,
                                   "%s: error removing temp file : %s and error string : %s ",
                                   __FUNCTION__,
                                   tempPath,
                                   errStr);
                        free(errStr);
                    }
                }
            }
            else
            {
                // This is normally cleaned up by copyFile in the happy path.
                fclose(source);
                source = NULL;
            }
            free(tempPath);
        }
    }
    return result;
}

/**
 * Move a file from one path to another.  Works across filesystems.
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 */
bool moveFile(const char *sourcePath, const char *destPath)
{
    bool result = false;

    if (sourcePath != NULL && destPath != NULL)
    {
        // first try rename.  This is easy and efficient and works so long as the source and dest are on the same
        // filesystem
        if (rename(sourcePath, destPath) == 0)
        {
            result = true;
        }
        else if (errno == EXDEV)
        {
            // the source and dest were not on the same filesystem.  Copy it over and delete the original
            if (copyFileByPath(sourcePath, destPath) == true)
            {
                if (unlink(sourcePath) != 0)
                {
                    icLogError(LOG_TAG, "%s: error removing source file! (errno=%d)", __FUNCTION__, errno);
                }
                else
                {
                    result = true;
                }
            }
        }
    }

    return result;
}

/*
 * returns if the file exists and has size to it
 */
bool doesNonEmptyFileExist(const char *filename)
{
    if (filename != NULL)
    {
        struct stat fileInfo;
        if (stat(filename, &fileInfo) == 0 && fileInfo.st_size > 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * returns if the file exists
 */
bool doesFileExist(const char *filename)
{
    if (filename != NULL)
    {
        struct stat fileInfo;
        if (stat(filename, &fileInfo) == 0)
        {
            return true;
        }
    }
    return false;
}

/**
 * returns if the directory exists
 */
bool doesDirExist(const char *dirPath)
{
    if (dirPath != NULL)
    {
        struct stat st;
        if (stat(dirPath, &st) == 0 && S_ISDIR(st.st_mode))
        {
            return true;
        }
    }

    return false;
}

int listDirectory(const char *dir, directoryHandler handler, void *privateData)
{
    DIR *directory;
    struct dirent *entry;

    if (handler == NULL)
    {
        return EINVAL;
    }

    directory = opendir(dir);
    if (directory == NULL)
    {
        icLogError(LOG_TAG, "Error: Bad directory specified.");
        return EBADF;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        int ret;

        // skip 'self' and 'parent'
        //
        if ((entry->d_type == DT_DIR) && ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)))
        {
            continue;
        }

        if ((ret = handler(dir, entry->d_name, entry->d_type, privateData)) != 0)
        {
            icLogError(
                LOG_TAG, "Error: Failed on dir: [%s], file: [%s], type: [%u]", dir, entry->d_name, entry->d_type);
            closedir(directory);
            return ret;
        }
    }

    closedir(directory);

    return 0;
}

/*
 * helper to recursively delete a directory and the files within it
 */
bool deleteDirectory(const char *path)
{
    bool output = false;
    errno = 0;

    if (deleteDirectoryContent(path) == true)
    {
        int ret = rmdir(path);
        if (ret == 0)
        {
            output = true;
        }
        else
        {
            scoped_generic char *errStr = strerrorSafe(ret);
            icLogError(LOG_TAG, "Error: Failed to delete directory. [%s]", errStr);
        }
    }

    return output;
}

/*
 * helper to recursively delete content within a directory but not the directory itself.
 */
bool deleteDirectoryContent(const char *path)
{
    int ret;
    errno = 0;

    ret = listDirectory(path, deleteDirHandler, NULL);
    if (ret != 0)
    {
        scoped_generic char *errStr = strerrorSafe(ret);
        icLogError(LOG_TAG, "Error: Failed to delete directory content. [%s]", errStr);
        return false;
    }

    return true;
}

/**
 * Delete a file
 * @param filename full path to the file to be deleted
 */
bool deleteFile(const char *filename)
{
    bool retVal = false;
    if (filename != NULL)
    {
        if (unlink(filename) == 0)
        {
            retVal = true;
        }
    }
    return retVal;
}

bool copyDirectory(const char *src, const char *dst)
{
    int ret;

    struct stat sinfo;

    char realdst[PATH_MAX];

    if ((src == NULL) || (src[0] == '\0'))
    {
        icLogError(LOG_TAG, "Error: Invalid source provided.");
        return false;
    }

    if ((dst == NULL) || (dst[0] == '\0'))
    {
        icLogError(LOG_TAG, "Error: Invalid destination provided.");
        return false;
    }

    if (stat(src, &sinfo) != 0)
    {
        icLogError(LOG_TAG, "Error: Cannot read source path.");
        return false;
    }
    else if ((sinfo.st_mode & S_IFDIR) == 0)
    {
        icLogError(LOG_TAG, "Error: Source path is not a directory.");
        return false;
    }

    realdst[PATH_MAX - 1] = '\0';
    safeStringCopy(realdst, PATH_MAX - 1, (char *) dst);

    ret = mkdir(realdst, sinfo.st_mode);
    if (ret != 0)
    {
        if (errno == EEXIST)
        {
            /*
             * If this is a directory then it already exists. Thus
             * to match functionality of `cp -a` we want to copy
             * the folder into the one that exists.
             *
             * Example:
             * Source: /tmp/src/my_dir
             * Dest:   /tmp/dst/my_dir
             *
             * If "dest" "my_dir" does NOT exist:
             * mkdir /tmp/dst/my_dir
             *
             * If "dest" "my_dir" does exist:
             * mkdir /tmp/dst/my_dir/my_dir
             */
            char *basesrc, *tmpsrc;

            basesrc = strdup(src);
            tmpsrc = basename(basesrc);

            safeStringAppend(realdst, PATH_MAX - strlen(realdst) - 1, "/");
            safeStringAppend(realdst, PATH_MAX - strlen(realdst) - 1, tmpsrc);

            free(basesrc);

            /*
             * Try and mimic `cp -a` behaviour here. It does
             * not error out if the sub-directory already exists.
             * Instead it will overwrite existing files, directories,
             * and symlinks.
             */
            ret = mkdir(realdst, sinfo.st_mode);
            if ((ret != 0) && (errno != EEXIST))
            {
                scoped_generic char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "Error: Failed to create destination directory: [%s] [%s]", realdst, errStr);

                return false;
            }
        }
        else
        {
            scoped_generic char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "Error: Destination cannot be created [%d:%s].", errno, errStr);

            return false;
        }
    }

    ret = listDirectory(src, copyDirHandler, realdst);
    if (ret != 0)
    {
        scoped_generic char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "Error: Failed to copy directory. [%s]", errStr);
        return false;
    }

    return true;
}

static void storageFileInfoFreeFunc(void *item)
{
    FileStorageInfo *info = (FileStorageInfo *) item;
    if (info != NULL)
    {
        free(info->path);
        free(info);
    }
}

/*
 * compare new element with existing element of FileStorageInfo to sort in ascending order.
 * @param  new element
 * @param  existing element
 */
static int8_t fileInfoCompareFunc(void *newItem, void *element)
{
    FileStorageInfo *newInfo = (FileStorageInfo *) newItem;
    FileStorageInfo *existingInfo = (FileStorageInfo *) element;

    if (existingInfo->lastUpdatedTimeSecs < newInfo->lastUpdatedTimeSecs)
    {
        return 1;
    }
    else if (existingInfo->lastUpdatedTimeSecs > newInfo->lastUpdatedTimeSecs)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

/**
 * Iterate over a dir and remove the oldest files until we are under our directory size limit. Then, continue to remove
 * files that are older than maxAllowedAgeInSeconds.
 *
 * @param dirToBeChecked - The directory to purge old files from
 * @param fileExtension - The subset of files to cleanup by file extension. Caller may pass "*" to allow all file types.
 * @param maxAllowedFileStorageMb - The size limit, in Mb, that is trying to be reached by purging files
 * @param maxAllowedAgeInSeconds - The maximum allowed number of seconds since the file was last modified. Files older
 *                                  than this will be deleted. Caller may provide 0 to skip this step entirely.
 * @return true if we successfully purged files below the storage cap and removed files older than the allowed age
 *              in seconds, false otherwise.
 */
bool fileUtilsDeleteOld(const char *dirToBeChecked,
                        const char *fileExtension,
                        uint32_t maxAllowedFileStorageMb,
                        uint32_t maxAllowedAgeInSeconds)
{
    if (stringIsEmpty(dirToBeChecked) == true || stringIsEmpty(fileExtension) == true)
    {
        icLogError(LOG_TAG, "%s: invalid arguments provided.", __FUNCTION__);
        return false;
    }

    DIR *d;
    if ((d = opendir(dirToBeChecked)) == NULL)
    {
        icLogError(LOG_TAG, "%s: failed to get handle for directory %s", __FUNCTION__, dirToBeChecked);
        return false;
    }

    // if caller wants to allow deletion of all file types from dirToBeChecked
    // then allow for wildcard fileExtension.
    //
    if (stringCompare(fileExtension, "*", false) == 0)
    {
        fileExtension = "";
    }

    // first collect a linked list sorted by last modification date
    //
    icSortedLinkedList *files = sortedLinkedListCreate();
    uint32_t totalSize = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        if (entry->d_type == DT_REG && stringEndsWith(entry->d_name, fileExtension, false) == true)
        {
            FileStorageInfo *info = calloc(1, sizeof(FileStorageInfo));
            info->path = stringBuilder("%s/%s", dirToBeChecked, entry->d_name);
            struct stat s;
            if (stat(info->path, &s) == 0)
            {
#ifndef CONFIG_OS_DARWIN
                info->lastUpdatedTimeSecs = s.st_mtim.tv_sec;
#endif
                info->size = s.st_size;
                totalSize += s.st_size;
                if (sortedLinkedListAdd(files, info, fileInfoCompareFunc) == false)
                {
                    icLogWarn(LOG_TAG, "%s: Failed to add file information for %s to list", __FUNCTION__, info->path);
                    storageFileInfoFreeFunc(info);
                }
            }
            else
            {
                icLogWarn(LOG_TAG, "%s: could not stat file %s.", __FUNCTION__, info->path);
                storageFileInfoFreeFunc(info);
            }
        }
    }
    closedir(d);

    // now loop through our files and remove the oldest one until we are under our size cap
    //
    bool retVal = true;
    // Keep a retry counter for when we fail to unlink. If we somehow fail to unlink, we're probably going to keep
    // failing, which would cause us to loop forever. Use this counter to give up on the file after a while.
    uint8_t fileRetryCount = 0;
    while (totalSize > (maxAllowedFileStorageMb * 1024 * 1024) && linkedListCount(files) > 0)
    {
        icLogDebug(LOG_TAG, "%s: totalSize now %" PRIu32, __FUNCTION__, totalSize);

        FileStorageInfo *info = linkedListGetElementAt(files, 0);
        retVal = fileStorageInfoDeleteFile(info);
        if (retVal == true)
        {
            fileRetryCount = 0;
            linkedListRemove(files, 0);
            totalSize -= info->size;
        }
        else if (++fileRetryCount >= 3)
        {
            // Tried 3 times to remove the file and failed. Move on to another so we don't loop forever.
            linkedListRemove(files, 0);
        }
    }

    // now again loop through our files and remove file which is greater than maxAllowedAgeInSeconds.
    // if maxAllowedAgeInSeconds is 0 means we want to keep file as it as.
    //
    if (maxAllowedAgeInSeconds != 0)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(files);
        while (linkedListIteratorHasNext(iter) == true)
        {
#ifdef CONFIG_OS_DARWIN
            // do this the Mac way
            //
            struct timeval now;
            gettimeofday(&now, NULL);
#else
            // standard linux way
            //
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
#endif
            FileStorageInfo *info = linkedListIteratorGetNext(iter);
            // Make sure the clock isn't time traveling
            if (now.tv_sec >= info->lastUpdatedTimeSecs)
            {
                time_t fileAge = (now.tv_sec) - (info->lastUpdatedTimeSecs);
                if (fileAge > maxAllowedAgeInSeconds)
                {
                    retVal = fileStorageInfoDeleteFile(info);
                    if (retVal == true)
                    {
                        linkedListRemove(files, 0);
                    }
                    else
                    {
                        icLogError(LOG_TAG,
                                   "%s: File %s is older than the specified age, but we failed to remove it. "
                                   "Skipping file.",
                                   __FUNCTION__,
                                   info->path);
                    }
                }
                else
                {
                    break;
                }
            }
        }
        // clean up
        //
        linkedListIteratorDestroy(iter);
    }
    linkedListDestroy(files, storageFileInfoFreeFunc);
    return retVal;
}

/**
 * Helper for FileStorageInfo operations to remove the file from the filesystem.
 *
 * TODO: This is basically deleteFile with a check for ENOENT. This should be merged
 * with that API.
 *
 * @param info
 * @return true if the provided file is no longer on the filesystem, false otherwise
 */
static bool fileStorageInfoDeleteFile(FileStorageInfo *info)
{
    bool retVal = false;

    if (info != NULL)
    {
        errno = 0;
        if (unlink(info->path) == 0 || errno == ENOENT)
        {
            retVal = true;
        }
    }

    return retVal;
}


/*
 * create a marker file of zero length
 */
bool createMarkerFile(const char *path)
{
    int rc = creat(path, S_IRWXU | S_IRGRP | S_IROTH);
    if (rc >= 0)
    {
        close(rc);
        return true;
    }
    else
    {
        scoped_generic char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "Error: %s: unable to create marker file %s: %s", __FUNCTION__, path, errStr);
        return false;
    }
}

#ifdef CONFIG_LIB_TAR
/*
 * Extracts contents of given tar file to given output directory.
 * Note: This currently does not handle tgz extraction
 *
 * @param tarFile : Full path to tar file including name.
 * @param outputDir : Path to output directory where the contents of provided
 *                    tar are to be extracted.
 * @return true if the extraction could be completed, false otherwise.
 *
 */
bool extractTarFile(const char *tarFileName, const char *outputDir)
{
    TAR *tar_handle = NULL;
    bool retVal = false;
    errno = 0;

    if ((tar_open(&tar_handle, tarFileName, NULL, O_RDONLY, 0, TAR_GNU) == 0) &&
        (tar_extract_all(tar_handle, (char *) outputDir) == 0))
    {
        retVal = true;
    }
    else
    {
        // log warning, cleanup, and return the error
        //
        scoped_generic char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "%s: Error extracting via tar - %s [%d]", __FUNCTION__, errStr, errno);
        retVal = false;
    }

    // Free TAR handle if it's not NULL
    if (tar_handle != NULL)
    {
        tar_close(tar_handle);
        tar_handle = NULL;
    }
    return retVal;
}
#endif // CONFIG_LIB_TAR
