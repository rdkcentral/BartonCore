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

#ifndef ZILKER_PERSISTENTSTORAGEDELEGATE_H
#define ZILKER_PERSISTENTSTORAGEDELEGATE_H

#define CHIP_HAVE_CONFIG_H 1

#include <lib/core/CHIPPersistentStorageDelegate.h>

namespace zilker
{

    /**
     * Zilker's implementation of CHIP's persistent storage.  Storing stuff in our dynamic config dir.
     */
    class PersistentStorageDelegate : public chip::PersistentStorageDelegate
    {
    public:
        PersistentStorageDelegate() = default;
        ~PersistentStorageDelegate() = default;

        // These are from the PersistentStorageDelegate interface
        CHIP_ERROR SyncGetKeyValue(const char *key, void *buffer, uint16_t &size) override;
        CHIP_ERROR SyncSetKeyValue(const char *key, const void *value, uint16_t size) override;
        CHIP_ERROR SyncDeleteKeyValue(const char *key) override;

    private:
        static char *scrubKey(const char *key); // caller must free
    };
} // namespace zilker

#endif // ZILKER_PERSISTENTSTORAGEDELEGATE_H
