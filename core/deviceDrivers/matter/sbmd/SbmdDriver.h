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
 *   1. Load: Parse .sbmd.js file, extract metadata. Handler references are not held alive.
 *   2. Activate: Re-evaluate file, hold handler JSValues alive. Ready for dispatch.
 *   3. Deactivate: Release held references, clear handlers. Back to metadata-only.
 *
 * The source text is retained so the file can be re-evaluated on activation.
 * The mquickjs context is shared across all drivers — activation requires
 * the caller to hold MQuickJsRuntime::GetMutex().
 */

#pragma once

#include "SbmdDispatch.h"
#include "SbmdRegistration.h"

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
         * JSValues are present but not yet held alive — they are only valid transiently.
         * Call Activate() to hold them alive.
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
         * Activate the driver — re-evaluate the .sbmd.js file and hold all handler JSValues alive.
         *
         * After activation, handler functions can be called safely for the driver's active lifetime.
         * Caller must hold MQuickJsRuntime::GetMutex().
         *
         * @param ctx The mquickjs context
         * @return true if activation succeeded
         */
        bool Activate(JSContext *ctx);

        /**
         * Deactivate the driver — release all held handler references and clear handler JSValues.
         *
         * After deactivation, only metadata is available. The driver can be re-activated later.
         * Caller must hold MQuickJsRuntime::GetMutex().
         *
         * @param ctx The mquickjs context
         */
        void Deactivate(JSContext *ctx);

        /**
         * Whether the driver is currently activated (handler references are held alive).
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
         * Invoke fn(SbmdHandler&) for every handler in the registration: each resource's
         * seed/read/write/execute plus the attribute/event/command handler vectors.
         */
        template<typename Fn>
        void VisitHandlers(Fn &&fn)
        {
            for (auto &endpoint : registration->endpoints)
            {
                for (auto &resource : endpoint.resources)
                {
                    if (resource.seed.has_value())
                    {
                        fn(*resource.seed);
                    }

                    if (resource.read.has_value())
                    {
                        fn(*resource.read);
                    }

                    if (resource.write.has_value())
                    {
                        fn(*resource.write);
                    }

                    if (resource.execute.has_value())
                    {
                        fn(*resource.execute);
                    }
                }
            }

            for (auto &handler : registration->attributeHandlers)
            {
                fn(handler);
            }

            for (auto &handler : registration->eventHandlers)
            {
                fn(handler);
            }

            for (auto &handler : registration->commandHandlers)
            {
                fn(handler);
            }
        }

        /**
         * Walk the registration and hold all handler JSValues alive.
         */
        void HoldHandlers(JSContext *ctx);

        /**
         * Release every handler's held reference and reset it to the unloaded state.
         */
        void ReleaseHandlers();

        /**
         * Hold a handler's JSValue alive if it is not JS_UNDEFINED. On success the handler's
         * heldFn SafeJSValue keeps it alive; callers must invoke through entry.Fn(), not the raw copy.
         */
        void HoldIfValid(JSContext *ctx, SbmdHandler &entry);

        std::unique_ptr<SbmdRegistration> registration;
        std::string source; // Retained for re-activation

        // Dispatch tables — built at activation, cleared at deactivation
        SbmdDispatchTable attributeDispatch;
        SbmdDispatchTable eventDispatch;
        SbmdDispatchTable commandDispatch;
    };

} // namespace barton
