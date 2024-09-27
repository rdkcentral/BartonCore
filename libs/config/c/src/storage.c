//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by tlea on 4/4/18.
//

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <pthread.h>

#include <fcntl.h>
#include <icConcurrent/threadUtils.h>
#include <icConfig/backupUtils.h>
#include <icConfig/simpleProtectConfig.h>
#include <icConfig/storage.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/array.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define LOG_TAG "storage"
#define logFmt(fmt) "%s: " fmt, __func__

#define STORAGE_DIR "storage"

//FIXME: this big lock can likely go away with atomic fs ops
static pthread_mutex_t mtx;
static bool loadInternalLocked(const char *namespace, const char *key, char **contentOut, const StorageCallbacks *cb);

static char *configPath = NULL;

static pthread_once_t initOnce = PTHREAD_ONCE_INIT;
static void oneTimeInit(void)
{
    mutexInitWithType(&mtx, PTHREAD_MUTEX_ERRORCHECK);
}

typedef struct
{
    char* main; // the primary filename
    char* backup; // the backup
    char* temp; //the temporary working file
    char* bad; //path to move any invalid files to
} StorageFilePaths;

static char* getNamespacePath(const char* namespace)
{
    char *configDir = strdupOpt(configPath);
    char *path = NULL;
    if (configPath != NULL)
    {
        path = stringBuilder("%s/%s/%s", configDir, STORAGE_DIR, namespace);
    }

    free(configDir);

    return path;
}

static void getFilepaths(const char* namespace, const char* key, StorageFilePaths* paths)
{
    char* path = getNamespacePath(namespace);

    paths->main = stringBuilder("%s/%s", path, key);

    paths->backup = stringBuilder("%s/%s.bak", path, key);

    paths->temp = stringBuilder("%s/%s.tmp", path, key);

    paths->bad = stringBuilder("%s/%s.bad", path, key);

    free(path);
}

static void freeFilePaths(StorageFilePaths* paths)
{
    free(paths->main);
    free(paths->backup);
    free(paths->temp);
    free(paths->bad);
}

void storageSetConfigPath(const char *_configPath)
{
    if (configPath != NULL)
    {
        icDebug("Changing config path from %s to %s", configPath, stringCoalesce(_configPath));
        free(configPath);
    }

    configPath = strdup(_configPath);
}

const char *storageRestoreErrorDescribe(StorageRestoreErrorCode code)
{
    const char *descr = "(unrecognized)";

    if (code >= 0 && code < ARRAY_LENGTH(StorageRestoreErrorLabels))
    {
        descr = StorageRestoreErrorLabels[code];
    }

    return descr;
}

bool storageSave(const char* namespace, const char* key, const char* value)
{
    bool result = true;

    if (namespace == NULL || key == NULL || value == NULL)
    {
        icLogError(LOG_TAG, "storageSave: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    uint64_t startMillis = getMonotonicMillis();

    mutexLock(&mtx);

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    //ensure the namespace directory exists
    char* path = getNamespacePath(namespace);

    struct stat buf;
    if (stat(path, &buf) && errno == ENOENT)
    {
        if (mkdir_p(path, 0777) == -1)
        {
            icLogError(LOG_TAG, "storageSave: failed to create directory %s: %s", path, strerror(errno));
            result = false;
        }
    }

    if (result == true)
    {
        //write to the temp file first, then safely move with our backupUtils
        FILE* fp = fopen(paths.temp, "w");
        if (fp != NULL)
        {
            if (fputs(value, fp) < 0)
            {
                icLogError(LOG_TAG, "storageSave: failed to store value at %s", paths.temp);
                result = false;
            }

            if (fflush(fp) != 0)
            {
                char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "%s: fflush failed when trying to save %s with error: %s", __func__, paths.temp, errStr);
                free(errStr);
                result = false;
            }
            int fd = fileno(fp);
            int rc = fsync(fd);
            if(rc != 0)
            {
                char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "storageSave:  fsync on %s failed when trying to save %s with error: %s",paths.temp,path,errStr);
                free(errStr);
                result = false;
            }
            fclose(fp);

            if (result == true)
            {
                safeFileSave(paths.temp, paths.main, paths.backup); //make the swap safely and with backup

                uint64_t endMillis = getMonotonicMillis();
                icLogDebug(LOG_TAG, "%s: saved file %s in %" PRId64 "ms", __FUNCTION__, paths.main, endMillis - startMillis);
            }
        }
        else
        {
            char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "storageSave: fopen failed for %s: %s", paths.temp, errStr);
            free(errStr);
            result = false;
        }
    }

    mutexUnlock(&mtx);

    free(path);
    freeFilePaths(&paths);

    return result;
}

static inline bool parseAny(const char *fileData, void *data)
{
    return fileData != NULL;
}

static const StorageCallbacks parseAnyCallbacks = {
        .parse = parseAny
};

bool storageLoad(const char* namespace, const char* key, char** value)
{
    bool result = false;

    if (value == NULL)
    {
        icLogError(LOG_TAG, "storageLoad: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    *value = NULL;

    mutexLock(&mtx);

    char *data = NULL;
    if ((result = loadInternalLocked(namespace, key, &data, &parseAnyCallbacks)) == true)
    {
        *value = data;
        data = NULL;
    }
    free(data);

    mutexUnlock(&mtx);

    return result;
}

/**
 * Load a storage item, possibly falling back on backup.
 * If the backup is used, any invalid input is moved to "key.bad," and replaced with the backup.
 * @param namespace
 * @param key
 * @param contentOut If non-null, this will point to the raw file data.
 * @param cb Non-null pointer to a StorageCallbacks that implements parse.
 * @return true when data was loaded
 */
static bool loadInternalLocked(const char *namespace, const char *key, char **contentOut, const StorageCallbacks *cb)
{
    if (cb == NULL || cb->parse == NULL)
    {
        icLogError(LOG_TAG, "%s: No parse callback supplied", __func__);
        return false;
    }

    if (namespace == NULL || key == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __func__);
        return false;
    }

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    fileToRead whichFile = chooseFileToRead(paths.main, paths.backup, NULL);
    const char *filepath = NULL;
    char *data = NULL;
    bool ok = false;

    switch (whichFile)
    {
        case ORIGINAL_FILE:
            filepath = paths.main;
            data = readFileContents(filepath);

            mutexUnlock(&mtx);

            ok = cb->parse(data, cb->parserCtx);

            mutexLock(&mtx);

            if (ok == false)
            {
                icLogWarn(LOG_TAG,
                          "Unable to parse file at %s, attempting to use backup. "
                              "The bad file, if it exists, will be moved to %s",
                          filepath,
                          paths.bad);

                if (rename(paths.main, paths.bad) != 0)
                {
                    if (isIcLogPriorityTrace() == true)
                    {
                        char *errStr = strerrorSafe(errno);
                        icLogTrace(LOG_TAG, "%s: unable to rename %s to %s: %s", __func__, paths.main, paths.bad, errStr);
                        free(errStr);
                    }
                }

                whichFile = BACKUP_FILE;
            }
            break;

        case BACKUP_FILE:
            /* Handled later: ORIGINAL_FILE may have switched to BACKUP_FILE above */
            break;

        case FILE_NOT_PRESENT:
            icLogWarn(LOG_TAG, "No file found for %s/%s", namespace, key);
            break;

        default:
            icLogError(LOG_TAG, "%s: Unsupported file path type [%d]!", __func__, whichFile);
            break;
    }

    if (whichFile == BACKUP_FILE)
    {
        filepath = paths.backup;
        free(data);
        data = readFileContents(filepath);

        mutexUnlock(&mtx);

        ok = cb->parse(data, cb->parserCtx);

        mutexLock(&mtx);

        if (ok == true)
        {
            if (copyFileByPath(filepath, paths.main) == false)
            {
                icLogWarn(LOG_TAG, "Failed to copy restored backup at %s to %s!", filepath, paths.main);
            }

            /* Even if (unlikely) the copy failed, the data was still loaded and is usable */
            icLogInfo(LOG_TAG, "%s/%s restored from backup", namespace, key);
        }
    }

    if (ok == false && whichFile != FILE_NOT_PRESENT)
    {
        icLogError(LOG_TAG,
                   "Unable to parse file for %s/%s (filename %s)!",
                   namespace,
                   key,
                   stringCoalesceAlt(filepath, "(none)"));
    }

    if (contentOut != NULL)
    {
        //coverity[use] data is locally owned heap, underlying file changes do not affect it
        *contentOut = data;
        data = NULL;
    }

    //coverity[use] data is locally owned heap, underlying file changes do not affect it
    free(data);
    freeFilePaths(&paths);

    return ok;
}

bool storageParse(const char *namespace, const char *key, const StorageCallbacks *cb)
{
    pthread_once(&initOnce, oneTimeInit);

    mutexLock(&mtx);

    bool ok = loadInternalLocked(namespace, key, NULL, cb);

    mutexUnlock(&mtx);

    return ok;
}

bool storageParseBad(const char *namespace, const char *key, const StorageCallbacks *cb)
{
    bool ok = false;
    pthread_once(&initOnce, oneTimeInit);

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    /* Closed by copyFile */
    FILE *bad = fopen(paths.bad, "r");
    FILE *restored = NULL;

    /*
     * fopen() doesn't know how to atomically create in exclusive mode, and we don't want to clobber
     * a (unlikely) recently saved file. Whenever parsing fails, the 'main' path is renamed to the 'bad'
     * so a main is never expected when this is called.
     * closed by copyFile via stream
     */
    errno = 0;
    int mainFd = open(paths.main,
                      O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_SYNC,
                      S_IRGRP | S_IWGRP | S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);

    if (mainFd >= 0)
    {
        restored = fdopen(mainFd, "w");
        mainFd = -1;
    }
    else
    {
        scoped_generic char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "%s: Can't open restored main file for writing: %s", __func__, errStr);
    }

    /*
     * Copy, but don't move the bad file. We need a pristine record from
     * when the parser failed for later examination
     */
    if (copyFile(bad, restored) == true)
    {
        mutexLock(&mtx);

        ok = loadInternalLocked(namespace, key, NULL, cb);

        mutexUnlock(&mtx);
    }

    if (ok == false)
    {
        icLogError(LOG_TAG, "Unable to restore '%s'!", key);
    }

    freeFilePaths(&paths);
    return ok;
}

typedef struct XMLContext
{
    xmlDoc *doc;
    const char *encoding;
    const char *docName;
    int parseOptions;
} XMLContext;

typedef struct JSONContext
{
    cJSON *json;
} JSONContext;

static bool parseXML(const char *fileData, void *xmlCtx)
{
    bool xmlIsValid = false;
    XMLContext *xmlContext = xmlCtx;

    if (stringIsEmpty(fileData) == false)
    {
        xmlContext->doc = xmlReadMemory(fileData,
                                        (int) strlen(fileData),
                                        xmlContext->docName,
                                        xmlContext->encoding,
                                        xmlContext->parseOptions);

        xmlIsValid = xmlContext->doc != NULL;
    }

    return xmlIsValid;
}

xmlDoc *storageLoadXML(const char *namespace, const char *key, const char *encoding, int xmlParserOptions)
{
    XMLContext ctx = {
            .doc = NULL,
            .encoding = encoding,
            .docName = key,
            .parseOptions = xmlParserOptions
    };

    const StorageCallbacks xmlParser = {
            .parse = parseXML,
            .parserCtx = &ctx,
    };

    if (storageParse(namespace, key, &xmlParser) == false)
    {
        icLogWarn(LOG_TAG, "%s: %s/%s is not valid XML!", __func__, namespace, key);
    }

    return ctx.doc;
}

static bool parseJSON(const char *fileData, void *jsonCtx)
{
    JSONContext *jsonContext = jsonCtx;

    jsonContext->json = cJSON_Parse(fileData);

    return jsonContext->json != NULL;
}

cJSON *storageLoadJSON(const char *namespace, const char *key)
{
    JSONContext ctx = {
            .json = NULL
    };

    const StorageCallbacks jsonParser = {
            .parse = parseJSON,
            .parserCtx = &ctx
    };

    if (storageParse(namespace, key, &jsonParser) == false)
    {
        icLogWarn(LOG_TAG, "%s: %s/%s is not valid JSON!", __func__, namespace, key);
    }

    return ctx.json;
}

/*
 * delete the main file, its backup, and any temp file
 */
bool storageDelete(const char* namespace, const char* key)
{
    bool result = true;

    if (namespace == NULL || key == NULL)
    {
        icLogError(LOG_TAG, "storageDelete: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    mutexLock(&mtx);

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    if (unlink(paths.main) == -1)
    {
        icLogError(LOG_TAG, "storageDelete: failed to unlink %s: %s", paths.main, strerror(errno));
        result = false;
    }

    //silently ignore any errors deleting temp or backup
    unlink(paths.backup);
    unlink(paths.temp);
    unlink(paths.bad);

    mutexUnlock(&mtx);

    freeFilePaths(&paths);
    return result;
}

bool storageDeleteNamespace(const char* namespace)
{
    bool result = true;

    if (namespace == NULL)
    {
        icLogError(LOG_TAG, "storageDeleteNamespace: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    char* path = getNamespacePath(namespace);
    mutexLock(&mtx);
    result = deleteDirectory(path);
    mutexUnlock(&mtx);
    free(path);
    return result;
}

/**
 * Checks for the existance of a file in this namespace and this key
 *
 * @param namespace
 * @param key
 * @return the if the key is the ORIGINAL_FILE, BACKUP_FILE, or missing
 */
fileToRead storageHasKey(const char* namespace, const char *key)
{
    // determine the location of where the file would be
    //
    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    // see if this key (or it's backup) exists
    //
    fileToRead whichFile = chooseFileToRead(paths.main, paths.backup, NULL);

    // cleanup and return
    freeFilePaths(&paths);
    return whichFile;
}

icLinkedList* storageGetKeys(const char* namespace)
{
    if (namespace == NULL)
    {
        icLogError(LOG_TAG, "storageGetKeys: invalid args");
        return NULL;
    }

    pthread_once(&initOnce, oneTimeInit);

    icLinkedList* result = NULL;
    char* path = getNamespacePath(namespace);

    DIR* dir;
    struct dirent *entry;

    mutexLock(&mtx);
    if ((dir = opendir(path)) == NULL)
    {
        icLogDebug(LOG_TAG, "storageGetKeys: failed to open namespace directory %s: %s", path, strerror(errno));
    }
    else
    {
        result = linkedListCreate();

        icStringHashMap* regularFiles = stringHashMapCreate();
        icStringHashMap* bakFiles = stringHashMapCreate();

        while((entry = readdir(dir)) != NULL)
        {
            if (entry->d_type != DT_DIR)
            {
                //our list of keys will be all files that are not .bak, .bad or .tmp PLUS any .bak files that are
                // missing their regular entry
                //dont add our backup or temp files
                if (strcmp(entry->d_name, STORAGE_KEY_FILE_NAME) == 0)
                {
                    icLogDebug(LOG_TAG, "Skipping storage key file");
                    continue;
                }
                size_t nameLen = strlen(entry->d_name);
                if(nameLen > 4) //the only ones that could have a .bak, .bad, or .tmp extension
                {
                    char* extension = entry->d_name + (nameLen - 4);
                    if(strcmp(extension, ".bak") == 0)
                    {
                        stringHashMapPut(bakFiles, strdup(entry->d_name), NULL);
                    }
                    else if(strcmp(extension, ".tmp") != 0 && strcmp(extension, ".bad") != 0)
                    {
                        stringHashMapPut(regularFiles, strdup(entry->d_name), NULL);
                    }
                }
                else //it was too short to be one of the files we are trying to filter... just add it
                {
                    stringHashMapPut(regularFiles, strdup(entry->d_name), NULL);
                }
            }
        }

        //now we have a list of regular files (keys) and .bak files.  If we have a .bak file for which there is no
        // regular file, add it to our results
        icStringHashMapIterator* it = stringHashMapIteratorCreate(bakFiles);
        while(stringHashMapIteratorHasNext(it))
        {
            char* key;
            char* value;
            if(stringHashMapIteratorGetNext(it, &key, &value))
            {
                //truncate the .bak file at the dot
                key[strlen(key) - 4] = '\0';
                if(stringHashMapContains(regularFiles, key) == false)
                {
                    stringHashMapPut(regularFiles, strdup(key), NULL);
                }
            }
        }
        stringHashMapIteratorDestroy(it);

        //now collect our final results from regularFiles
        it = stringHashMapIteratorCreate(regularFiles);
        while(stringHashMapIteratorHasNext(it))
        {
            char* key;
            char* value;
            if(stringHashMapIteratorGetNext(it, &key, &value))
            {
                linkedListAppend(result, strdup(key));
            }
        }
        stringHashMapIteratorDestroy(it);

        stringHashMapDestroy(regularFiles, NULL);
        stringHashMapDestroy(bakFiles, NULL);
        closedir(dir);
    }
    mutexUnlock(&mtx);

    free(path);
    return result;
}

StorageRestoreErrorCode storageRestoreNamespace(const char* namespace, const char* basePath)
{
    // Assume success, set in failure cases
    StorageRestoreErrorCode ret = STORAGE_RESTORE_ERROR_NONE;

    struct stat sinfo;

    char* restorePath = stringBuilder("%s/%s/%s", basePath, STORAGE_DIR, namespace);

    pthread_once(&initOnce, oneTimeInit);

    if (doesDirExist(restorePath) == true)
    {
        bool pathExists = false;
        char* configPath = getNamespacePath(namespace);

        mutexLock(&mtx);
        if (stat(configPath, &sinfo) == 0)
        {
            if (!deleteDirectory(configPath))
            {
                /*
                 * Avoiding multiple return points. Just set a flag
                 * that we were not able to remove the existing namespace.
                 */
                ret = STORAGE_RESTORE_ERROR_OLD_CONFIG_DELETE;
                pathExists = true;
                icLogError(LOG_TAG, "storageRestoreNamespace: failed to delete namespace directory %s: %s",
                           configPath, strerror(errno));
            }
        }

        /*
         * Only copy over the namespace if the new location
         * does not exist. If we allowed the namespace to be
         * copied while already existing we could pollute
         * the namespace.
         */
        if (!pathExists)
        {
            if (!copyDirectory(restorePath, configPath))
            {
                ret = STORAGE_RESTORE_ERROR_FAILED_COPY;
                icLogError(LOG_TAG, "storageRestoreNamespace: failed to copy namespace directory %s -> %s: %s",
                           restorePath, configPath, strerror(errno));
            }
        }
        else
        {
            icLogError(LOG_TAG, "storageRestoreNamespace: failed to create namespace directory %s: %s",
                       configPath, strerror(errno));
        }
        mutexUnlock(&mtx);

        free(configPath);
    }
    else
    {
        ret = STORAGE_RESTORE_ERROR_NEW_DIR_MISSING;
        icLogDebug(LOG_TAG, "storageRestoreNamespace: failed to find namespace directory %s to restore",
                   restorePath);
    }

    free(restorePath);

    return ret;
}

const char *getStorageDir(void)
{
    return STORAGE_DIR;
}

bool storageGetMtime(const char *namespace, const char *key, struct timespec *mtime)
{
    if (mtime == NULL || namespace == NULL || key == NULL)
    {
        return false;
    }

    bool ok = false;

    pthread_once(&initOnce, oneTimeInit);

    mutexLock(&mtx);

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    struct stat fileInfo;
    errno = 0;
    if (stat(paths.main, &fileInfo) == 0)
    {
        ok = true;
#ifdef CONFIG_OS_DARWIN
        *mtime = fileInfo.st_mtimespec;
#else
        *mtime = fileInfo.st_mtim;
#endif
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *error = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "Cannot stat %s/%s: %s", namespace, key, error);
    }

    mutexUnlock(&mtx);

    return ok;
}
