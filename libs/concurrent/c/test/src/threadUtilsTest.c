//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast Corporation
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

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <signal.h>
#include <unistd.h>
#include <wait.h>

#define LOG_TAG "threadUtilsTest"

static pthread_mutex_t testMtx;
static pthread_rwlock_t testRwLock = PTHREAD_RWLOCK_INITIALIZER;

static int teardown(void **state)
{
    pthread_mutex_destroy(&testMtx);

    return 0;
}

static void forkExpectSignal(void (*test)(void **state), void **state, int signal)
{
    pid_t pid = fork();
    if (pid > 0)
    {
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status) == 0 || WTERMSIG(status) != signal)
        {
            fail_msg("Child did not receive signal: signaled: %d signo: %d", WIFSIGNALED(status), WTERMSIG(status));
        }
    }
    else if (pid == 0)
    {
        test(state);
        /* Abort! */
    }
    else
    {
        fail_msg("fork() failed to create test process!");
    }
}

/**
 * Do a bad mutex operation: Try to lock the same mtx in the same thread.
 * @note testMtx must be an ERRORCHECK mutex.
 * @param state
 */
static void doMutexDeadlock(void **state)
{
    mutexLock(&testMtx);
    mutexLock(&testMtx);
}

/**
 * Do a bad mutex operation: Try to unlock a mtx not owned by the calling thread.
 * @note testMtx must be an ERRORCHECK mutex.
 * @param state
 */
static void doMutexOverUnlock(void **state)
{
    mutexLock(&testMtx);
    mutexUnlock(&testMtx);

    /* Abort! */
    mutexUnlock(&testMtx);
}

static void test_mutexErrorCheck(void **state)
{
    mutexInitWithType(&testMtx, PTHREAD_MUTEX_ERRORCHECK);

    forkExpectSignal(doMutexDeadlock, state, SIGABRT);
    forkExpectSignal(doMutexOverUnlock, state, SIGABRT);
}

static void test_mutexReentrant(void **state)
{
    mutexInitWithType(&testMtx, PTHREAD_MUTEX_RECURSIVE);

    mutexLock(&testMtx);
    mutexLock(&testMtx);

    mutexUnlock(&testMtx);
    mutexUnlock(&testMtx);
}

static void doMutexLock(void **state)
{
    mutexLock(&testMtx);
}

static void doMutexUnlock(void **state)
{
    mutexUnlock(&testMtx);
}

static void doBadScopeUnlock(void **state)
{
    {
        LOCK_SCOPE(testMtx);
    }

    /* testMtx unlocked at end of previous scope; this will fail */
    mutexUnlock(&testMtx);
}

static void test_mutexUninitialized(void **state)
{
    forkExpectSignal(doMutexLock, state, SIGABRT);
    forkExpectSignal(doMutexUnlock, state, SIGABRT);
}

static void test_mutexLockScope(void **state)
{
    mutexInitWithType(&testMtx, PTHREAD_MUTEX_ERRORCHECK);
    {
        LOCK_SCOPE(testMtx);
    }

    forkExpectSignal(doBadScopeUnlock, state, SIGABRT);
}

static void test_readWriteLockScope(void **state)
{
    {
        READ_LOCK_SCOPE(testRwLock);
    }

    {
        WRITE_LOCK_SCOPE(testRwLock);
    }
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_teardown(test_mutexReentrant, teardown),
        cmocka_unit_test_teardown(test_mutexErrorCheck, teardown),
        cmocka_unit_test_teardown(test_mutexUninitialized, teardown),
        cmocka_unit_test_teardown(test_mutexLockScope, teardown),
        cmocka_unit_test(test_readWriteLockScope),
    };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
