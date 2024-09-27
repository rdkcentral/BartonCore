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


#ifndef ZILKER_THREADUTILS_H
#define ZILKER_THREADUTILS_H

#include <stdbool.h>
#include <pthread.h>
#include <icTypes/sbrm.h>

typedef void * (*taskFunc)(void *);

/**
 * Create a detached thread
 * @param task
 * @param taskArg
 * @param name The thread name; may be NULL. This should be limited to 15 characters; long strings will be truncated.
 * @return true when the thread was created
 */
bool createDetachedThread(taskFunc task, void *taskArg, const char *name);

/**
 * Create a joinable thread
 * @param tid
 * @param task
 * @param taskArg
 * @param name The thread name; may be NULL. This should be limited to 15 characters; long strings will be truncated.
 * @return true when the thread was created
 */
bool createThread(pthread_t *tid, taskFunc task, void *taskArg, const char *name);

/**
 * Get the thread name by tid
 * @note Android only supports getting the calling thread name
 * @param tid
 * @return thread name or NULL if unavailable (caller must free)
 */
char *getThreadName(const pthread_t tid);

/**
 * Initialize a mutex with a type
 * @param mtx
 * @param type The lock type, e.g., PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ERRORCHECK
 */
void mutexInitWithType(pthread_mutex_t *mtx, int type);

/**
 * This implements mutexLock - use the macro below to automatically localize errors
 * @param mtx The mutex to lock
 * @param file The mutexLock call site file
 * @param line The mutexLock call site line number
 */
void _mutexLock(pthread_mutex_t *mtx, const char *file, const int line);

/**
 * Lock a mutex, blocking if held by another thread.
 * @param mtx
 * @note When mutex error checking is enabled, an invalid operation will abort the program.
 */
#define mutexLock(mtx) _mutexLock(mtx, __FILE__, __LINE__)

/* --- */

/**
 * This implements mutexUnlock - use the macro below to automatically localize errors
 * @param mtx The mutex to unlock
 * @param file The mutexUnlock call site file
 * @param line The mutexUnlock call site line number
 */
void _mutexUnlock(pthread_mutex_t *mtx, const char *file, const int line);

/**
 * Unlock a mutex
 * @param mtx
 * @note When mutex error checking is enabled, an invalid unlock will abort the program
 */
#define mutexUnlock(mtx) _mutexUnlock(mtx, __FILE__, __LINE__)

/**
 * Lock the current scope with a
 */
#define LOCK_SCOPE_ALIAS(m, n) AUTO_CLEAN(pthread_mutex_unlock__auto) pthread_mutex_t *lock_guard_##n = &m;\
mutexLock(lock_guard_##n)

/**
 * Convenience macro to create a guard pointer for a mutex and lock it, releasing it when the guard leaves the current scope.
 * @note This should be limited to use in small sections. It generally is not suitable for use in an entire function.
 */
#define LOCK_SCOPE(m) LOCK_SCOPE_ALIAS(m, m)

inline void pthread_mutex_unlock__auto(pthread_mutex_t **mutex)
{
    mutexUnlock(*mutex);
}

/**
 * This implements readLock - use the macro below to automatically localize errors
 * @param lock The lock to read lock
 * @param file The readLock call site file
 * @param line The readLock call site line number
 */
void _readLock(pthread_rwlock_t *lock, const char *file, const int line);

/**
 * Read lock a rwlock, blocking if held by another thread.
 * @param lock
 * @note When lock error checking is enabled, an invalid operation will abort the program.
 */
#define readLock(lock) _readLock(lock, __FILE__, __LINE__)

/**
 * This implements writeLock - use the macro below to automatically localize errors
 * @param lock The lock to write lock
 * @param file The readLock call site file
 * @param line The readLock call site line number
 */
void _writeLock(pthread_rwlock_t *lock, const char *file, const int line);

/**
 * Write lock a rwlock, blocking if held by another thread.
 * @param lock
 * @note When lock error checking is enabled, an invalid operation will abort the program.
 */
#define writeLock(lock) _writeLock(lock, __FILE__, __LINE__)

/* --- */

/**
 * This implements rwUnlock - use the macro below to automatically localize errors
 * @param lock The lock to unlock
 * @param file The rwUnlock call site file
 * @param line The rwUnlock call site line number
 */
void _rwUnlock(pthread_rwlock_t *lock, const char *file, const int line);

/**
 * Unlock a rw lock
 * @param lock
 * @note When lock error checking is enabled, an invalid unlock will abort the program
 */
#define rwUnlock(lock) _rwUnlock(lock, __FILE__, __LINE__)

/**
 * Convenience macro to create a guard pointer for a rwlock and read lock it, releasing it when the guard leaves the current scope.
 * @note This should be limited to use in small sections. It generally is not suitable for use in an entire function.
 */
#define READ_LOCK_SCOPE(m) AUTO_CLEAN(pthread_rwlock_unlock__auto) pthread_rwlock_t *lock_guard_##m = &m;\
readLock(lock_guard_##m)

/**
 * Convenience macro to create a guard pointer for a rwlock and read lock it, releasing it when the guard leaves the current scope.
 * @note This should be limited to use in small sections. It generally is not suitable for use in an entire function.
 */
#define WRITE_LOCK_SCOPE(m) AUTO_CLEAN(pthread_rwlock_unlock__auto) pthread_rwlock_t *lock_guard_##m = &m;\
writeLock(lock_guard_##m)

inline void pthread_rwlock_unlock__auto(pthread_rwlock_t **lock)
{
    rwUnlock(*lock);
}



#endif //ZILKER_THREADUTILS_H
