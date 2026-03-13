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

#include <mutex>
#include <quickjs/quickjs.h>
#include <string>

namespace barton
{
    /**
     * Singleton manager for the shared QuickJS runtime and context used by SBMD scripts.
     *
     * This class provides a single, shared QuickJS runtime and context for all SBMD
     * device scripts. This approach:
     * - Provides thread-safe access via a shared mutex
     * - Isolates scripts by wrapping them in IIFEs (no persistent state between scripts)
     * - Installs browser shims (console, performance, URL) needed for JavaScript libraries
     *
     * Thread Safety:
     * All script execution must be protected by acquiring the mutex via GetMutex().
     * The Initialize() and Shutdown() methods are NOT thread-safe and should only
     * be called during application startup/shutdown.
     */
    class QuickJsRuntime
    {
    public:
        /**
         * Initialize the shared QuickJS runtime and context.
         *
         * This must be called once during application startup before any
         * SBMD scripts are executed. It creates the runtime, context, and
         * installs browser shims.
         *
         * @return true if initialization succeeded, false otherwise
         */
        static bool Initialize();

        /**
         * Shutdown the shared QuickJS runtime and context.
         *
         * This should be called during application shutdown to free resources.
         * After calling this, Initialize() must be called again before using
         * the shared context.
         */
        static void Shutdown();

        /**
         * Get the shared QuickJS context.
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
         * Deep freeze a global object to prevent modifications by scripts.
         * This provides isolation - scripts cannot pollute or modify shared libraries.
         *
         * @param name The name of the global object to freeze
         * @return true if the object was frozen successfully
         */
        static bool FreezeGlobalObject(const char *name);

        /**
         * Check for and clear any pending JavaScript exception.
         *
         * JS_GetException returns JS_NULL or JS_TAG_UNINITIALIZED when no exception
         * is pending - this helper handles both cases correctly.
         *
         * @param ctx The JSContext to check
         * @param outExceptionMsg If non-null and an exception was found, filled with
         *        the exception message (including stack trace if available)
         * @return true if a pending exception was found and cleared, false otherwise
         */
        static bool CheckAndClearPendingException(JSContext *ctx, std::string *outExceptionMsg = nullptr);

    private:
        static bool InstallBrowserShims();

        static JSRuntime *runtime_;
        static JSContext *ctx_;
        static std::mutex mutex_;
        static bool initialized_;
    };

} // namespace barton
