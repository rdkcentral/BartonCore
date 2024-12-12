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
