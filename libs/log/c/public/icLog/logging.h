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
 * logger.h
 *
 * Facade of a common interface for logging.  Allows us
 * to supply different runtime implementations based
 * on the environment (ex: android, log4c, etc)
 * During runtime, the appropriate underlying logging
 * mechanism will be available and loaded.
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_LOGGING_H
#define IC_LOGGING_H

#include <stdlib.h>
#include <stdbool.h>

/*
 * define the log priorities
 */
typedef enum _logPriority
{
    IC_LOG_TRACE = 0,
    IC_LOG_DEBUG = 1,
    IC_LOG_INFO  = 2,
    IC_LOG_WARN  = 3,
    IC_LOG_ERROR = 4,
    IC_LOG_NONE  = 5 //disables all log output
} logPriority;

/*
 * return the system-level logging priority setting, which dictates
 * what is actually sent to the log and what is ignored
 */
logPriority getIcLogPriorityFilter(void);

/*
 * set the system-level logging priority setting, allowing messages
 * to be sent or filtered out
 */
void setIcLogPriorityFilter(logPriority priority);

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_TRACE' messages
 */
bool isIcLogPriorityTrace(void);

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_DEBUG' messages
 */
bool isIcLogPriorityDebug(void);

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_INFO' messages
 */
bool isIcLogPriorityInfo(void);

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_WARN' messages
 */
bool isIcLogPriorityWarn(void);

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_ERROR' messages
 */
bool isIcLogPriorityError(void);


/*
 * Issue logging message based on a 'categoryName' and 'priority'
 */
__attribute__ ((format (__printf__, 8, 9)))
void icLogMsg(const char *file,
              size_t filelen,
              const char *func,
              size_t funclen,
              long line,
              const char *categoryName,
              logPriority priority, const char *format, ...);

/*
 * Issue trace log message for 'categoryName'
 */
#define icLogTrace(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_TRACE, __VA_ARGS__)

/*
 * Issue debug log message for 'categoryName'
 */
#define icLogDebug(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_DEBUG, __VA_ARGS__)

/*
 * Issue informative log message for 'categoryName'
 */
#define icLogInfo(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_INFO, __VA_ARGS__)

/*
 * Issue warning log message for 'categoryName'
 */
#define icLogWarn(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_WARN, __VA_ARGS__)

/*
 * Issue error log message for 'categoryName'
 */
#define icLogError(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_ERROR, __VA_ARGS__)


/**
 * Log a trace message with a provided formatter.
 * To use this, #define a function like macro named logFmt, and a LOG_TAG macro before #including this header.
 * The formatter macro may produce any valid format string, followed by its arguments, in order.
 * @example #define logFmt(fmt) "%s: " fmt, __func__ will prepend the calling function to the log message.
 */
#define icTrace(fmt, ...) icLogTrace(LOG_TAG, logFmt(fmt), ##__VA_ARGS__)

/**
 * Log a debug message with a provided formatter.
 * To use this, #define a function like macro named logFmt, and a LOG_TAG macro before #including this header.
 * The formatter macro may produce any valid format string, followed by its arguments, in order.
 * @example #define logFmt(fmt) "%s: " fmt, __func__ will prepend the calling function to the log message.
 */
#define icDebug(fmt, ...) icLogDebug(LOG_TAG, logFmt(fmt), ##__VA_ARGS__)

/**
 * Log an informational message with a provided formatter.
 * To use this, #define a function like macro named logFmt, and a LOG_TAG macro before #including this header.
 * The formatter macro may produce any valid format string, followed by its arguments, in order.
 * @example #define logFmt(fmt) "%s: " fmt, __func__ will prepend the calling function to the log message.
 */
#define icInfo(fmt, ...) icLogInfo(LOG_TAG, logFmt(fmt), ##__VA_ARGS__)

/**
 * Log a warning message with a provided formatter.
 * To use this, #define a function like macro named logFmt, and a LOG_TAG macro before #including this header.
 * The formatter macro may produce any valid format string, followed by its arguments, in order.
 * @example #define logFmt(fmt) "%s: " fmt, __func__ will prepend the calling function to the log message.
 */
#define icWarn(fmt, ...) icLogWarn(LOG_TAG, logFmt(fmt), ##__VA_ARGS__)

/**
 * Log an error message with a provided formatter.
 * To use this, #define a function like macro named logFmt, and a LOG_TAG macro before #including this header.
 * The formatter macro may produce any valid format string, followed by its arguments, in order.
 * @example #define logFmt(fmt) "%s: " fmt, __func__ will prepend the calling function to the log message.
 */
#define icError(fmt, ...) icLogError(LOG_TAG, logFmt(fmt), ##__VA_ARGS__)

// Implementation details for Telemetry 2.0 for ZLOG
// CONFIG_RDK_PLATFORM uses ZLOG, so T2.0 was only implemented
// in ZLOG.  Other loggers have a no-op macro for compilation purposes
//
#ifdef CONFIG_TELEMETRY2
#define TELEMETRY_CATEGORY "telemetry"

//NOTE: Below telemetry macros also perform an icLog operation. This is done because as of writing this comment, we are
// still using telemetry 1.0 profiles that require a log line in order to function. If we migrate to purely Telemetry
// Framework (telemetry 2.0) profiles, we can remove the log invocations.

/*
 * Issue telemetry counter to T2.0
 */
extern void icTelemetryCounter(char *marker);

/*
 * Macro wrapper to issue a telemetry counter
 */
#define TELEMETRY_COUNTER(...) ({\
    char *marker = stringBuilder(__VA_ARGS__); \
    icLogInfo(TELEMETRY_CATEGORY, "%s",marker); \
    icTelemetryCounter(marker); \
    free(marker); })

/*
 * Issue telemetry string value to T2.0
 */
extern void icTelemetryStringValue(char* marker, char* value);

/*
 * Macro wrapper to issue telemetry string value
 * Prints the <marker>:<value> in the logs for backwards compatibility with T1
 */
#define TELEMETRY_VALUE_STRING(marker, value) ({ icLogInfo(TELEMETRY_CATEGORY, "%s:%s",marker,value); \
    icTelemetryStringValue(marker, value);})

/*
 * Issue a telemetry number value to T2.0
 */
extern void icTelemetryNumberValue(char* marker, double value);

/*
 * Macro wrapper to issue telemetry number value
 * Prints the <marker>:<value> in the logs for backwards compatibility with T1
 */
#define TELEMETRY_VALUE_NUMBER(marker,value) ({ icLogInfo(TELEMETRY_CATEGORY,"%s:%f",marker,value); \
    icTelemetryNumberValue(marker, value);})
#else
// No-op macros to support non-T2.0 capable platforms
//
#define TELEMETRY_COUNTER(...) NULL
#define TELEMETRY_VALUE_STRING(marker,value) NULL
#define TELEMETRY_VALUE_NUMBER(marker,value) NULL

#endif //CONFIG_LIB_LOG_ZLOG

#endif // IC_LOGGING_H

