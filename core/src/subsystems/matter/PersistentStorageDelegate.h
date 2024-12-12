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
