//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * loggerDebug.c
 *
 * Development implementation of the logging.h facade
 * to print all logging messages to STDOUT.  Like the
 * other implementations, built and included based on
 * #define values
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <icLog/logging.h>
#include <pthread.h>
#include <sys/time.h>

#include "loggingCommon.h"
#include <glib.h>


static char *getTimeString(void);

/*
 * initialize the logger
 */
__attribute__((constructor)) static void initIcLogger(void)
{
    // uses STDOUT, so nothing to open
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

    // skip if priority is > logLevel
    //
    if (!shouldLogMessage(priority))
    {
        return;
    }

    // print the cat name, priority, then the message
    // NOTE: trying to keep format the same as zlog.conf
    //
    g_autofree char *timeStr = getTimeString();
    g_autofree char *catPidStr = g_strdup_printf("[%s %d] - ", categoryName, getpid());

    const char *priorityStr = NULL;
    // map priority to Android syntax
    //
    switch (priority)
    {
        case IC_LOG_TRACE:
            priorityStr = "TRACE: ";
            break;

        case IC_LOG_DEBUG:
            priorityStr = "DEBUG: ";
            break;

        case IC_LOG_INFO:
            priorityStr = "INFO: ";
            break;

        case IC_LOG_WARN:
            priorityStr = "WARN: ";
            break;

        case IC_LOG_ERROR:
            priorityStr = "ERROR: ";
            break;

        default:
            priorityStr = "???: ";
            break;
    }

    // preprocess the variable args format, then forward to STDOUT
    //
    va_start(arglist, format);
    g_autofree char *logOut = g_strdup_vprintf(format, arglist);
    va_end(arglist);

    printf("%s%s%s%s\n", timeStr, catPidStr, priorityStr, logOut);

    if (fflush(stdout) == EOF)
    {
        // Output is gone, prevent an infinite cycle when logging any SIGPIPEs
        setIcLogPriorityFilter(IC_LOG_NONE);
    }
}

static char *getTimeString(void)
{
    char buff[32];
    struct timeval now;
    struct tm ptr;

    // get local time & millis
    //
    gettimeofday(&now, NULL);
    localtime_r(&now.tv_sec, &ptr);

    // pretty-print the time
    //
    strftime(buff, 31, "%Y-%m-%d %H:%M:%S", &ptr);

    // add millis to the end of the formatted string
    //
    return g_strdup_printf("%s.%03ld : ", buff, (long) (now.tv_usec / 1000));
}
