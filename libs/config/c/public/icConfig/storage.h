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

//
// Created by tlea on 4/4/18.
//

/**
 * This is a simple storage library for key value string pairs organized into a 'namespace'.
 * An example namespace might be 'deviceService'.  A key might be the EUI64 of a ZigBee device
 * and its value might be the JSON representation of the device and all of its settings.
 */

#pragma once

#include <cjson/cJSON.h>
#include <icConfig/backupUtils.h>
#include <icTypes/icLinkedList.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <sys/time.h>

#define HAVE_STORAGE_SET_CONFIG_PATH 1
void storageSetConfigPath(const char *configPath);

/**
 * Save a value for a key under a namespace.
 *
 * @param ns
 * @param key
 * @param value
 * @return true on success
 */
bool storageSave(const char *ns, const char *key, const char *value);

/**
 * Load a value for a key under a namespace.
 *
 * @param ns
 * @param key
 * @param value the retrieved value, if any.  Caller frees.
 * @return true on success
 */
bool storageLoad(const char *ns, const char *key, char **value);

/**
 * Try to load valid XML from storage.
 * @param ns
 * @param key
 * @param encoding the document encoding or NULL
 * @param xmlParserOptions A set of logically OR'd xmlParserOptions or 0 for defaults
 * @return a valid xmlDoc, or NULL on failure. Free with xmlFreeDoc when finished.
 */
xmlDoc *storageLoadXML(const char *ns, const char *key, const char *encoding, int xmlParserOptions);

/**
 * Try to load valid JSON from storage.
 * @param ns
 * @param key
 * @return a valid cJSON document, or NULL on failure. Free with cJSON_Delete when finished.
 */
cJSON *storageLoadJSON(const char *ns, const char *key);


typedef struct StorageCallbacks
{
    /**
     * Any context data needed by parse()
     */
    void *parserCtx;

    /**
     * A custom parser for reading and validating storage data.
     * @param fileContents
     * @param parserCtx
     * @return true if fileContents represents valid data.
     */
    bool (*parse)(const char *fileContents, void *parserCtx);
} StorageCallbacks;

/**
 * Load valid data from storage with a custom parser/validator.
 * @param ns
 * @param key
 * @param cb A non-null StorageCallbacks that describes your parser.
 * @return true if any valid data was loaded.
 * @see storageLoadJSON and storageLoadXML if you only want basic document validation.
 */
bool storageParse(const char *ns, const char *key, const StorageCallbacks *cb);

/**
 * Attempt to parse a key after storageParse() fails, e.g., with a more forgiving data model in the parse callback
 * @see storageParse
 * @return TRUE if the key could be restored
 */
bool storageParseBad(const char *ns, const char *key, const StorageCallbacks *cb);

/**
 * Delete a key from a namespace.
 *
 * @param ns
 * @param key
 * @return true on success
 */
bool storageDelete(const char *ns, const char *key);

/**
 * Delete a namespace and all of the keys stored within it.
 *
 * NOTE: this will remove other content added under the namespace out-of-band from this library
 *
 * @param ns
 * @return true on success
 */
bool storageDeleteNamespace(const char *ns);

typedef enum StorageRestoreErrorCode
{
    STORAGE_RESTORE_ERROR_NONE,              // Restore succeeded
    STORAGE_RESTORE_ERROR_NEW_DIR_MISSING,   // The configuration directory to restore to does not exist
    STORAGE_RESTORE_ERROR_OLD_CONFIG_DELETE, // The current configuration directory could not be deleted
    STORAGE_RESTORE_ERROR_FAILED_COPY        // The new configuration could not be copied to the current configuration
                                             // directory
} StorageRestoreErrorCode;


static const char *const StorageRestoreErrorLabels[] = {"STORAGE_RESTORE_ERROR_NONE",
                                                        "STORAGE_RESTORE_ERROR_NEW_DIR_MISSING",
                                                        "STORAGE_RESTORE_ERROR_OLD_CONFIG_DELETE",
                                                        "STORAGE_RESTORE_ERROR_FAILED_COPY"};

const char *storageRestoreErrorDescribe(StorageRestoreErrorCode code);

/**
 * Restore a storage namespace from a backed up
 * storage location.
 *
 * All content within the existing namespace (if any)
 * will be destroyed.
 *
 * @param ns The namespace to restore.
 * @param basePath The base configuration path without any storage modifiers.
 * @return STORAGE_RESTORE_ERROR_NONE on success
 *         STORAGE_RESTORE_ERROR_NEW_DIR_MISSING if the configuration directory to restore to does not exist
 *         STORAGE_RESTORE_ERROR_OLD_CONFIG_DELETE if the current configuration directory could not be deleted
 *         STORAGE_RESTORE_ERROR_FAILED_COPY if the new configuration could not be copied to the current configuration
 * directory
 */
StorageRestoreErrorCode storageRestoreNamespace(const char *ns, const char *basePath);

/**
 * Checks for the existance of a file in this namespace and this key
 *
 * @param ns
 * @param key
 * @return the if the key is the ORIGINAL_FILE, BACKUP_FILE, or missing
 */
fileToRead storageHasKey(const char *ns, const char *key);

/**
 * Retrieve a list of all the keys under a namespace.
 *
 * @param ns
 * @return the list of keys or NULL on failure
 */
icLinkedList *storageGetKeys(const char *ns);

/**
 * Retrieve the name of the storage directory within the dynamic config directory
 * @return the storage dir name, caller should NOT free this string
 */
const char *getStorageDir(void);

/**
 * Retrieve the last modification date for the key.
 * @note only the key's main entry is loaded. Invoke @refitem storageLoad first
 *       for best results.
 * @param ns
 * @param key
 * @return true when the item's modification date is avaialable.
 */
bool storageGetMtime(const char *ns, const char *key, struct timespec *mtime);
