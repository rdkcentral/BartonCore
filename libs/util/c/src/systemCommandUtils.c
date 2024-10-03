//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast
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
// Comcast Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * systemCommandUtils.c
 *
 * Utilities for system commands
 *
 * Author: eInfochips
 *-----------------------------------------------*/

#include <errno.h>
#include <icLog/logging.h>
#include <icTypes/icStringBuffer.h>
#include <icUtil/stringUtils.h>
#include <icUtil/systemCommandUtils.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOG_TAG "systemCommandUtils"

#undef HAVE_EXECVPE
#if defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 11)
#define HAVE_EXECVPE
#endif
#endif

int execShellCommand(const char *commandStr)
{
    if (commandStr == NULL)
    {
        return -1;
    }

    icLogDebug(LOG_TAG, "running command '%s'", commandStr);
    int rc = system(commandStr);
    if (rc != 0)
    {
        icLogWarn(LOG_TAG, "failed running '%s' script, errorCode : %d", commandStr, WEXITSTATUS(rc));
    }
    else
    {
        icLogDebug(LOG_TAG, "Command '%s' executed successfully", commandStr);
    }

    return WEXITSTATUS(rc);
}

int execSystemCommand(const char *program, ...)
{
    if (program == NULL)
    {
        return -1;
    }

    int argc;
    va_list argList;
    va_start(argList, program);

    // To determine the number of arguments provided
    //
    for (argc = 1; va_arg(argList, const char *) != NULL; argc++)
        ;

    va_end(argList);

    char *argv[argc + 1];
    va_start(argList, program);

    argv[0] = (char *) program;

    for (int i = 1; i <= argc; i++)
    {
        argv[i] = va_arg(argList, char *);
    }

    va_end(argList);

    return __executeSystemCommand(program, argv);
}

int __executeSystemCommand(const char *path, char *const argv[])
{
    if (argv[0] == NULL || path == NULL)
    {
        return -1;
    }

    int skipFD = -1;
    struct rlimit fdlim;
    uint16_t fdmax = 1024;

#ifdef BUILDENV_android
    char *propEnv = getenv("ANDROID_PROPERTY_WORKSPACE");

    // If we can find the android property workspace environment, we want to leave that fd
    // open across the exec
    //
    if (propEnv != NULL)
    {
        // We cannot change propEnv, or we are screwing with the environment
        //
        char *localEnv = strdup(propEnv);
        char *comma = strchr(localEnv, ',');

        // Remove the comma
        //
        if (comma != NULL)
        {
            *comma = '\0';
        }
        if (stringToInt32(localEnv, &skipFD) == false)
        {
            skipFD = -1;
        }
        free(localEnv);
    }
#endif

    if (getrlimit(RLIMIT_NOFILE, &fdlim) == 0)
    {
        fdmax = fdlim.rlim_cur;
    }

    pid_t pid;
    int status;
    pid = fork();

    if (pid == -1)
    {
        icLogError(LOG_TAG, "Error forking thread");
        return -1;
    }
    else if (pid == 0)
    {
        // Child process
        // Only functions that are listed in `man 7 signal-safety` may be called from here on.
        //
        // Close all fds above stdio to release any resources opened without FD_CLOEXEC,
        // and exec the given process.
        //
        for (int cnt = 3; cnt < fdmax; cnt++)
        {
            if (cnt != skipFD)
            {
                close(cnt);
            }
        }

        /*
         * Don't use most exec()s with gcov. Unfortunately there's no predefined
         * macro(s) to detect it is being used. Some libcs may hang randomly due
         * to non-async-safe calls in the gcov stack that are either not handled
         * with pthread_atfork or are impossible to sanely protect.
         * execvpe is *not* wrapped by gcov.
         *
         * _exit() prevents any registered atexit() functions from running, which
         * can cause child processes to wait forever on no longer valid clones of
         * thread control objects (e.g., mutexes and condition variables).
         */
#ifdef HAVE_EXECVPE
        _exit(execvpe(path, argv, environ));
#else
        _exit(execvp(path, argv));
#endif
    }
    else
    {
        if (waitpid(pid, &status, 0) > 0)
        {
            if (WIFEXITED(status) == true)
            {
                if (WEXITSTATUS(status) == 0)
                {
                    icLogDebug(LOG_TAG, "Command '%s' executed successfully", argv[0]);
                    return 0;
                }
                else
                {
                    icLogError(
                        LOG_TAG, "Command '%s' execution failed - %d [%d]", argv[0], WEXITSTATUS(status), status);
                    return WEXITSTATUS(status);
                }
            }
            else if (WIFSIGNALED(status) == true)
            {
                icLogError(LOG_TAG, "Process for command '%s' received signal %d", argv[0], WTERMSIG(status));
                return 128 + WTERMSIG(status);
            }
            else
            {
                icLogError(LOG_TAG, "Process for command '%s' didn't terminate normally %d", argv[0], status);
                return -1;
            }
        }
        else
        {
            icLogError(LOG_TAG, "Error waiting for child process");
            return -1;
        }
    }
}