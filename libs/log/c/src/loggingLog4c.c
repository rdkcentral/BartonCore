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
 * loggerLog4c.c
 *
 * Log4c implementation of the logging.h facade.
 * Will be built and included based on #define values
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

// look at buildtime settings to see if this log implementation should be enabled

#ifdef CONFIG_LIB_LOG_LOG4C

#include <stdarg.h>

#include <icLog/logging.h>
#include <log4c.h>

#include "loggingCommon.h"

/*
 * initialize the logger
 */
__attribute__((constructor)) static void initIcLogger(void)
{
    log4c_init();
}

/*
 * Issue logging message based on a 'categoryName' and 'priority'
 */
void icLogMsg(const char *file,
              size_t filelen,
              const char *func,
              size_t funclen,
              long line,
              const char *categoryName,
              logPriority priority,
              const char *format,
              ...)
{
    va_list arglist;
    log4c_priority_level_t logPriority = LOG4C_PRIORITY_DEBUG;
    log4c_category_t *logcat = NULL;

    if (!shouldLogMessage(priority))
    {
        return;
    }

    // map priority to Log4c syntax
    //
    switch (priority)
    {
        case IC_LOG_TRACE:
            logPriority = LOG4C_PRIORITY_TRACE;
            break;

        case IC_LOG_DEBUG:
            logPriority = LOG4C_PRIORITY_DEBUG;
            break;

        case IC_LOG_INFO:
            logPriority = LOG4C_PRIORITY_INFO;
            break;

        case IC_LOG_WARN:
            logPriority = LOG4C_PRIORITY_WARN;
            break;

        case IC_LOG_ERROR:
            logPriority = LOG4C_PRIORITY_ERROR;
            break;

        default:
            break;
    }

    // get category for name
    //
    logcat = log4c_category_get(categoryName);

    // preprocess the variable args format, then forward to Log4c
    //
    va_start(arglist, format);
    log4c_category_vlog(logcat, logPriority, format, arglist);
    va_end(arglist);
}

#endif /* CONFIG_LIB_LOG_LOG4C */
