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

#define LOG_TAG     "MQuickJsRuntime"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "MQuickJsRuntime.h"
#include "SbmdJsUtil.h"
#include "matter/sbmd/SafeJSValue.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>

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

    // Static member initialization
    uint8_t *MQuickJsRuntime::memBuffer = nullptr;
    size_t MQuickJsRuntime::memSize = 0;
    JSContext *MQuickJsRuntime::ctx = nullptr;
    std::mutex MQuickJsRuntime::mutex;
    bool MQuickJsRuntime::initialized = false;
    size_t MQuickJsRuntime::peakHeapUsed = 0;
    std::chrono::steady_clock::time_point MQuickJsRuntime::deadline {};

    // Observability metric handles
    ObservabilityHistogram *MQuickJsRuntime::heapUsedHisto = nullptr;
    ObservabilityGauge *MQuickJsRuntime::heapArenaGauge = nullptr;
    ObservabilityGauge *MQuickJsRuntime::heapFreeGauge = nullptr;
    ObservabilityGauge *MQuickJsRuntime::heapPeakGauge = nullptr;
    ObservabilityHistogram *MQuickJsRuntime::mutexWaitHisto = nullptr;
    ObservabilityCounter *MQuickJsRuntime::jsExceptionCounter = nullptr;
    ObservabilityCounter *MQuickJsRuntime::gcCountCounter = nullptr;
    ObservabilityHistogram *MQuickJsRuntime::gcDurationHisto = nullptr;
    ObservabilityGauge *MQuickJsRuntime::gcRootsGauge = nullptr;
    std::chrono::steady_clock::time_point MQuickJsRuntime::gcStartTime {};

    // Idle sampler thread state
    std::thread MQuickJsRuntime::periodicSamplerThread;
    std::atomic<bool> MQuickJsRuntime::samplerShouldStop {false};
    std::atomic<bool> MQuickJsRuntime::jsContextReady {false};
    std::atomic<uint64_t> MQuickJsRuntime::tickleSeq {0};
    std::condition_variable MQuickJsRuntime::samplerCv;
    std::mutex MQuickJsRuntime::samplerCvMutex;
    int64_t MQuickJsRuntime::peakHeapRecorded = 0;
    std::atomic<bool> MQuickJsRuntime::scriptInterruptFired {false};

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
        if (initialized)
        {
            icDebug("Shared mquickjs context already initialized");
            return true;
        }

        icInfo("Initializing shared mquickjs context for SBMD scripts (%zu bytes)...", memorySize);
        peakHeapUsed = 0;
        peakHeapRecorded = 0;

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
        bool polyfillFailed = false;
        {
            SafeJSValue urlResult(ctx, JS_Eval(ctx, urlPolyfill, strlen(urlPolyfill), "<url-polyfill>", JS_EVAL_REPL));
            polyfillFailed = JS_IsException(urlResult.Get());
        }

        if (polyfillFailed)
        {
            icError("Failed to install URL polyfill: %s", GetExceptionString(ctx).c_str());
            RecordJsException("init", nullptr);
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
            RecordJsException("init", nullptr);
        }

        initialized = true;

        // Install the script execution timeout interrupt handler
        JS_SetInterruptHandler(ctx, ScriptInterruptHandler);
        icDebug("Script execution interrupt handler installed");

        // Install the GC instrumentation callback
        JS_SetGCCallback(ctx, &MQuickJsRuntime::GCCallback, nullptr);

        {
            std::lock_guard<std::mutex> lock(mutex);
            LogMemoryUsage("post-init (context + stdlib + polyfills)", IC_LOG_DEBUG);

            // Record the arena size as a one-time gauge (arena is fixed after allocation)
            JSMemoryUsage arenaUsage = {};
            if (JS_GetMemoryUsage(ctx, &arenaUsage, 0) == 0)
            {
                observabilityGaugeRecord(heapArenaGauge, static_cast<int64_t>(arenaUsage.arena_size));
            }
        }

        // Mark JS context as live — enables ForceSnapshot and sampler thread
        jsContextReady.store(true, std::memory_order_release);

        // Start the background idle sampler.  std::thread construction can
        // throw std::system_error under extreme resource pressure; treat that as
        // a non-fatal degradation and leave the sampler disabled rather than
        // propagating an unexpected exception from a bool-returning function.
        samplerShouldStop.store(false, std::memory_order_relaxed);

        try
        {
            periodicSamplerThread = std::thread([]() {
                using clock = std::chrono::steady_clock;

                std::unique_lock<std::mutex> lock(samplerCvMutex);

                while (!samplerShouldStop.load(std::memory_order_relaxed))
                {
                    if (BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS <= 0)
                    {
                        // Periodic sampling disabled: block until tickle or shutdown
                        samplerCv.wait(lock);

                        if (samplerShouldStop.load(std::memory_order_relaxed))
                        {
                            return;
                        }

                        // Tickle or spurious wakeup: continue waiting
                        continue;
                    }

                    auto deadline =
                        clock::now() + std::chrono::milliseconds(BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS);
                    auto lastTickle = tickleSeq.load(std::memory_order_relaxed);

                    while (true)
                    {
                        auto status = samplerCv.wait_until(lock, deadline);

                        if (samplerShouldStop.load(std::memory_order_relaxed))
                        {
                            return;
                        }

                        if (status == std::cv_status::timeout)
                        {
                            // Idle period elapsed — take a snapshot
                            lock.unlock();
                            ForceSnapshot();
                            lock.lock();
                            break;
                        }

                        // Woken early: check if it was a real tickle or spurious
                        auto curTickle = tickleSeq.load(std::memory_order_relaxed);

                        if (curTickle != lastTickle)
                        {
                            // Real tickle: reset the idle timer
                            break;
                        }

                        // Spurious wakeup: continue waiting with same deadline
                    }
                }
            });
        }
        catch (const std::system_error &e)
        {
            icWarn("Failed to start background heap sampler: %s — periodic sampling disabled", e.what());
            // periodicSamplerThread stays default-constructed (not joinable); sampling disabled
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

        // Stop the sampler before freeing the context
        jsContextReady.store(false, std::memory_order_release);
        samplerShouldStop.store(true, std::memory_order_relaxed);
        TickleSampler();

        if (periodicSamplerThread.joinable())
        {
            periodicSamplerThread.join();
        }

        // Hold the JS mutex while freeing the context to prevent a
        // use-after-free race: a ForceSnapshot() caller that passed the
        // jsContextReady early-exit check (before it was cleared above) may
        // still be trying to acquire the mutex.  Holding it here ensures that
        // any such in-flight call either completes before we free ctx, or sees
        // ctx == nullptr afterward and exits cleanly.
        {
            std::lock_guard<std::mutex> lock(mutex);

            if (ctx)
            {
                JS_SetGCCallback(ctx, nullptr, nullptr);
                JS_FreeContext(ctx);
                ctx = nullptr;
            }
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

    void MQuickJsRuntime::SetDeadline(std::chrono::steady_clock::time_point value)
    {
        scriptInterruptFired.store(false, std::memory_order_relaxed);
        deadline = value;
    }

    void MQuickJsRuntime::ClearDeadline()
    {
        deadline = std::chrono::steady_clock::time_point {};
    }

    std::chrono::steady_clock::time_point MQuickJsRuntime::GetDeadline()
    {
        return deadline;
    }

    void MQuickJsRuntime::RecordInterrupt()
    {
        scriptInterruptFired.store(true, std::memory_order_relaxed);
    }

    bool MQuickJsRuntime::WasInterrupted()
    {
        return scriptInterruptFired.load(std::memory_order_relaxed);
    }

    void MQuickJsRuntime::InitializeMetrics()
    {
        if (heapUsedHisto)
        {
            return;
        }

        heapUsedHisto = observabilityHistogramCreate(
            "sbmd.js.heap.used_bytes", "Distribution of mquickjs heap bytes in use at each handler invocation", "By");
        heapArenaGauge =
            observabilityGaugeCreate("sbmd.js.heap.arena_bytes", "Total mquickjs arena size in bytes", "By");
        heapFreeGauge = observabilityGaugeCreate(
            "sbmd.js.heap.free_bytes", "Bytes between top of heap and bottom of JS call stack", "By");
        heapPeakGauge = observabilityGaugeCreate(
            "sbmd.js.heap.peak_bytes", "All-time peak heap bytes observed since last init", "By");
        mutexWaitHisto = observabilityHistogramCreate(
            "sbmd.js.mutex.wait_ms", "Time spent waiting to acquire the JS mutex before a handler call", "ms");
        jsExceptionCounter = observabilityCounterCreate(
            "sbmd.js.exception", "Number of JavaScript exceptions encountered", "{exception}");
        gcCountCounter = observabilityCounterCreate("sbmd.js.gc.count", "Number of GC cycles completed", "{cycle}");
        gcDurationHisto = observabilityHistogramCreate("sbmd.js.gc.duration_ms", "Duration of each GC cycle", "ms");
        gcRootsGauge = observabilityGaugeCreate(
            "sbmd.js.gc_roots", "Live GC roots registered on the JS context (push/pop + add/delete lists)", "{root}");
    }

    void MQuickJsRuntime::ShutdownMetrics()
    {
        // Unregister the GC callback so no further invocations fire against
        // the now-released metric handles.
        if (ctx)
        {
            JS_SetGCCallback(ctx, nullptr, nullptr);
        }

        observabilityHistogramRelease(heapUsedHisto);
        heapUsedHisto = nullptr;
        observabilityGaugeRelease(heapArenaGauge);
        heapArenaGauge = nullptr;
        observabilityGaugeRelease(heapFreeGauge);
        heapFreeGauge = nullptr;
        observabilityGaugeRelease(heapPeakGauge);
        heapPeakGauge = nullptr;
        observabilityHistogramRelease(mutexWaitHisto);
        mutexWaitHisto = nullptr;
        observabilityCounterRelease(jsExceptionCounter);
        jsExceptionCounter = nullptr;
        observabilityCounterRelease(gcCountCounter);
        gcCountCounter = nullptr;
        observabilityHistogramRelease(gcDurationHisto);
        gcDurationHisto = nullptr;
        observabilityGaugeRelease(gcRootsGauge);
        gcRootsGauge = nullptr;
    }

    void MQuickJsRuntime::ForceSnapshot()
    {
        if (!jsContextReady.load(std::memory_order_acquire))
        {
            return;
        }

        if (!heapUsedHisto || !heapFreeGauge || !heapPeakGauge || !gcRootsGauge)
        {
            return;
        }

        JSMemoryUsage usage = {};
        size_t gcRootCount = 0;

        {
            std::lock_guard<std::mutex> lock(mutex);

            if (!ctx)
            {
                return;
            }

            JS_GetMemoryUsage(ctx, &usage, 0);
            gcRootCount = JS_GetGCRootCount(ctx);
            RecordHeapSnapshot(usage, gcRootCount);
        }

        TickleSampler();
    }

    void MQuickJsRuntime::TickleSampler()
    {
        tickleSeq.fetch_add(1, std::memory_order_relaxed);
        samplerCv.notify_one();
    }

    void MQuickJsRuntime::RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount)
    {
        if (!heapUsedHisto)
        {
            return;
        }

        observabilityHistogramRecord(heapUsedHisto, static_cast<double>(usage.heap_used));
        observabilityGaugeRecord(heapFreeGauge, static_cast<int64_t>(usage.free_size));

        if (usage.heap_used > static_cast<size_t>(peakHeapRecorded))
        {
            peakHeapRecorded = static_cast<int64_t>(usage.heap_used);
            observabilityGaugeRecord(heapPeakGauge, peakHeapRecorded);
        }

        observabilityGaugeRecord(gcRootsGauge, static_cast<int64_t>(gcRootCount));
    }

    void MQuickJsRuntime::RecordMutexWait(double ms)
    {
        if (!mutexWaitHisto)
        {
            return;
        }

        observabilityHistogramRecord(mutexWaitHisto, ms);
    }

    void MQuickJsRuntime::RecordJsException(const char *phase, const char *driver)
    {
        if (!jsExceptionCounter)
        {
            return;
        }

        if (driver)
        {
            observabilityCounterAddWithAttrs(jsExceptionCounter, 1, "phase", phase, "driver", driver, nullptr);
        }
        else
        {
            observabilityCounterAddWithAttrs(jsExceptionCounter, 1, "phase", phase, nullptr);
        }
    }

    void MQuickJsRuntime::GCCallback(JSContext * /*ctx*/, int isEnd, void * /*opaque*/) noexcept
    {
        if (isEnd == 0)
        {
            gcStartTime = std::chrono::steady_clock::now();

            return;
        }

        if (gcCountCounter)
        {
            observabilityCounterAdd(gcCountCounter, 1);
        }

        if (gcDurationHisto)
        {
            auto elapsed = std::chrono::steady_clock::now() - gcStartTime;
            double ms = std::chrono::duration<double, std::milli>(elapsed).count();

            observabilityHistogramRecord(gcDurationHisto, ms);
        }
    }

} // namespace barton
