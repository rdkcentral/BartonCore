//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
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
#include <matter/platform/KeyValueStoreManager.h>
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
