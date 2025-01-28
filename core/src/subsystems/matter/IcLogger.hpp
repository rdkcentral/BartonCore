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

#pragma once

#include <stdint.h>
#include <string>
#include <sys/types.h>

#include "lib/support/logging/CHIPLogging.h"
#include "lib/support/logging/Constants.h"

extern "C" {
#include "glib.h"
#include "glib/gprintf.h"
#include "icLog/logging.h"
#include "icTypes/sbrm.h"
}

namespace barton
{
    class IcLogger
    {
    public:
        IcLogger()
        {
            chip::Logging::SetLogRedirectCallback(OnLogMessage);

            uint8_t matterCategory = chip::Logging::kLogCategory_None;

            /*
             * This doesn't care about unlikely dynamic property changes. Its only
             * purpose is to prevent wastefully generating messages in the Matter SDK
             * that would immediately be discarded in low verbosity configuraitons.
             * When increasing verbosity dynamically, restart the process to apply.
             */

            switch (getIcLogPriorityFilter())
            {
                case IC_LOG_NONE:
                    break;

                case IC_LOG_ERROR:
                    // fallthrough - matter doesn't have distinct error/warn cats
                case IC_LOG_WARN:
                    // fallthrough - matter doesn't align cleanly with 'info' level
                case IC_LOG_INFO:
                    matterCategory = chip::Logging::kLogCategory_Error;
                    break;

                case IC_LOG_DEBUG:
                    matterCategory = chip::Logging::kLogCategory_Progress;
                    break;

                case IC_LOG_TRACE:
                    matterCategory = chip::Logging::kLogCategory_Max;
                    break;

                default:
                    break;
            }

            chip::Logging::SetLogFilter(matterCategory);
        }

    private:
        static char *FormatMatterLog(const char *msg, va_list args)
        {
            scoped_generic char *tmp = NULL;

            if (g_vasprintf(&tmp, msg, args) == -1)
            {
                return NULL;
            }

            return (char *) g_steal_pointer(&tmp);
        }

        static void OnLogMessage(const char *module, uint8_t category, const char *msg, va_list args);
    };
} // namespace barton
