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

/*
 * Created by Thomas Lea on 3/9/21.
 */

#define LOG_TAG     "MatterStorage"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include <libxml/parser.h>

extern "C" {
#include <icConfig/storage.h>
#include <icLog/logging.h>
#include <icUtil/base64.h>
#include <icUtil/stringUtils.h>
}

#include "PersistentStorageDelegate.h"
#include <cstring>
#include <platform/KeyValueStoreManager.h>
#include <system/SystemConfig.h>

using namespace chip::DeviceLayer::PersistedStorage;

#define PASS_THROUGH
#define STORAGE_NAMESPACE "devicedb/matter"

namespace zilker
{
    // synchronous: get a byte buffer value
    CHIP_ERROR PersistentStorageDelegate::SyncGetKeyValue(const char *key, void *buffer, uint16_t &size)
    {
        CHIP_ERROR err = CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;

        icDebug("key = %s", stringCoalesce(key));

#ifndef PASS_THROUGH
        scoped_generic char *storedVal = nullptr;
        scoped_generic char *scrubbedKey = scrubKey(key);
        if (storageLoad(STORAGE_NAMESPACE, scrubbedKey, &storedVal))
        {
            // base64 decode what was stored
            uint8_t *localBuf = nullptr;
            uint16_t localBufLen = 0;
            if (icDecodeBase64(storedVal, &localBuf, &localBufLen))
            {
                if (localBufLen <= size)
                {
                    memcpy(buffer, localBuf, localBufLen);
                    size = localBufLen;
                    err = CHIP_NO_ERROR;
                }
                else
                {
                    icError("destination too small for key %s", key);
                    err = CHIP_ERROR_BUFFER_TOO_SMALL;
                }
            }
            else
            {
                icError("unable to base64 decode key %s", key);
                err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;
            }

            free(localBuf);
        }

#else
        if (key == nullptr)
        {
            return CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
        }

        size_t bytesRead = 0;
        err = chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Get(key, buffer, size, &bytesRead);
        if (err == CHIP_NO_ERROR)
        {
            // Update state only if KeyValueStoreMgr().Get() succeeded. Otherwise we may get 'Incorrect state' errors
            // later
            size = static_cast<uint16_t>(bytesRead);
        }

#endif
        return err;
    }

    // synchronous: set a byte buffer value
    CHIP_ERROR PersistentStorageDelegate::SyncSetKeyValue(const char *key, const void *value, uint16_t size)
    {
        CHIP_ERROR err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;

        icDebug("key = %s", stringCoalesce(key));

        if (key == nullptr || value == nullptr || size == 0)
        {
            return CHIP_ERROR_INVALID_ARGUMENT;
        }

#ifndef PASS_THROUGH
        scoped_generic char *encodedValue = icEncodeBase64((uint8_t *) value, size);
        if (encodedValue != nullptr)
        {
            scoped_generic char *scrubbedKey = scrubKey(key);
            if (storageSave(STORAGE_NAMESPACE, scrubbedKey, encodedValue))
            {
                err = CHIP_NO_ERROR;
            }
            else
            {
                icError("failed to save key %s!", key);
            }
        }
#else
        err = chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Put(key, value, size);
#endif
        return err;
    }

    // synchronous: delete a key
    CHIP_ERROR PersistentStorageDelegate::SyncDeleteKeyValue(const char *key)
    {
        CHIP_ERROR err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;

        icDebug("key = %s", stringCoalesce(key));

        if (key != nullptr)
        {
#ifndef PASS_THROUGH
            scoped_generic char *scrubbedKey = scrubKey(key);
            if (storageDelete(STORAGE_NAMESPACE, scrubbedKey))
            {
                err = CHIP_NO_ERROR;
            }
            {
                icError("failed to delete key %s!", key);
            }
#else
            err = chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr().Delete(key);
#endif
        }

        return err;
    }

    // TODO ditch this and make storage work with keys that have slashes.
    char *PersistentStorageDelegate::scrubKey(const char *key)
    {
        char *result = strdup(key);
        char *pos = strchr(result, '/');
        while (pos != nullptr)
        {
            *pos = '_';
            pos = strchr(pos, '/');
        }

        return result;
    }
} // namespace zilker
