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
 *   1. Load: Parse .sbmd.js file and extract the registration; its handler references are rooted as
 *      they are extracted.
 *   2. Reduce: ReduceToClaimMetadata() releases those roots, the source text, and the heavy parsed
 *      registration, keeping only the claim metadata. The factory does this right after registration.
 *   3. Activate: Re-read the spec file from disk (or re-use the source if not yet reduced),
 *      re-evaluate it, hold handler JSValues alive, and build the dispatch tables. Ready for dispatch.
 *   4. Deactivate: Release held references and dispatch tables. Back to claim metadata only.
 *
 * The mquickjs context is shared across all drivers — every lifecycle step requires the caller to
 * hold MQuickJsRuntime::Instance().GetMutex().
 */

#pragma once

#include "SbmdDispatch.h"
#include "SbmdRegistration.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    /**
     * Extract the clean driver name from a .sbmd.js file path by stripping the double extension.
     * e.g. "/path/to/door-lock.sbmd.js" → "door-lock"
     */
    inline std::string driverStemFromPath(const std::string &filePath)
    {
        return std::filesystem::path(filePath).stem().stem().string();
    }

    class SbmdDriver
    {
    public:
        /**
         * Why Activate() failed.
         */
        enum class ActivationFailure
        {
            None,
            // The shared JS context is gone (the runtime has been shut down).
            RuntimeUnavailable,
            // The spec file at filePath could not be read.
            ReadFailed,
            // A numeric claim/version field in the re-read spec is not an integer within its range.
            InvalidField,
            // The re-read spec could not be evaluated or its registration could not be extracted.
            LoadFailed,
            // The re-read spec's claim identity no longer matches the claim metadata.
            ClaimMismatch,
            // The re-read spec's device-class version, an endpoint profile version, or its set of
            // endpoint profiles differs from the spec snapshot taken at construction.
            VersionMismatch,
        };

        /**
         * The parts of the spec that must not change while Barton runs, captured from the full
         * registration at construction and never modified afterwards, so (unlike GetRegistration())
         * safe to read without the JS mutex: the claim fields that claim-time callers read, and the
         * versions published to the device-service layer. Activation rejects a re-read spec whose
         * versions differ (VersionMismatch).
         */
        struct SpecSnapshot
        {
            std::vector<uint16_t> deviceTypes;
            std::optional<uint16_t> vendorId;
            std::optional<uint16_t> productId;
            uint32_t deviceClassVersion = 0;
            // One entry per distinct endpoint profile, in declaration order, with the version of
            // the first endpoint that declares that profile.
            std::vector<std::pair<std::string, uint32_t>> profileVersions;
        };

        /**
         * Create a driver from a loaded registration and its source text.
         *
         * The registration should come from SbmdLoader::LoadDriver(), which roots its handler
         * JSValues as it extracts them. Its filePath is made absolute here (against the current
         * working directory), so later re-reads do not depend on the working directory at that time.
         *
         * @param registration The extracted registration (takes ownership)
         * @param source The .sbmd.js file contents. Held only until ReduceToClaimMetadata() or
         *               Deactivate() releases it; after that, Activate() re-reads the spec from
         *               the registration's filePath.
         */
        SbmdDriver(std::unique_ptr<SbmdRegistration> registration, std::string source);

        ~SbmdDriver();

        // Non-copyable; move-constructible only (the snapshot is const)
        SbmdDriver(const SbmdDriver &) = delete;
        SbmdDriver &operator=(const SbmdDriver &) = delete;
        SbmdDriver(SbmdDriver &&) = default;
        SbmdDriver &operator=(SbmdDriver &&) = delete;

        /**
         * Activate the driver — re-evaluate the .sbmd.js file and hold all handler JSValues alive.
         * Equivalent to LoadForActivation() followed by CommitActivation(), reading the spec file
         * itself (with ReadSpecFile(), while the caller holds the JS mutex) if the driver has
         * released its source. Production activation uses the two halves instead, so the read
         * happens before the mutex is taken.
         *
         * After activation, handler functions can be called safely for the driver's active lifetime.
         * Caller must hold MQuickJsRuntime::Instance().GetMutex().
         *
         * @param ctx The mquickjs context; activation fails if it is null (runtime shut down)
         * @param failure If non-null, set to why activation failed (ActivationFailure::None on success)
         * @return true if activation succeeded
         */
        bool Activate(JSContext *ctx, ActivationFailure *failure = nullptr);

        /**
         * First half of activation: evaluate the spec text and check it against the claim metadata and the
         * spec snapshot, without changing this driver's state. Splitting it out lets a caller do the slow
         * part without holding the Matter stack lock (report dispatch still waits on the JS mutex), and
         * take that lock only for the CommitActivation() swap. This function does no file I/O: the caller
         * reads the spec with ReadSpecFile(GetSpecPath()), ideally before taking the JS mutex. Evaluation
         * and extraction run under the SBMD script deadline (BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS); a
         * timeout fails with LoadFailed.
         * Caller must hold MQuickJsRuntime::Instance().GetMutex().
         *
         * @param ctx The mquickjs context; fails with RuntimeUnavailable if it is null
         * @param specText The spec file's contents, or null if reading it failed. Ignored while the
         *                 driver still retains its load-time source; otherwise null fails with
         *                 ReadFailed.
         * @param failure If non-null, set to why loading failed (ActivationFailure::None on success)
         * @return The fresh registration (its handler JSValues rooted), or nullptr on failure
         */
        std::unique_ptr<SbmdRegistration>
        LoadForActivation(JSContext *ctx, const std::string *specText, ActivationFailure *failure = nullptr);

        /**
         * Second half of activation: replace the registration with one from LoadForActivation(), hold
         * its handlers alive, and build the dispatch tables. The driver must not already be active.
         * Caller must hold MQuickJsRuntime::Instance().GetMutex() and pass the same, still-live
         * context the registration was loaded with.
         */
        void CommitActivation(std::unique_ptr<SbmdRegistration> freshReg, JSContext *ctx);

        /**
         * Destroy a registration from LoadForActivation() that will not be committed. Caller must hold
         * MQuickJsRuntime::Instance().GetMutex(). Pass the current shared context; when it is null
         * (the runtime has shut down since loading) the handler references are detached rather than
         * released through the freed context.
         */
        static void DiscardRegistration(std::unique_ptr<SbmdRegistration> unused, JSContext *ctx);

        /**
         * Read a .sbmd.js spec file into out. Only a regular file is read; it is opened non-blocking
         * and checked by descriptor, so a spec path that is a FIFO or device cannot block the caller.
         * Returns false otherwise, on any I/O error, or if the buffer cannot be allocated (an
         * oversized file); it does not throw for those.
         */
        static bool ReadSpecFile(const std::string &path, std::string &out);

        /**
         * Deactivate the driver — release all held handler references and clear handler JSValues.
         *
         * After deactivation, only metadata is available. The driver can be re-activated later.
         * Caller must hold MQuickJsRuntime::Instance().GetMutex().
         *
         * @param ctx The current shared mquickjs context. When null (the runtime has been shut down
         *            and its context freed), the handler references are detached rather than
         *            released, so nothing touches the freed context.
         */
        void Deactivate(JSContext *ctx);

        /**
         * Reduce a loaded-but-inactive driver to its claim metadata: release the load-time handler
         * roots and free the retained source text and heavy parsed registration (endpoints,
         * resources, aliases, handler vectors), keeping only the claim metadata resident.
         *
         * Intended to be called once after load/registration so an unclaimed driver holds only its
         * claim metadata. No-op if the driver is already reduced; refuses to run on an active driver.
         * Caller must hold MQuickJsRuntime::GetMutex() (ReleaseHandlers() releases GC roots).
         */
        void ReduceToClaimMetadata();

        /**
         * Whether the driver is currently activated (handler references are held alive).
         */
        bool IsActivated() const;

        /**
         * Get the driver registration.
         *
         * Only the claim-identity fields (`name`, `filePath`, `barton.deviceClass`, and
         * `matter.deviceTypes`/`vendorId`/`productId`) are valid in every state. While the driver is
         * inactive the remaining fields are released or zeroed (endpoints, aliases, handlers,
         * `reporting`, `featureClusters`, `defaultTimeoutMs`, `schemaVersion`, `driverVersion`,
         * `barton.deviceClassVersion`, `matter.revision`), and handler JSValues are valid only while
         * activated. `Activate()` replaces the underlying registration object, so a reference returned
         * here (including `GetName()`'s, which points into the same object) MUST NOT be held across an
         * activation or deactivation.
         */
        const SbmdRegistration &GetRegistration() const;

        /**
         * Get the spec snapshot taken at construction. Never modified, so it is safe to read without
         * the JS mutex.
         */
        const SpecSnapshot &GetSpecSnapshot() const;

        /**
         * Get the absolute path of this driver's spec file. Fixed at construction, so unlike
         * GetRegistration() it is safe to call without the JS mutex.
         */
        const std::string &GetSpecPath() const;

        /**
         * Get the driver name (convenience — same as registration.name).
         */
        const std::string &GetName() const;

        /**
         * Get the clean driver stem for observability metric attributes.
         * e.g. "door-lock.sbmd.js" → "door-lock"
         *
         * Computed by driverStemFromPath() at construction time; subsequent calls are O(1).
         */
        const std::string &GetDriverStem() const;

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
            VisitHandlers(*registration, std::forward<Fn>(fn));
        }

        /**
         * VisitHandlers() over a given registration.
         */
        template<typename Fn>
        static void VisitHandlers(SbmdRegistration &reg, Fn &&fn)
        {
            for (auto &endpoint : reg.endpoints)
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

            for (auto &handler : reg.attributeHandlers)
            {
                fn(handler);
            }

            for (auto &handler : reg.eventHandlers)
            {
                fn(handler);
            }

            for (auto &handler : reg.commandHandlers)
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
         * Drop every handler's held reference without touching the JS context, for use when the
         * context has already been freed.
         */
        void DetachHandlers();

        /**
         * Hold a handler's JSValue alive if it is not JS_UNDEFINED. On success the handler's
         * heldFn SafeJSValue keeps it alive; callers must invoke through entry.Fn(), not the raw copy.
         */
        void HoldIfValid(JSContext *ctx, SbmdHandler &entry);

        /**
         * Compare a freshly re-evaluated registration's claim-identity fields (name, device class,
         * device types, vendor/product IDs) against this driver's resident claim metadata.
         */
        bool MatchesClaimMetadata(const SbmdRegistration &fresh) const;

        /**
         * Compare a freshly re-evaluated registration's device-class version and endpoint profile
         * versions (and profile set) against the spec snapshot, logging the first difference.
         */
        bool MatchesSnapshotVersions(const SbmdRegistration &fresh) const;

        /**
         * Each distinct endpoint profile's version (the first endpoint's that declares it), in
         * declaration order.
         */
        static std::vector<std::pair<std::string, uint32_t>> ProfileVersionsOf(const SbmdRegistration &reg);

        /**
         * The spec snapshot of a full registration.
         */
        static SpecSnapshot TakeSnapshot(const SbmdRegistration &reg);

        /**
         * Free the retained source text and heavy parsed registration collections, leaving only
         * the claim metadata resident. Handler roots must already be released before calling.
         */
        void ReleaseHeavyRegistration();

        std::unique_ptr<SbmdRegistration> registration;
        std::string source;     // Load-time spec text; released by ReduceToClaimMetadata()/Deactivate()
        std::string driverStem; // Cached at construction from registration->filePath
        std::string specPath;   // Absolute spec path, fixed at construction
        const SpecSnapshot snapshot;

        // Dispatch tables — built at activation, cleared at deactivation
        SbmdDispatchTable attributeDispatch;
        SbmdDispatchTable eventDispatch;
        SbmdDispatchTable commandDispatch;
    };

} // namespace barton
