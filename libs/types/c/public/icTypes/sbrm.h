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

/**
 * Basic Scope Bound Resource Management for C. This is essentially a subset of RAII that binds a resource to its
 * scope lifetime.
 *
 * When these macros are used, the compiler will generate scope epilogue code that passes stack pointers to a function,
 * i.e., (cleanup_func(&thing)).
 */

#ifndef ZILKER_SBRM_H
#define ZILKER_SBRM_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __GNUC__

/**
 * Add an attribute to an automatic (stack) variable to call a cleanup function after it goes out of scope.
 * The cleanup function will receive a pointer compatible with the declared type. Its return
 * value is ignored.
 * @note Only use this on variables that are guaranteed to be initialized before the declaring scope can possibly exit.
 * E.g.,
 * BAD: AUTO_CLEAN(free_generic__auto) char *buf;
 *      …
 * OK: AUTO_CLEAN(free_generic__auto) char *buf = NULL;
 */
#define AUTO_CLEAN(d) __attribute__((cleanup(d)))

/**
 * Convenience function to free a trivial pointer type (e.g., string)
 */
inline void free_generic__auto(void *p)
{
    void **tmp = (void **) p;
    free(*tmp);
}

#define DESTRUCTOR_NAME(destroy) destroy##__auto

/**
 * Shorthand for declaring a scoped type.
 * @example #define scoped_Foo DECLARE_SCOPED(type, fooDestroy)
 *          scoped_Foo *foo = foo_new();
 */
#define DECLARE_SCOPED(type, d)  AUTO_CLEAN(DESTRUCTOR_NAME(d)) type

/**
 * Invoke free() on a heap pointer when it leaves the declaring scope.
 */
#define scoped_generic           AUTO_CLEAN(free_generic__auto)

/**
 * Define a trivial inline destructor for AUTO_CLEAN
 * Put this in your header.
 */
#define DEFINE_DESTRUCTOR(type, d)                                                                                     \
    inline void DESTRUCTOR_NAME(d)(type * *p)                                                                          \
    {                                                                                                                  \
        d(*p);                                                                                                         \
    }

/**
 * Declare a trivial inline destructor for AUTO_CLEAN
 * Put this in your implementation _once_.
 */
#define DECLARE_DESTRUCTOR(type, d) extern inline void DESTRUCTOR_NAME(d)(type * *p)

/**
 * Convenience function to auto-close a file. Files opened in a write mode will by fsynced.
 * @param fp
 */
void fclose__auto(FILE **fp);

#define scoped_FILE AUTO_CLEAN(fclose__auto) FILE

#else
#error "GCC is required for SBRM"
#endif //__GNUC__
#endif // ZILKER_SBRM_H
