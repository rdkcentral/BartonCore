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
#include "SbmdJsUtil.h"
#include "matter/sbmd/SafeJSValue.h"

#include <chrono>
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
    using namespace mquickjs;

    namespace
    {
        /**
         * Interrupt handler for script execution timeout.
         *
         * Called periodically by the mquickjs engine during bytecode execution.
         * Returns non-zero to abort the running script when the deadline has passed.
         * When no deadline is active (epoch value), always returns 0.
         */
        int ScriptInterruptHandler(JSContext * /*ctx*/, void * /*opaque*/)
        {
            auto currentDeadline = MQuickJsRuntime::GetDeadline();

            // No deadline set, allow script to run uninterrupted
            if (currentDeadline == std::chrono::steady_clock::time_point {})
            {
                return 0;
            }

            if (std::chrono::steady_clock::now() > currentDeadline)
            {
                icError("SBMD script execution timeout: script exceeded the configured time limit");
                MQuickJsRuntime::RecordInterrupt();
                return 1;
            }

            return 0;
        }

} // anonymous namespace

bool MQuickJsRuntime::Initialize(size_t memorySize)
{
    auto &rt = GetInstance();

    if (rt.initialized)
    {
        icDebug("Shared mquickjs context already initialized");
        return true;
    }

    icInfo("Initializing shared mquickjs context for SBMD scripts (%zu bytes)...", memorySize);
    rt.peakHeapUsed = 0;

    // Allocate the memory buffer for the mquickjs context
    rt.memBuffer = static_cast<uint8_t *>(malloc(memorySize));
    if (!rt.memBuffer)
    {
        icError("Failed to allocate %zu bytes for mquickjs context", memorySize);
        return false;
    }
    rt.memSize = memorySize;

    // Create the context with pre-allocated memory and default stdlib
    rt.ctx = JS_NewContext(rt.memBuffer, rt.memSize, &js_stdlib);
    if (!rt.ctx)
    {
        icError("Failed to create shared mquickjs context");
        free(rt.memBuffer);
        rt.memBuffer = nullptr;
        rt.memSize = 0;
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
    bool polyfillFailed = false;
    {
        SafeJSValue urlResult(rt.ctx,
                              JS_Eval(rt.ctx, urlPolyfill, strlen(urlPolyfill), "<url-polyfill>", JS_EVAL_REPL));
        polyfillFailed = JS_IsException(urlResult.Get());
    }

    if (polyfillFailed)
    {
        icError("Failed to install URL polyfill: %s", GetExceptionString(rt.ctx).c_str());
        rt.metrics.RecordJsException("init", nullptr);
        {
            std::lock_guard<std::mutex> lock(rt.mutex);
            LogMemoryUsage("polyfill-failed", IC_LOG_ERROR, true);
        }
        JS_FreeContext(rt.ctx);
        rt.ctx = nullptr;
        free(rt.memBuffer);
        rt.memBuffer = nullptr;
        rt.memSize = 0;
        return false;
    }

    // Check if polyfill installation left an exception
    std::string exMsg;
    if (CheckAndClearPendingException(rt.ctx, &exMsg))
    {
        icError("Polyfill installation left a pending exception: %s - this is a bug", exMsg.c_str());
        rt.metrics.RecordJsException("init", nullptr);
    }

    rt.initialized = true;

    // Install the script execution timeout interrupt handler
    JS_SetInterruptHandler(rt.ctx, ScriptInterruptHandler);
    icDebug("Script execution interrupt handler installed");

    // Install the GC instrumentation callback (passes the metrics instance as opaque)
    JS_SetGCCallback(rt.ctx, &MQuickJsRuntimeMetrics::GCCallback, &rt.metrics);

    {
        std::lock_guard<std::mutex> lock(rt.mutex);
        LogMemoryUsage("post-init (context + stdlib + polyfills)", IC_LOG_DEBUG);

        // Record the arena size as a one-time gauge (arena is fixed after allocation)
        JSMemoryUsage arenaUsage = {};

        if (JS_GetMemoryUsage(rt.ctx, &arenaUsage, 0) == 0)
        {
            rt.metrics.RecordArenaSize(static_cast<int64_t>(arenaUsage.arena_size));
            rt.metrics.RecordHeapSnapshot(arenaUsage, 0);
        }
    }

    // Mark JS context as live — enables ForceSnapshot and the sampler thread
    rt.jsContextReady.store(true, std::memory_order_release);

    // Start the background idle sampler (no-op when sample period is disabled)
    rt.metrics.StartSampler();

    icInfo("Shared mquickjs context initialized successfully");

    return true;
}

void MQuickJsRuntime::Shutdown()
{
    auto &rt = GetInstance();

    if (!rt.initialized)
    {
        return;
    }

    icInfo("Shutting down shared mquickjs context...");

    // Stop the sampler before freeing the context
    rt.jsContextReady.store(false, std::memory_order_release);
    rt.metrics.StopSampler();

    // Hold the JS mutex while freeing the context to prevent a
    // use-after-free race: a ForceSnapshot() caller that passed the
    // jsContextReady early-exit check (before it was cleared above) may
    // still be trying to acquire the mutex.  Holding it here ensures that
    // any such in-flight call either completes before we free ctx, or sees
    // ctx == nullptr afterward and exits cleanly.
    {
        std::lock_guard<std::mutex> lock(rt.mutex);

        if (rt.ctx)
        {
            JS_SetGCCallback(rt.ctx, nullptr, nullptr);
            JS_FreeContext(rt.ctx);
            rt.ctx = nullptr;
        }
    }

    if (rt.memBuffer)
    {
        free(rt.memBuffer);
        rt.memBuffer = nullptr;
        rt.memSize = 0;
    }

    rt.initialized = false;
    icInfo("Shared mquickjs context shutdown complete");
}

JSContext *MQuickJsRuntime::GetSharedContext()
{
    return GetInstance().ctx;
}

std::mutex &MQuickJsRuntime::GetMutex()
{
    return GetInstance().mutex;
}

bool MQuickJsRuntime::IsInitialized()
{
    return GetInstance().initialized;
}

bool MQuickJsRuntime::CheckAndClearPendingException(JSContext *ctx, std::string *outExceptionMsg)
{
    if (!ctx)
    {
        return false;
    }

    JSValue pendingExRaw = JS_GetException(ctx);

    // JS_GetException returns JS_NULL or JS_UNDEFINED when no exception is pending
    bool hasException =
        !JS_IsNull(pendingExRaw) && !JS_IsUndefined(pendingExRaw) && !JS_IsUninitialized(pendingExRaw);

    if (!hasException)
    {
        return false;
    }

    // Root the exception across property reads/conversions; mquickjs can relocate unrooted JSValues.
    SafeJSValue pendingEx(ctx, pendingExRaw);

    // Extract exception message if caller wants it
    if (outExceptionMsg)
    {
        std::string exMsg;

        // Try ToCString for string exceptions
        if (JS_IsString(ctx, pendingEx.Get()))
        {
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, pendingEx.Get(), &buf);
            if (str)
            {
                exMsg = str;
            }
        }
        else if (JS_IsPtr(pendingEx.Get()))
        {
            // Try "message" property for Error objects
            SafeJSValue msgVal(ctx, JS_GetPropertyStr(ctx, pendingEx.Get(), "message"));
            if (JS_IsString(ctx, msgVal.Get()))
            {
                JSCStringBuf buf;
                const char *msgStr = JS_ToCString(ctx, msgVal.Get(), &buf);
                if (msgStr)
                {
                    exMsg = msgStr;
                }
            }

            // Also try to get stack trace for debugging
            SafeJSValue stackVal(ctx, JS_GetPropertyStr(ctx, pendingEx.Get(), "stack"));
            if (JS_IsString(ctx, stackVal.Get()))
            {
                JSCStringBuf buf;
                const char *stackStr = JS_ToCString(ctx, stackVal.Get(), &buf);
                if (stackStr)
                {
                    if (!exMsg.empty())
                    {
                        exMsg += " | Stack: ";
                    }
                    exMsg += stackStr;
                }
            }

            // Pull name/fileName/lineNumber to help localize throws that carry no
            // useful stack (e.g. exceptions raised from native bindings).
            for (const char *prop : {"name", "fileName", "lineNumber"})
            {
                SafeJSValue propVal(ctx, JS_GetPropertyStr(ctx, pendingEx.Get(), prop));
                JSCStringBuf buf;
                const char *propStr = JS_ToCString(ctx, propVal.Get(), &buf);
                if (propStr)
                {
                    exMsg += " | ";
                    exMsg += prop;
                    exMsg += "=";
                    exMsg += propStr;
                }
            }
        }
        else
        {
            // Try ToCString as fallback for other types
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, pendingEx.Get(), &buf);
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
    auto &rt = GetInstance();

    if (!rt.ctx)
    {
        return;
    }

    int flags = walkHeap ? JS_MEMUSAGE_WALK_HEAP : 0;
    JSMemoryUsage usage = {};

    if (JS_GetMemoryUsage(rt.ctx, &usage, flags) != 0)
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

        if (netHeapUsed > rt.peakHeapUsed)
        {
            rt.peakHeapUsed = netHeapUsed;
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
                 rt.peakHeapUsed);
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

void MQuickJsRuntime::SetDeadline(std::chrono::steady_clock::time_point value)
{
    auto &rt = GetInstance();
    rt.scriptInterruptFired.store(false, std::memory_order_relaxed);
    rt.deadline = value;
}

void MQuickJsRuntime::ClearDeadline()
{
    GetInstance().deadline = std::chrono::steady_clock::time_point {};
}

std::chrono::steady_clock::time_point MQuickJsRuntime::GetDeadline()
{
    return GetInstance().deadline;
}

void MQuickJsRuntime::RecordInterrupt()
{
    GetInstance().scriptInterruptFired.store(true, std::memory_order_relaxed);
}

bool MQuickJsRuntime::WasInterrupted()
{
    return GetInstance().scriptInterruptFired.load(std::memory_order_relaxed);
}

bool MQuickJsRuntime::IsContextReady()
{
    return GetInstance().jsContextReady.load(std::memory_order_acquire);
}

void MQuickJsRuntime::RecordMutexWait(double ms)
{
    GetInstance().metrics.RecordMutexWait(ms);
}

void MQuickJsRuntime::RecordJsException(const char *phase, const char *driver)
{
    GetInstance().metrics.RecordJsException(phase, driver);
}

void MQuickJsRuntime::RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount)
{
    GetInstance().metrics.RecordHeapSnapshot(usage, gcRootCount);
}

void MQuickJsRuntime::TickleSampler()
{
    GetInstance().metrics.TickleSampler();
}

void MQuickJsRuntime::ForceSnapshot()
{
    GetInstance().metrics.ForceSnapshot();
}

} // namespace barton
