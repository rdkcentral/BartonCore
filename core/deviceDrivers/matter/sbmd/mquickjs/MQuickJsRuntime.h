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

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

#include "matter/sbmd/metrics/MQuickJsRuntimeMetrics.h"

namespace barton
{
    /**
     * Singleton manager for the shared mquickjs context used by SBMD scripts.
     *
     * This class provides a single, shared mquickjs context for all SBMD
     * device scripts. This approach:
     * - Provides thread-safe access via a shared mutex
     * - Isolates scripts by wrapping them in IIFEs (no persistent state between scripts)
     * - Uses the mquickjs default stdlib which provides console.log, performance.now, etc.
     *
     * Memory Model:
     * mquickjs uses a pre-allocated memory buffer. The context, all JavaScript objects,
     * and GC state live within this buffer. The buffer size is set at initialization
     * and cannot grow.
     *
     * Thread Safety:
     * All script execution must be protected by acquiring the mutex via GetMutex().
     * The Initialize() and Shutdown() methods are NOT thread-safe and should only
     * be called during application startup/shutdown.
     */
    class MQuickJsRuntime
    {
    public:
        /**
         * Initialize the shared mquickjs context.
         *
         * This must be called once during application startup before any
         * SBMD scripts are executed. It allocates the memory buffer and
         * creates the JS context with the default stdlib.
         *
         * @param memorySize Size in bytes of the pre-allocated memory buffer
         * @return true if initialization succeeded, false otherwise
         */
        static bool Initialize(size_t memorySize);

        /**
         * Shutdown the shared mquickjs context.
         *
         * This should be called during application shutdown to free resources.
         * After calling this, Initialize() must be called again before using
         * the shared context.
         */
        static void Shutdown();

        /**
         * Get the shared mquickjs context.
         *
         * @return The shared JSContext, or nullptr if not initialized
         */
        static JSContext *GetSharedContext();

        /**
         * Get the mutex for thread-safe access to the shared context.
         *
         * Callers must lock this mutex before calling GetSharedContext() or
         * executing any scripts.
         *
         * @return Reference to the shared mutex
         */
        static std::mutex &GetMutex();

        /**
         * Check if the shared context has been initialized.
         *
         * @return true if Initialize() has been called successfully
         */
        static bool IsInitialized();

        /**
         * Check for and clear any pending JavaScript exception.
         *
         * @param ctx The JSContext to check
         * @param outExceptionMsg If non-null and an exception was found, filled with
         *        the exception message (including stack trace if available)
         * @return true if a pending exception was found and cleared, false otherwise
         */
        static bool CheckAndClearPendingException(JSContext *ctx, std::string *outExceptionMsg = nullptr);

        /**
         * Log current mquickjs memory usage at the given label.
         *
         * Not thread-safe: callers must hold MQuickJsRuntime::GetMutex() while
         * calling this function.
         *
         * @param label Descriptive label for the log entry (e.g. "post-init")
         * @param priority Log priority level (e.g. IC_LOG_DEBUG, IC_LOG_ERROR)
         * @param walkHeap If true, walk the heap to compute free block fragmentation
         *                 (expensive O(n) scan). Use on error paths where the extra
         *                 detail justifies the cost.
         */
        static void LogMemoryUsage(const char *label, logPriority priority, bool walkHeap = false);

        /**
         * Set the script execution deadline.
         *
         * When set, the interrupt handler will terminate any running script
         * once steady_clock::now() exceeds the deadline. Call ClearDeadline()
         * to disable timeout enforcement.
         *
         * Must be called while holding GetMutex().
         *
         * @param deadline The absolute time point at which to interrupt
         */
        static void SetDeadline(std::chrono::steady_clock::time_point deadline);

        /**
         * Clear the script execution deadline, disabling timeout enforcement.
         *
         * Must be called while holding GetMutex().
         */
        static void ClearDeadline();

        /**
         * Get the current script execution deadline.
         *
         * @return The current deadline, or epoch if no deadline is active
         */
        static std::chrono::steady_clock::time_point GetDeadline();

        /**
         * Mark that the script interrupt handler fired (deadline exceeded).
         *
         * Public only because ScriptInterruptHandler is a C-compatible callback in an
         * anonymous namespace and cannot be a friend. Do not call this from anywhere
         * other than ScriptInterruptHandler.
         */
        static void RecordInterrupt();

        /**
         * Return whether the interrupt handler has fired since the last SetDeadline call.
         *
         * @return true if ScriptInterruptHandler fired for the current JS_Call
         */
        static bool WasInterrupted();

        /**
         * Return true when the JS context is live and accepting snapshots.
         *
         * Set to true (memory_order_release) at the end of Initialize(), cleared
         * to false (memory_order_release) at the start of Shutdown().
         */
        static bool IsContextReady();

        /**
         * Record a JS mutex wait duration in milliseconds.
         * Called from AcquireJsMutex() in SpecBasedMatterDeviceDriver.cpp.
         */
        static void RecordMutexWait(double ms);

        /**
         * Record a JS exception event.
         * @param phase  "init" or "loading"
         * @param driver Filename stem, or nullptr to omit the "driver" attribute
         */
        static void RecordJsException(const char *phase, const char *driver);

        /**
         * Record pool health metrics from an already-captured JSMemoryUsage.
         */
        static void RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount);

        /**
         * Reset the idle sampler timer without waiting for the full period.
         */
        static void TickleSampler();

        /**
         * Synchronously sample heap stats and record pool health metrics.
         * Delegates to the metrics member. Called from unit tests.
         */
        static void ForceSnapshot();

    private:
        MQuickJsRuntime() = delete;

        inline static uint8_t *memBuffer = nullptr;
        inline static size_t memSize = 0;
        inline static JSContext *ctx = nullptr;
        inline static std::mutex mutex;
        inline static bool initialized = false;
        inline static size_t peakHeapUsed = 0;
        inline static std::chrono::steady_clock::time_point deadline;
        inline static std::atomic<bool> scriptInterruptFired {false};

        // jsContextReady: set true at end of Initialize(), cleared at start of
        // Shutdown().
        inline static std::atomic<bool> jsContextReady {false};

        inline static MQuickJsRuntimeMetrics metrics;
    };

} // namespace barton
