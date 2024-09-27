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
 * loggerSyslog.c
 *
 * Syslog implementation of the logging.h facade.
 * Will be built and included based on #define values
 *
 * Author: jelderton - 7/8/15
 *-----------------------------------------------*/

// look at buildtime settings to see if this log implementation should be enabled

#ifdef CONFIG_LIB_LOG_SYSLOG

#include <syslog.h>
#include <stdarg.h>
#include <sys/syscall.h>

#include <icLog/logging.h>
#include <memory.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "loggingCommon.h"

#define FALLBACKTAG "library"
#define BUFFER_SIZE 512
#define THREAD_NAME_BUFFER_SIZE 30
#define FORMAT_BUFFER_SIZE 10

//Setting the chunk size to 900 to be safe
//I tested some higher values, but there were still occasions of messages getting truncated
//Probably do to my inability to adquately account for all additional bytes
//added to the message through syslog itself and adjustFormat
//
#define LOG_CHUNK_SIZE 900

static char chunkBuffer[LOG_CHUNK_SIZE+1] = "";

static char *logName = NULL;



//static methods
static char* findNameByPid(int pid);
static char *getNameForThread(void);
static char *adjustFormat(const char *origFormat);
//end static methods

/*
 * initialize the logger
 */
__attribute__ ((constructor)) static void initIcLogger(void)
{
    logName = findNameByPid(getpid());
    openlog(logName, LOG_PID, LOG_USER);
}

/*
 * Find the process name from the PID
 * returns the process name, must be freed by the caller
 */
static char* findNameByPid(int pid)
{
    ssize_t rc;
    int commFd;
    char fname[BUFFER_SIZE];
    char *pidName;
    snprintf(fname,BUFFER_SIZE,"/proc/%d/cmdline",pid);
    commFd = open(fname,O_RDONLY | O_CLOEXEC);
    if (commFd < 0)  //Can't read the file, fall back to
    {
        pidName = calloc(1,BUFFER_SIZE);
        snprintf(pidName,BUFFER_SIZE,"%s",FALLBACKTAG);
    }
    else
    {
        char *pidNameTemp = calloc(1,BUFFER_SIZE);
        rc = read(commFd, pidNameTemp, BUFFER_SIZE);
        if (rc <= 0)
        {
            pidName = calloc(1,BUFFER_SIZE);
            snprintf(pidName,BUFFER_SIZE,"%s",FALLBACKTAG);
        }
        else
        {
            char *lastSlash = strrchr(pidNameTemp, '/');
            if (lastSlash != NULL)
            {
                lastSlash++;
                pidName = strdup(lastSlash);
            }
            else
            {
                pidName = strdup(pidNameTemp);
            }
        }
        close(commFd);
        free(pidNameTemp);
    }

    return pidName;
}

/*
 * Get the name of the thread
 * returns the thread name, must be freed by the caller
 */
static char *getNameForThread(void)
{
    long tid = syscall(SYS_gettid);
    pid_t pid = getpid();
    char *retval = calloc(1,THREAD_NAME_BUFFER_SIZE);
    if (tid == pid)
    {
        snprintf(retval,THREAD_NAME_BUFFER_SIZE,"main-%05d",pid);
    }
    else
    {
        snprintf(retval,THREAD_NAME_BUFFER_SIZE,"thread-%05d",(int)tid);
    }
    return retval;
}

/*
 * Adjust the format to add in additional information
 * Returns the adjusted format string(char*).  Must be freed by the caller
 */
static char *adjustFormat(const char *origFormat)
{
    char *threadName = getNameForThread();
    size_t reqSpace = strlen(origFormat)+strlen(threadName)+FORMAT_BUFFER_SIZE;
    char *retval = calloc(1,reqSpace);
    snprintf(retval,reqSpace," [tid=%s] %s",threadName,origFormat);
    free(threadName);
    return retval;
}

/*
 * Issue logging message based on a 'categoryName' and 'priority'
 */
void icLogMsg(const char *file, size_t filelen,
              const char *func, size_t funclen,
              long line,
              const char *categoryName, logPriority priority, const char *format, ...)
{
    va_list  arglist;
    int      logPriority = LOG_DEBUG;

    // skip if priority is > logLevel
    //
    if (!shouldLogMessage(priority))
    {
        return;
    }

    // map priority to Log4c syntax
    //
    switch (priority)
    {
        case IC_LOG_TRACE:  // no TRACE, so use DEBUG
        case IC_LOG_DEBUG:
            logPriority = LOG_DEBUG;
            break;

        case IC_LOG_INFO:
            logPriority = LOG_INFO;
            break;

        case IC_LOG_WARN:
            logPriority = LOG_WARNING;
            break;

        case IC_LOG_ERROR:
            logPriority = LOG_ERR;
            break;

        default:
            break;
    }

    // TODO: deal with 'category name'

    // preprocess the variable args format, then forward to syslog
    //
    char *realFmt = adjustFormat(format);

    //Initialize the list of arguments after format, in this case its the "..." in the method
    //
    va_start(arglist, format);

    char *formattedMessage = NULL;

    //get the size of the log message
    //
    int formattedMessageLen = vsnprintf(formattedMessage, 0, realFmt, arglist);

    //invalidate the list of args
    //
    va_end(arglist);

    if (formattedMessageLen > LOG_CHUNK_SIZE)
    {

        //Initialize the list of arguments after format, in this case its the "..." in the function
        //Get the length of the formatted string without having adjusted the format
        //Assign the memory to the length +1 for the null byte
        //
        va_start(arglist, format);
        formattedMessageLen = vsnprintf(formattedMessage, 0, format, arglist);
        formattedMessage = calloc(1, formattedMessageLen + 1);  //add 1 for the null byte

        //invalidate the list of args after use
        //
        va_end(arglist);

        //Re-initialize the list of arguments after format, in this case its the "..." in the function
        //Print the formatted message into the buffer
        //
        va_start(arglist, format);
        vsnprintf(formattedMessage, formattedMessageLen + 1, format, arglist);

        //invalidate the list of args
        //
        va_end(arglist);

        //determine how many times we need to split it up
        //
        int numChunks = formattedMessageLen / LOG_CHUNK_SIZE;

        //if there is a remainder, then add one to account for it
        //
        if(formattedMessageLen % LOG_CHUNK_SIZE != 0)
        {
            numChunks++;
        }
        char *ptrLocation = formattedMessage;
        for(int chunk = 0 ; chunk < numChunks; chunk++)
        {
            memcpy(chunkBuffer,ptrLocation,LOG_CHUNK_SIZE);
            chunkBuffer[LOG_CHUNK_SIZE] = '\0';

            char *chunkedRealFmt = adjustFormat(chunkBuffer);
            syslog(logPriority, "%s", chunkedRealFmt);
            free(chunkedRealFmt);

            ptrLocation = ptrLocation + LOG_CHUNK_SIZE;  //Move down the message in LOG_CHUNK_SIZE increments
            memset(chunkBuffer,0,LOG_CHUNK_SIZE+1);  //reset the buffer
        }
        free(formattedMessage);
    }
    else
    {
        //reinitialize the argument list
        //Send the format string and the arg list to syslog
        //
        va_start(arglist, format);
        vsyslog(logPriority, realFmt, arglist);

        //invalidate the arg list
        //
        va_end(arglist);
    }

    free(realFmt);
}

#endif /* CONFIG_LIB_LOG_SYSLOG */
