//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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
 * @file MQuickJsStdlib.c
 *
 * C implementation of the mquickjs stdlib functions required by the generated
 * mqjs_stdlib.h header. This file MUST be compiled as C (not C++) because the
 * generated header uses C99 designated initializers for union members.
 *
 * The generated mqjs_stdlib.h references these C functions by name in its
 * js_c_function_table[]. They must be defined before including that header.
 *
 * Console output (js_print) is routed to icLog for integration with Barton's
 * logging system.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <mquickjs/mquickjs.h>

#define LOG_TAG "MQuickJsStdlib"
#define logFmt(fmt) "(%s): " fmt, __func__
#include <icLog/logging.h>

/*
 * console.log / print implementation.
 * Routes output to icDebug for Barton logging integration.
 */
static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    char message[1024];
    int offset = 0;

    for (int i = 0; i < argc && offset < (int) sizeof(message) - 1; i++)
    {
        if (i != 0 && offset < (int) sizeof(message) - 1)
        {
            message[offset++] = ' ';
        }

        JSValue v = argv[i];
        if (JS_IsString(ctx, v))
        {
            JSCStringBuf buf;
            size_t len;
            const char *str = JS_ToCStringLen(ctx, &len, v, &buf);
            if (str)
            {
                int remaining = (int) sizeof(message) - offset - 1;
                int copyLen = (int) len < remaining ? (int) len : remaining;
                memcpy(message + offset, str, copyLen);
                offset += copyLen;
            }
        }
        else
        {
            /* For non-string values, convert to string first */
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, v, &buf);
            if (str)
            {
                int remaining = (int) sizeof(message) - offset - 1;
                int slen = (int) strlen(str);
                int copyLen = slen < remaining ? slen : remaining;
                memcpy(message + offset, str, copyLen);
                offset += copyLen;
            }
        }
    }
    message[offset] = '\0';
    icDebug("[JS] %s", message);
    return JS_UNDEFINED;
}

/*
 * gc() implementation - triggers garbage collection.
 */
static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JS_GC(ctx);
    return JS_UNDEFINED;
}

/*
 * Date.now() implementation - returns current time in milliseconds.
 */
static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return JS_NewInt64(ctx, (int64_t) tv.tv_sec * 1000 + (tv.tv_usec / 1000));
}

/*
 * performance.now() implementation - returns high-resolution time in milliseconds.
 */
static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return JS_NewInt64(ctx, (int64_t) ts.tv_sec * 1000 + (ts.tv_nsec / 1000000));
}

/*
 * load() implementation - not supported in embedded context.
 * Returns undefined (file loading is not available in SBMD scripts).
 */
static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    icError("load() is not implemented");
    return JS_UNDEFINED;
}

/*
 * setTimeout() implementation - minimal no-op stub.
 * Timer functionality is not needed for synchronous SBMD script execution.
 */
static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    icError("setTimeout() is not implemented");
    return JS_NewInt32(ctx, 0);
}

/*
 * clearTimeout() implementation - minimal no-op stub.
 */
static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    icError("clearTimeout() is not implemented");
    return JS_UNDEFINED;
}

/*
 * Include the generated stdlib header AFTER defining the C functions it
 * references. This instantiates the js_stdlib global variable.
 */
#include <mquickjs/mqjs_stdlib.h>
