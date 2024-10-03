//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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
//
#include <pthread.h>

/*
 * Simply detecting if we have a pthread_setname_np is insufficient to decide how get/setThreadName will work.
 * macOS and glibc, for example, both export that symbol but they have incompatible prototypes.
 *
 * prctl is Linux-specific and is not a suitable generic solution.
 *
 * For best compatibility we will make use of a wrapper around the thread function that will set the thread's name once
 * the thread starts.  Using pthread_setname_np from a sibling thread was occasionally producing SEGV's from tests
 */

/* Android does not have pthread_getname_np, and uClibc has neither getname nor setname, so go direct to prctl */
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "icConcurrent/threadUtils.h"
#include <assert.h>
#include <errno.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

#define LOG_TAG "threadUtils"

#ifdef CONFIG_OS_DARWIN
#define TASK_COMM_LEN 64
#else
#define TASK_COMM_LEN 16
#endif

typedef struct
{
    taskFunc task;
    void *taskArg;
    char *name;
} TaskWrapper;

extern inline void pthread_mutex_unlock__auto(pthread_mutex_t **mutex);

extern inline void pthread_rwlock_unlock__auto(pthread_rwlock_t **mutex);

static void setThreadName(const pthread_t *tid, const char *name)
{
#if defined(_GNU_SOURCE) || defined(__ANDROID__) || defined(CONFIG_OS_DARWIN)
    if (name != NULL)
    {
        char stdName[TASK_COMM_LEN];
        /*
         * The system is supposed to silently truncate the name if > TASK_COMM_LEN chars (including null)
         * but Linux will EFAULT if too long.
         */
        if (strlen(name) >= TASK_COMM_LEN)
        {
            icLogDebug(LOG_TAG, "thread name '%s' is too long, truncating to %d chars", name, TASK_COMM_LEN - 1);
            strncpy(stdName, name, TASK_COMM_LEN);
            stdName[TASK_COMM_LEN - 1] = 0;
            name = stdName;
        }

        int rc = 0;

        /* For MacOS 10.6+, we can only set our own thread's name */
        if (pthread_equal(*tid, pthread_self()))
        {
#ifdef CONFIG_OS_DARWIN
            rc = pthread_setname_np(name);
#elif defined(__linux__)
            rc = prctl(PR_SET_NAME, name, 0, 0, 0);
#else
#warning No support for thread names available
#endif
        }
        else
        {
            rc = -1;
            errno = ESRCH;
        }

        if (rc != 0)
        {
            AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(rc);
            icLogWarn(LOG_TAG, "Unable to set thread name '%s': %s", name, errStr);
        }
    }
#else
#warning No support for thread names available
#endif /* _GNU_SOURCE || __ANDROID__ || CONFIG_OS_DARWIN */
}

static void *wrappedTask(void *taskWrapper)
{
    TaskWrapper wrapper = *((TaskWrapper *) taskWrapper);
    free(taskWrapper);
    taskWrapper = NULL;

    void *retVal = NULL;
    pthread_t self = pthread_self();
    setThreadName(&self, wrapper.name);
    free(wrapper.name);
    wrapper.name = NULL;

    retVal = wrapper.task(wrapper.taskArg);

    return retVal;
}

bool createDetachedThread(taskFunc task, void *taskArg, const char *name)
{
    bool ok;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    TaskWrapper *wrapper = malloc(sizeof(TaskWrapper));
    wrapper->task = task;
    wrapper->name = strdupOpt(name);
    wrapper->taskArg = taskArg;

    task = wrappedTask;
    taskArg = wrapper;

    int rc = pthread_create(&tid, &attr, task, taskArg);
    pthread_attr_destroy(&attr);
    ok = rc == 0;

    if (rc != 0)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(rc);
        icLogWarn(LOG_TAG, "Unable to create thread '%s': %s", stringCoalesceAlt(name, "(anonymous)"), errStr);
    }

    return ok;
}

bool createThread(pthread_t *tid, taskFunc task, void *taskArg, const char *name)
{
    bool ok;

    if (tid == NULL)
    {
        icLogError(LOG_TAG, "tid cannot be NULL");
        return false;
    }

    TaskWrapper *wrapper = malloc(sizeof(TaskWrapper));
    wrapper->task = task;
    wrapper->name = strdupOpt(name);
    wrapper->taskArg = taskArg;

    task = wrappedTask;
    taskArg = wrapper;

    int rc = pthread_create(tid, NULL, task, taskArg);

    ok = rc == 0;
    if (rc != 0)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(rc);
        icLogWarn(LOG_TAG, "Unable to create thread '%s': %s", stringCoalesceAlt(name, "(anonymous)"), errStr);
    }

    return ok;
}

char *getThreadName(const pthread_t tid)
{
    char *threadName = NULL;
#if defined(_GNU_SOURCE) || defined(__ANDROID__) || defined(CONFIG_OS_DARWIN)
    threadName = malloc(TASK_COMM_LEN);
    int rc = 0;

#if defined(__linux__)
    if (pthread_equal(tid, pthread_self()))
    {
        /*
         * Android (until Pie/9.0) and uClibc don't have pthread_getname_np but support getting the current
         * thread name via prctl.
         */
        rc = prctl(PR_GET_NAME, threadName, 0, 0, 0);
    }
    else
    {
        rc = -1;
        errno = ESRCH;
    }
#else
    /* This is supported in MacOS (their getname and setname implementations are inconsistent) */
    rc = pthread_getname_np(tid, threadName, TASK_COMM_LEN);
#endif /* __linux___ */

    if (rc != 0)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogDebug(LOG_TAG, "%s: unable to get thread name: %s", __func__, errStr);

        free(threadName);
        threadName = NULL;
    }
#else
#warning No support for thread names available
#endif /* _GNU_SOURCE || __ANDROID__ || CONFIG_OS_DARWIN */
    return threadName;
}

void mutexInitWithType(pthread_mutex_t *mtx, int type)
{
    int pthreadError = 0;
    pthread_mutexattr_t attrs;
    pthreadError = pthread_mutexattr_init(&attrs);
    if (pthreadError != 0)
    {
        icLogWarn(LOG_TAG, "Mutex attr init at %p failed!", mtx);
    }
    assert(pthreadError == 0);

    if (type == PTHREAD_MUTEX_NORMAL)
    {
        icLogWarn(LOG_TAG, "PTHREAD_MUTEX_NORMAL removes safety mechanisms; upgrading to PTHREAD_MUTEX_ERRORCHECK");
        type = PTHREAD_MUTEX_ERRORCHECK;
    }

    pthreadError = pthread_mutexattr_settype(&attrs, type);
    if (pthreadError != 0)
    {
        icLogWarn(LOG_TAG, "Mutex set type to %d at %p failed!", type, mtx);
    }
    assert(pthreadError == 0);

    pthreadError = pthread_mutex_init(mtx, &attrs);
    pthread_mutexattr_destroy(&attrs);

    if (pthreadError != 0)
    {
        icLogWarn(LOG_TAG, "Mutex init at %p failed!", mtx);
        abort();
    }
}

void _mutexLock(pthread_mutex_t *mtx, const char *file, const int line)
{
    int error = pthread_mutex_lock(mtx);

    if (error != 0)
    {
        char *errStr;
        switch (error)
        {
            case EAGAIN:
                errStr = "recursion limit reached";
                break;

            case EDEADLK:
                errStr = "already locked by current thread";
                break;

            case EINVAL:
                errStr = "uninitialized mutex or priority inversion";
                break;

            default:
                errStr = "unknown";
                break;
        }
        icLogError(LOG_TAG, "mutex lock failed: [%d](%s) at %s:%d!", error, errStr, file, line);

        abort();
    }
}

void _mutexUnlock(pthread_mutex_t *mtx, const char *file, const int line)
{
    int error = pthread_mutex_unlock(mtx);

    if (error != 0)
    {
        const char *errStr = "unknown";
        if (error == EPERM)
        {
            errStr = "current thread does not own mutex";
        }
        else if (error == EINVAL)
        {
            errStr = "mutex not initialized";
        }

        icLogError(LOG_TAG, "mutex unlock failed: [%d](%s) at %s:%d!", error, errStr, file, line);

        abort();
    }
}


void _readLock(pthread_rwlock_t *lock, const char *file, const int line)
{
    int error = pthread_rwlock_rdlock(lock);

    if (error != 0)
    {
        char *errStr;
        switch (error)
        {
            case EAGAIN:
                errStr = "recursion limit reached";
                break;

            case EDEADLK:
                errStr = "already write locked by current thread";
                break;

            case EINVAL:
                errStr = "uninitialized mutex or priority inversion";
                break;

            default:
                errStr = "unknown";
                break;
        }
        icLogError(LOG_TAG, "read lock failed: [%d](%s) at %s:%d!", error, errStr, file, line);

        abort();
    }
}

void _writeLock(pthread_rwlock_t *lock, const char *file, const int line)
{
    int error = pthread_rwlock_wrlock(lock);

    if (error != 0)
    {
        char *errStr;
        switch (error)
        {
            case EDEADLK:
                errStr = "already locked by current thread";
                break;

            case EINVAL:
                errStr = "uninitialized mutex or priority inversion";
                break;

            default:
                errStr = "unknown";
                break;
        }
        icLogError(LOG_TAG, "write lock failed: [%d](%s) at %s:%d!", error, errStr, file, line);

        abort();
    }
}

void _rwUnlock(pthread_rwlock_t *lock, const char *file, const int line)
{
    int error = pthread_rwlock_unlock(lock);

    if (error != 0)
    {
        const char *errStr = "unknown";
        if (error == EPERM)
        {
            errStr = "current thread does not own mutex";
        }
        else if (error == EINVAL)
        {
            errStr = "mutex not initialized";
        }

        icLogError(LOG_TAG, "rw unlock failed: [%d](%s) at %s:%d!", error, errStr, file, line);

        abort();
    }
}
