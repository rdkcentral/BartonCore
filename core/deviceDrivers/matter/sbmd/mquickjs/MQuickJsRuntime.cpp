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

#define LOG_TAG "MQuickJsRuntime"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "MQuickJsRuntime.h"

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
uint8_t *MQuickJsRuntime::memBuffer = nullptr;
size_t MQuickJsRuntime::memSize = 0;
JSContext *MQuickJsRuntime::ctx = nullptr;
std::mutex MQuickJsRuntime::mutex;
bool MQuickJsRuntime::initialized = false;
size_t MQuickJsRuntime::peakHeapUsed = 0;

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

bool MQuickJsRuntime::Initialize(size_t memorySize)
{
    if (initialized)
    {
        icDebug("Shared mquickjs context already initialized");
        return true;
    }

    icInfo("Initializing shared mquickjs context for SBMD scripts (%zu bytes)...", memorySize);
    peakHeapUsed = 0;

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
        icError("Failed to install URL polyfill: %s", GetExceptionString(ctx).c_str());
        {
            std::lock_guard<std::mutex> lock(mutex);
            LogMemoryUsage("polyfill-failed", IC_LOG_ERROR, true);
        }
        JS_FreeContext(ctx);
        ctx = nullptr;
        free(memBuffer);
        memBuffer = nullptr;
        memSize = 0;
        return false;
    }

    // Check if polyfill installation left an exception
    std::string exMsg;
    if (CheckAndClearPendingException(ctx, &exMsg))
    {
        icError("Polyfill installation left a pending exception: %s - this is a bug", exMsg.c_str());
    }

    initialized = true;
    {
        std::lock_guard<std::mutex> lock(mutex);
        LogMemoryUsage("post-init (context + stdlib + polyfills)", IC_LOG_DEBUG);
    }
    icInfo("Shared mquickjs context initialized successfully");
    return true;
}

void MQuickJsRuntime::Shutdown()
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

JSContext *MQuickJsRuntime::GetSharedContext()
{
    return ctx;
}

std::mutex &MQuickJsRuntime::GetMutex()
{
    return mutex;
}

bool MQuickJsRuntime::IsInitialized()
{
    return initialized;
}

bool MQuickJsRuntime::CheckAndClearPendingException(JSContext *ctx, std::string *outExceptionMsg)
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

void MQuickJsRuntime::LogMemoryUsage(const char *label, logPriority priority, bool walkHeap)
{
    if (!ctx)
    {
        return;
    }

    int flags = walkHeap ? JS_MEMUSAGE_WALK_HEAP : 0;
    JSMemoryUsage usage = {};

    if (JS_GetMemoryUsage(ctx, &usage, flags) != 0)
    {
        icWarn("Failed to get mquickjs memory usage at '%s'", label);
        return;
    }

    bool heapWalked = (usage.flags & JS_MEMUSAGE_WALK_HEAP) != 0;

    if (heapWalked)
    {
        // Net heap = heap region minus free blocks reclaimed by GC
        size_t netHeapUsed = 0;

        if (usage.heap_used >= usage.heap_free_blocks)
        {
            netHeapUsed = usage.heap_used - usage.heap_free_blocks;
        }
        
        if (netHeapUsed > peakHeapUsed)
        {
            peakHeapUsed = netHeapUsed;
        }

        icLogMsg(__FILE__,
                 sizeof(__FILE__) - 1,
                 __func__,
                 sizeof(__func__) - 1,
                 __LINE__,
                 LOG_TAG,
                 priority,
                 logFmt("[%s] mquickjs memory: arena=%zu heap=%zu (net=%zu, free_blocks=%zu) "
                        "stack=%zu free_gap=%zu overhead=%zu peak_heap=%zu"),
                 label,
                 usage.arena_size,
                 usage.heap_used,
                 netHeapUsed,
                 usage.heap_free_blocks,
                 usage.stack_used,
                 usage.free_size,
                 usage.overhead,
                 peakHeapUsed);
    }
    else
    {
        icLogMsg(__FILE__,
                 sizeof(__FILE__) - 1,
                 __func__,
                 sizeof(__func__) - 1,
                 __LINE__,
                 LOG_TAG,
                 priority,
                 logFmt("[%s] mquickjs memory: arena=%zu heap=%zu "
                        "stack=%zu free_gap=%zu overhead=%zu (heap_free_blocks not computed)"),
                 label,
                 usage.arena_size,
                 usage.heap_used,
                 usage.stack_used,
                 usage.free_size,
                 usage.overhead);
    }
}

} // namespace barton
