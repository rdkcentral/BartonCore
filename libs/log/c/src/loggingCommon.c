//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast Corporation
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
// Created by dcalde202 on 2/12/19.
//

#include <pthread.h>
#include <stdbool.h>

#ifdef CONFIG_LIB_GLIB
#include <glib.h>
#endif

#include <icLog/logging.h>

static pthread_mutex_t LOG_MTX = PTHREAD_MUTEX_INITIALIZER;
static logPriority logLevel = IC_LOG_DEBUG;

/*
 * return the system-level logging priority setting, which dictates
 * what is actually sent to the log and what is ignored
 */
logPriority getIcLogPriorityFilter(void)
{
    logPriority retVal = IC_LOG_DEBUG;

    pthread_mutex_lock(&LOG_MTX);
    retVal = logLevel;
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * set the system-level logging priority setting, allowing messages
 * to be sent or filtered out
 */
void setIcLogPriorityFilter(logPriority priority)
{
    pthread_mutex_lock(&LOG_MTX);
    logLevel = priority;
    pthread_mutex_unlock(&LOG_MTX);
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_TRACE' messages
 */
bool isIcLogPriorityTrace(void)
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel == IC_LOG_TRACE)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_DEBUG' messages
 */
bool isIcLogPriorityDebug(void)
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_DEBUG)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_INFO' messages
 */
bool isIcLogPriorityInfo(void)
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_INFO)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_WARN' messages
 */
bool isIcLogPriorityWarn(void)
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_WARN)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_ERROR' messages
 */
bool isIcLogPriorityError(void)
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_ERROR)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

bool shouldLogMessage(logPriority priority)
{
    pthread_mutex_lock(&LOG_MTX);
    bool retval = true;

    if (logLevel > priority)
    {
        retval = false;
    }

    pthread_mutex_unlock(&LOG_MTX);
    return retval;
}

#ifdef CONFIG_LIB_GLIB

#define GLIB_LOG "glib"
static void glibLog(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    switch (log_level)
    {
        case G_LOG_LEVEL_ERROR:
        case G_LOG_LEVEL_CRITICAL:
            icLogError(GLIB_LOG, "%s: %s", log_domain, message);
            break;

        case G_LOG_LEVEL_WARNING:
            icLogWarn(GLIB_LOG, "%s: %s", log_domain, message);
            break;

        case G_LOG_LEVEL_MESSAGE:
        case G_LOG_LEVEL_INFO:
            icLogInfo(GLIB_LOG, "%s: %s", log_domain, message);
            break;

        case G_LOG_LEVEL_DEBUG:
            icLogDebug(GLIB_LOG, "%s: %s", log_domain, message);
            break;

        default:
            icLogWarn(GLIB_LOG, "[Unrecognized log level %#x] %s: %s", log_level, log_domain, message);
            break;
    }
}

#endif

__attribute__((constructor)) static void initCommon(void)
{
#ifdef CONFIG_LIB_GLIB
    g_log_set_default_handler(glibLog, NULL);
#endif
}
