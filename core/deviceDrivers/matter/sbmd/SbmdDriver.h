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

/*
 * Created by tlea on 6/12/2026
 *
 * A SBMD driver instance with activate/deactivate lifecycle.
 *
 * Lifecycle:
 *   1. Load: Parse .sbmd.js file, extract metadata. Handlers are NOT rooted.
 *   2. Activate: Re-evaluate file, GC-root handler JSValues. Ready for dispatch.
 *   3. Deactivate: Release GC roots, clear handlers. Back to metadata-only.
 *
 * The source text is retained so the file can be re-evaluated on activation.
 * The mquickjs context is shared across all drivers — activation requires
 * the caller to hold MQuickJsRuntime::GetMutex().
 */

#pragma once

#include "SbmdDispatch.h"
#include "SbmdRegistration.h"

#include <list>
#include <memory>
#include <string>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    class SbmdDriver
    {
    public:
        /**
         * Create a driver from a loaded registration and its source text.
         *
         * The registration should come from SbmdLoader::LoadDriver(). Its handler
         * JSValues are present but NOT GC-rooted — they are only valid until the next
         * GC cycle. Call Activate() to root them.
         *
         * @param registration The extracted registration (takes ownership)
         * @param source The .sbmd.js file contents (retained for re-activation)
         */
        SbmdDriver(std::unique_ptr<SbmdRegistration> registration, std::string source);

        ~SbmdDriver();

        // Non-copyable, movable
        SbmdDriver(const SbmdDriver &) = delete;
        SbmdDriver &operator=(const SbmdDriver &) = delete;
        SbmdDriver(SbmdDriver &&) = default;
        SbmdDriver &operator=(SbmdDriver &&) = default;

        /**
         * Activate the driver — re-evaluate the .sbmd.js file and GC-root all handler JSValues.
         *
         * After activation, handler functions can be called safely across GC cycles.
         * Caller must hold MQuickJsRuntime::GetMutex().
         *
         * @param ctx The mquickjs context
         * @return true if activation succeeded
         */
        bool Activate(JSContext *ctx);

        /**
         * Deactivate the driver — release all GC roots and clear handler JSValues.
         *
         * After deactivation, only metadata is available. The driver can be re-activated later.
         * Caller must hold MQuickJsRuntime::GetMutex().
         *
         * @param ctx The mquickjs context
         */
        void Deactivate(JSContext *ctx);

        /**
         * Whether the driver is currently activated (handlers are GC-rooted).
         */
        bool IsActivated() const;

        /**
         * Get the driver registration (always available, even when deactivated).
         * Handler JSValues are only valid when activated.
         */
        const SbmdRegistration &GetRegistration() const;

        /**
         * Get the driver name (convenience — same as registration.name).
         */
        const std::string &GetName() const;

        /**
         * Get the attribute dispatch table (only valid when activated).
         */
        const SbmdDispatchTable &GetAttributeDispatch() const;

        /**
         * Get the event dispatch table (only valid when activated).
         */
        const SbmdDispatchTable &GetEventDispatch() const;

        /**
         * Get the command dispatch table (only valid when activated).
         */
        const SbmdDispatchTable &GetCommandDispatch() const;

    private:
        /**
         * Walk the registration and GC-root all handler JSValues.
         */
        void RootHandlers(JSContext *ctx);

        /**
         * Walk the GC ref list, unroot all, clear the list, and reset handler JSValues.
         */
        void UnrootHandlers(JSContext *ctx);

        /**
         * Add a GC root for a handler JSValue if it is not JS_UNDEFINED.
         */
        void RootIfValid(JSContext *ctx, JSValue &handler);

        std::unique_ptr<SbmdRegistration> registration;
        std::string source; // Retained for re-activation

        // GC roots — stable addresses via std::list (vector would invalidate on realloc)
        std::list<JSGCRef> gcRefs;

        // Dispatch tables — built at activation, cleared at deactivation
        SbmdDispatchTable attributeDispatch;
        SbmdDispatchTable eventDispatch;
        SbmdDispatchTable commandDispatch;
    };

} // namespace barton
