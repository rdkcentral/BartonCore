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

//
// Created by tlea on 2/18/26
//

#define LOG_TAG "QuickJsRuntime"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "QuickJsRuntime.h"

#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>

// The js_stdlib variable is defined in MQuickJsStdlib.c (compiled as C)
// and provides the mquickjs standard library (console, performance, JSON, etc.)
extern const JSSTDLibraryDef js_stdlib;
}

namespace barton
{

// Static member initialization
uint8_t *QuickJsRuntime::memBuffer = nullptr;
size_t QuickJsRuntime::memSize = 0;
JSContext *QuickJsRuntime::ctx = nullptr;
std::mutex QuickJsRuntime::mutex;
bool QuickJsRuntime::initialized = false;

namespace
{
    /**
     * Extract mquickjs exception as a string.
     */
    std::string GetExceptionString(JSContext *ctx)
    {
        JSValue ex = JS_GetException(ctx);

        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, ex, &buf);
        if (str)
        {
            return std::string(str);
        }
        return "unknown error";
    }

} // anonymous namespace

bool QuickJsRuntime::Initialize(size_t memorySize)
{
    if (initialized)
    {
        icDebug("Shared mquickjs context already initialized");
        return true;
    }

    icInfo("Initializing shared mquickjs context for SBMD scripts (%zu bytes)...", memorySize);

    // Allocate the memory buffer for the mquickjs context
    memBuffer = static_cast<uint8_t *>(malloc(memorySize));
    if (!memBuffer)
    {
        icError("Failed to allocate %zu bytes for mquickjs context", memorySize);
        return false;
    }
    memSize = memorySize;

    // Create the context with pre-allocated memory and default stdlib
    ctx = JS_NewContext(memBuffer, memSize, &js_stdlib);
    if (!ctx)
    {
        icError("Failed to create shared mquickjs context");
        free(memBuffer);
        memBuffer = nullptr;
        memSize = 0;
        return false;
    }

    // Install URL polyfill (required by some JS libraries)
    // Use JS_EVAL_REPL so var declarations persist as global variables
    const char *urlPolyfill = R"(
        var URL = function URL(url, base) {
            this.href = url;
            this.protocol = '';
            this.host = '';
            this.hostname = '';
            this.port = '';
            this.pathname = url;
            this.search = '';
            this.hash = '';
            this.origin = '';
        };
        URL.prototype.toString = function() { return this.href; };
        globalThis.URL = URL;
    )";
    JSValue urlResult = JS_Eval(ctx, urlPolyfill, strlen(urlPolyfill), "<url-polyfill>", JS_EVAL_REPL);
    if (JS_IsException(urlResult))
    {
        icWarn("Failed to install URL polyfill: %s", GetExceptionString(ctx).c_str());
    }

    // Check if polyfill installation left an exception
    std::string exMsg;
    if (CheckAndClearPendingException(ctx, &exMsg))
    {
        icError("Polyfill installation left a pending exception: %s - this is a bug", exMsg.c_str());
    }

    initialized = true;
    icInfo("Shared mquickjs context initialized successfully");
    return true;
}

void QuickJsRuntime::Shutdown()
{
    if (!initialized)
    {
        return;
    }

    icInfo("Shutting down shared mquickjs context...");

    if (ctx)
    {
        JS_FreeContext(ctx);
        ctx = nullptr;
    }

    if (memBuffer)
    {
        free(memBuffer);
        memBuffer = nullptr;
        memSize = 0;
    }

    initialized = false;
    icInfo("Shared mquickjs context shutdown complete");
}

JSContext *QuickJsRuntime::GetSharedContext()
{
    return ctx;
}

std::mutex &QuickJsRuntime::GetMutex()
{
    return mutex;
}

bool QuickJsRuntime::IsInitialized()
{
    return initialized;
}

bool QuickJsRuntime::CheckAndClearPendingException(JSContext *ctx, std::string *outExceptionMsg)
{
    if (!ctx)
    {
        return false;
    }

    JSValue pendingEx = JS_GetException(ctx);

    // JS_GetException returns JS_NULL or JS_UNDEFINED when no exception is pending
    bool hasException = !JS_IsNull(pendingEx) && !JS_IsUndefined(pendingEx) && !JS_IsUninitialized(pendingEx);

    if (!hasException)
    {
        return false;
    }

    // Extract exception message if caller wants it
    if (outExceptionMsg)
    {
        std::string exMsg;

        // Try ToCString for string exceptions
        if (JS_IsString(ctx, pendingEx))
        {
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, pendingEx, &buf);
            if (str)
            {
                exMsg = str;
            }
        }
        else if (JS_IsPtr(pendingEx))
        {
            // Try "message" property for Error objects
            JSValue msgVal = JS_GetPropertyStr(ctx, pendingEx, "message");
            if (JS_IsString(ctx, msgVal))
            {
                JSCStringBuf buf;
                const char *msgStr = JS_ToCString(ctx, msgVal, &buf);
                if (msgStr)
                {
                    exMsg = msgStr;
                }
            }

            // Also try to get stack trace for debugging
            JSValue stackVal = JS_GetPropertyStr(ctx, pendingEx, "stack");
            if (JS_IsString(ctx, stackVal))
            {
                JSCStringBuf buf;
                const char *stackStr = JS_ToCString(ctx, stackVal, &buf);
                if (stackStr)
                {
                    if (!exMsg.empty())
                    {
                        exMsg += " | Stack: ";
                    }
                    exMsg += stackStr;
                }
            }
        }
        else
        {
            // Try ToCString as fallback for other types
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, pendingEx, &buf);
            if (str)
            {
                exMsg = str;
            }
        }

        if (exMsg.empty())
        {
            exMsg = "unknown exception";
        }

        *outExceptionMsg = std::move(exMsg);
    }

    return true;
}

} // namespace barton
