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
 * loggerZlog.c
 *
 * zlog implementation of the logging.h facade.
 * Will be built and included based on #define values
 *
 * Author: tlea - 11/5/15
 *-----------------------------------------------*/

// look at buildtime settings to see if this log implementation should be enabled

#ifdef CONFIG_LIB_LOG_ZLOG

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#include <icLog/logging.h>
#include <zlog.h>

#ifdef CONFIG_TELEMETRY2
#include <telemetry_busmessage_sender.h>
#endif


#include "loggingCommon.h"

/*
 * initialize the logger
 */
__attribute__((constructor)) static void initIcLogger(void)
{
    char *confPath;
    char *ichome = getenv("IC_HOME");
    if (ichome != NULL)
    {
        confPath = (char *) calloc(1, strlen(ichome) + strlen("/etc/zlog.conf") + 1);
        sprintf(confPath, "%s/etc/zlog.conf", ichome);
    }
    else
    {
        // use default: /opt/icontrol/etc
        //
        confPath = (char *) calloc(1, strlen(CONFIG_STATIC_PATH) + strlen("/etc/zlog.conf") + 1);
        sprintf(confPath, "%s/etc/zlog.conf", CONFIG_STATIC_PATH);
    }

    struct stat fs;
    if (stat(confPath, &fs) == 0 &&
        fs.st_size > 5) // arbitrarily small amount of content before we consider it a real file and try to load it
    {
        // config file exists, safe to load
        //
        if (zlog_init(confPath))
        {
            fprintf(stderr, "zlog initialization failed.  confPath = %s\n", confPath);
        }
    }
    else
    {
        fprintf(stderr, "zlog file missing - %s\n", confPath);
    }

    free(confPath);
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
    int logPriority = ZLOG_LEVEL_DEBUG;

    if (!shouldLogMessage(priority))
    {
        return;
    }

    // map priority to zlog syntax
    //
    switch (priority)
    {
        case IC_LOG_TRACE: // no TRACE, so use DEBUG
        case IC_LOG_DEBUG:
            logPriority = ZLOG_LEVEL_DEBUG;
            break;

        case IC_LOG_INFO:
            logPriority = ZLOG_LEVEL_INFO;
            break;

        case IC_LOG_WARN:
            logPriority = ZLOG_LEVEL_WARN;
            break;

        case IC_LOG_ERROR:
            logPriority = ZLOG_LEVEL_ERROR;
            break;

        default:
            break;
    }

    zlog_category_t *category = zlog_get_category(categoryName);

    // preprocess the variable args format, then forward to zlog
    //
    va_start(arglist, format);
    vzlog(category,
          __FILE__,
          sizeof(__FILE__) - 1,
          __func__,
          sizeof(__func__) - 1,
          __LINE__,
          logPriority,
          format,
          arglist);
    va_end(arglist);
}

#ifdef CONFIG_TELEMETRY2
/*
 * Issue telemetry counter to T2.0
 */
void icTelemetryCounter(char *marker)
{
    t2_event_d(marker, 1);
}

/*
 * Issue telemetry string value to T2.0
 */
void icTelemetryStringValue(char *marker, char *value)
{
    t2_event_s(marker, value);
}

/*
 * Issue a telemetry number value to T2.0
 */
void icTelemetryNumberValue(char *marker, double value)
{
    t2_event_f(marker, value);
}
#endif /* CONFIG_TELEMETRY2 */

#endif /* CONFIG_LIB_LOG_ZLOG */
