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

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

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
         * Also updates peak tracking. Thread-safe (caller must hold mutex).
         *
         * @param label Descriptive label for the log entry (e.g. "post-init")
         * @param priority Log priority level (e.g. IC_LOG_DEBUG, IC_LOG_ERROR)
         * @param walkHeap If true, walk the heap to compute free block fragmentation
         *                 (expensive O(n) scan). Use on error paths where the extra
         *                 detail justifies the cost.
         */
        static void LogMemoryUsage(const char *label, logPriority priority, bool walkHeap = false);

    private:
        static uint8_t *memBuffer;
        static size_t memSize;
        static JSContext *ctx;
        static std::mutex mutex;
        static bool initialized;
        static size_t peakHeapUsed;
    };

} // namespace barton
