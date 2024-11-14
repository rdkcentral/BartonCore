// Copyright (C) 2024 Comcast Corporation
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

namespace zilker
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
} // namespace zilker
