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
 * loggerAndroid.c
 *
 * Android implementation of the logging.h facade
 * Will be built and included based on #define values
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

// look at buildtime settings to see if this log implementation should be enabled

#ifdef CONFIG_LIB_LOG_LOGCAT

#include "loggingCommon.h"
#include <android/log.h>
#include <icLog/logging.h>
#include <memory.h>
#include <stdarg.h>

#define LOG_CHUNK_SIZE 1023

static char chunkBuffer[LOG_CHUNK_SIZE + 1] = "";

/*
 * initialize the logger
 */
__attribute__((constructor)) static void initIcLogger(void)
{
    // nothing to do for Android
    //
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
    android_LogPriority logPriority = ANDROID_LOG_DEBUG;

    if (!shouldLogMessage(priority))
    {
        return;
    }

    // map priority to Android syntax
    //
    switch (priority)
    {
        case IC_LOG_TRACE: // no TRACE, so use VERBOSE
            logPriority = ANDROID_LOG_VERBOSE;
            break;

        case IC_LOG_DEBUG:
            logPriority = ANDROID_LOG_DEBUG;
            break;

        case IC_LOG_INFO:
            logPriority = ANDROID_LOG_INFO;
            break;

        case IC_LOG_WARN:
            logPriority = ANDROID_LOG_WARN;
            break;

        case IC_LOG_ERROR:
            logPriority = ANDROID_LOG_ERROR;
            break;

        default:
            break;
    }

    // get the size of the log message
    //
    va_start(arglist, format);
    char *formattedMessage = NULL;
    int formattedMessageLen = vsnprintf(formattedMessage, 0, format, arglist);
    va_end(arglist);

    // Print the formatted message into the buffer
    //
    formattedMessage = calloc(1, formattedMessageLen + 1); // add 1 for the null byte
    va_start(arglist, format);
    vsnprintf(formattedMessage, formattedMessageLen + 1, format, arglist);
    va_end(arglist);

    // determine how many times we need to split it up, if there is a remainder, then add one to account for it
    //
    int numChunks = formattedMessageLen / LOG_CHUNK_SIZE;
    if (formattedMessageLen % LOG_CHUNK_SIZE != 0)
    {
        numChunks++;
    }
    char *ptrLocation = formattedMessage;
    for (int chunk = 0; chunk < numChunks; chunk++)
    {
        if (strlen(ptrLocation) > LOG_CHUNK_SIZE)
        {
            memcpy(chunkBuffer, ptrLocation, LOG_CHUNK_SIZE);
            chunkBuffer[LOG_CHUNK_SIZE] = '\0';

            __android_log_write(logPriority, categoryName, chunkBuffer);

            ptrLocation = ptrLocation + LOG_CHUNK_SIZE; // Move down the message in LOG_CHUNK_SIZE increments
        }
        else
        {
            __android_log_write(logPriority, categoryName, ptrLocation);
        }
    }
    free(formattedMessage);
}

#endif /* CONFIG_LIB_LOG_LOGCAT */
