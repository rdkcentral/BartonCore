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
 */

#define LOG_TAG     "SbmdDriver"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdDriver.h"
#include "mquickjs/MQuickJsRuntime.h"
#include "mquickjs/SbmdLoader.h"

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include <icLog/logging.h>
}

namespace
{
    using ProfileVersions = std::vector<std::pair<std::string, uint32_t>>;

    ProfileVersions::const_iterator FindProfile(const ProfileVersions &versions, const std::string &profile)
    {
        for (auto it = versions.begin(); it != versions.end(); ++it)
        {
            if (it->first == profile)
            {
                return it;
            }
        }

        return versions.end();
    }
} // namespace

namespace barton
{
    SbmdDriver::SbmdDriver(std::unique_ptr<SbmdRegistration> registration, std::string source) :
        registration(std::move(registration)), source(std::move(source)), snapshot(TakeSnapshot(*this->registration))
    {
        // Activation re-reads the spec from filePath long after load, so record it as an absolute
        // path now: a path relative to a configured sbmd_dirs entry would otherwise resolve against
        // whatever the process's working directory is at bind time.
        // Only prepend the working directory: removing ".." textually (lexically_normal) would
        // change which file the path names when it passes through a symlink.
        std::error_code ec;
        auto absolutePath = std::filesystem::absolute(this->registration->filePath, ec);

        if (ec)
        {
            icWarn("could not make spec path '%s' absolute (%s); a later re-read uses it as is",
                   this->registration->filePath.c_str(),
                   ec.message().c_str());
        }
        else
        {
            this->registration->filePath = absolutePath.string();
        }

        specPath = this->registration->filePath;
        driverStem = driverStemFromPath(specPath);
    }

    SbmdDriver::~SbmdDriver()
    {
        // Handler references are rooted (as GC roots) at extraction/activation time, so even a
        // loaded-but-never-activated registration can still own roots here.
        if (registration && registration->activated)
        {
            icWarn("driver '%s' destroyed while still activated", registration->name.c_str());
        }

        // Clear held roots while holding the runtime mutex when the shared context is alive.
        // If the context is already gone, detach so SafeJSValue destructors do not call JS_DeleteGCRef
        // on a dead context.
        if (registration)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());

            if (MQuickJsRuntime::Instance().GetSharedContext() == nullptr)
            {
                DetachHandlers();
            }
            else
            {
                VisitHandlers([](SbmdHandler &entry) { entry.heldFn = SafeJSValue {}; });
            }
        }
    }

    bool SbmdDriver::Activate(JSContext *ctx, ActivationFailure *failure)
    {
        if (registration->activated)
        {
            icWarn("driver '%s' already activated", registration->name.c_str());

            if (failure != nullptr)
            {
                *failure = ActivationFailure::None;
            }

            return true;
        }

        // Only a driver reduced to its claim metadata needs the file; one that retains its source
        // re-evaluates that.
        std::string specText;
        const bool haveSpec = !source.empty() || ReadSpecFile(specPath, specText);
        auto freshReg = LoadForActivation(ctx, haveSpec ? &specText : nullptr, failure);

        if (!freshReg)
        {
            return false;
        }

        CommitActivation(std::move(freshReg), ctx);

        return true;
    }

    std::unique_ptr<SbmdRegistration>
    SbmdDriver::LoadForActivation(JSContext *ctx, const std::string *specText, ActivationFailure *failure)
    {
        ActivationFailure unusedFailure = ActivationFailure::None;
        ActivationFailure &activationFailure = failure != nullptr ? *failure : unusedFailure;
        activationFailure = ActivationFailure::None;

        if (ctx == nullptr)
        {
            icError("cannot activate driver '%s': the JS runtime is not available", registration->name.c_str());
            activationFailure = ActivationFailure::RuntimeUnavailable;
            return nullptr;
        }

        icDebug("loading driver '%s' for activation", registration->name.c_str());

        try
        {
            // Obtain the spec content: re-use the retained source if present, otherwise the spec text
            // the caller read. A driver reduced to its claim metadata has released its source.
            const char *specData = nullptr;
            size_t specLen = 0;

            if (!source.empty())
            {
                specData = source.c_str();
                specLen = source.size();
            }
            else if (specText != nullptr)
            {
                specData = specText->c_str();
                specLen = specText->size();
            }
            else
            {
                icError("failed to read spec file '%s' during activation of driver '%s'",
                        registration->filePath.c_str(),
                        registration->name.c_str());
                activationFailure = ActivationFailure::ReadFailed;
                return nullptr;
            }

            // Re-evaluate the source to get fresh handler JSValues. Unlike the startup load, this runs
            // on a binding thread while Matter-thread dispatch may be waiting on the JS mutex, so a
            // spec edited to never finish evaluating must time out like any other SBMD script.
            SbmdLoadFailure loadFailure = SbmdLoadFailure::None;
            std::unique_ptr<SbmdRegistration> freshReg;
            bool timedOut = false;

            {
                struct ScopedDeadline
                {
                    ScopedDeadline()
                    {
                        MQuickJsRuntime::Instance().SetDeadline(
                            std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS));
                    }

                    ~ScopedDeadline() { MQuickJsRuntime::Instance().ClearDeadline(); }
                } deadline;

                freshReg = SbmdLoader::LoadDriver(ctx, registration->filePath, specData, specLen, &loadFailure);
                timedOut = MQuickJsRuntime::Instance().WasTimedOut();
            }

            // An interrupted property read during extraction looks like an absent property, so a
            // timeout anywhere in the load means the registration may be incomplete: discard it.
            if (timedOut)
            {
                icError("re-evaluating driver '%s' during activation exceeded the SBMD script time limit",
                        registration->name.c_str());
                DiscardRegistration(std::move(freshReg), ctx);
                MQuickJsRuntime::Instance().CheckAndClearPendingException(ctx);
                activationFailure = ActivationFailure::LoadFailed;
                return nullptr;
            }

            if (!freshReg)
            {
                icError("failed to re-evaluate driver '%s' during activation", registration->name.c_str());
                activationFailure = loadFailure == SbmdLoadFailure::InvalidField ? ActivationFailure::InvalidField
                                                                                 : ActivationFailure::LoadFailed;
                return nullptr;
            }

            // The spec on disk may have changed since load. Refuse to activate against metadata that
            // no longer matches what the device claimed rather than dispatching to a mismatched driver.
            if (!MatchesClaimMetadata(*freshReg))
            {
                icError("driver '%s' spec no longer matches its claim metadata; refusing to activate",
                        registration->name.c_str());
                activationFailure = ActivationFailure::ClaimMismatch;
                return nullptr;
            }

            // The versions were published to the device-service layer at construction; a re-read
            // spec that changed them would run new handlers while commissioning and reconfiguration
            // still use the old versions.
            if (!MatchesSnapshotVersions(*freshReg))
            {
                activationFailure = ActivationFailure::VersionMismatch;
                return nullptr;
            }

            return freshReg;
        }
        catch (const std::exception &e)
        {
            // e.g. std::bad_alloc evaluating an unexpectedly large spec
            icError("failed to load driver '%s' for activation: %s", registration->name.c_str(), e.what());
            activationFailure = ActivationFailure::LoadFailed;
            return nullptr;
        }
    }

    void SbmdDriver::CommitActivation(std::unique_ptr<SbmdRegistration> freshReg, JSContext *ctx)
    {
        // Replace registration with the fresh one (preserves metadata, gets new handler JSValues)
        registration = std::move(freshReg);

        // Hold all handler JSValues alive
        HoldHandlers(ctx);

        // Build dispatch tables from aliases and handlers
        attributeDispatch.Build(registration->aliases, registration->attributeHandlers);
        eventDispatch.Build(registration->aliases, registration->eventHandlers);
        commandDispatch.Build(registration->aliases, registration->commandHandlers);

        registration->activated = true;
        icDebug("driver '%s' activated, dispatch: %zu attr, %zu event, %zu cmd entries",
                registration->name.c_str(),
                attributeDispatch.GetSpecificEntryCount() + attributeDispatch.GetWildcardEntryCount(),
                eventDispatch.GetSpecificEntryCount() + eventDispatch.GetWildcardEntryCount(),
                commandDispatch.GetSpecificEntryCount() + commandDispatch.GetWildcardEntryCount());
    }

    void SbmdDriver::DiscardRegistration(std::unique_ptr<SbmdRegistration> unused, JSContext *ctx)
    {
        if (!unused)
        {
            return;
        }

        // Without a live context its handler roots point into freed memory; detach instead of release.
        if (ctx == nullptr)
        {
            VisitHandlers(*unused, [](SbmdHandler &entry) { entry.heldFn.Detach(); });
        }

        unused.reset();
    }

    void SbmdDriver::Deactivate(JSContext *ctx)
    {
        if (!registration->activated)
        {
            icWarn("driver '%s' already deactivated", registration->name.c_str());
            return;
        }

        icDebug("deactivating driver '%s'", registration->name.c_str());

        // With the runtime shut down the handler roots point into a freed context; detach them so
        // neither this call nor the collections freed below touch it.
        if (ctx == nullptr)
        {
            DetachHandlers();
        }
        else
        {
            ReleaseHandlers();
        }

        attributeDispatch.Clear();
        eventDispatch.Clear();
        commandDispatch.Clear();

        ReleaseHeavyRegistration();

        registration->activated = false;
    }

    void SbmdDriver::ReduceToClaimMetadata()
    {
        if (registration->activated)
        {
            icWarn("refusing to reduce active driver '%s'", registration->name.c_str());
            return;
        }

        // Already reduced: source released and heavy collections empty.
        if (source.empty() && registration->IsReducedToClaimMetadata())
        {
            return;
        }

        icDebug("reducing driver '%s' to its claim metadata", registration->name.c_str());

        // Release the load-time handler roots before freeing the collections that hold them.
        ReleaseHandlers();

        ReleaseHeavyRegistration();
    }

    bool SbmdDriver::ReadSpecFile(const std::string &path, std::string &out)
    {
        // Open non-blocking and accept only a regular file, so a spec path that is a FIFO or device
        // cannot block the reading thread (startup, or a binding thread during activation).
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

        if (fd < 0)
        {
            return false;
        }

        struct FdCloser
        {
            int fd;

            ~FdCloser() { close(fd); }
        } closer {fd};

        struct stat info = {};

        if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0)
        {
            icError("spec '%s' is not a regular file", path.c_str());
            return false;
        }

        try
        {
            out.assign(static_cast<size_t>(info.st_size), '\0');
        }
        catch (const std::exception &e)
        {
            // An oversized file can make the allocation throw; fail this read rather than the caller.
            icError("cannot read spec '%s': %s", path.c_str(), e.what());
            std::string().swap(out);
            return false;
        }

        size_t total = 0;

        while (total < out.size())
        {
            ssize_t n = read(fd, out.data() + total, out.size() - total);

            if (n < 0 && errno == EINTR)
            {
                continue;
            }

            if (n <= 0)
            {
                return false;
            }

            total += static_cast<size_t>(n);
        }

        return true;
    }

    bool SbmdDriver::MatchesClaimMetadata(const SbmdRegistration &fresh) const
    {
        const auto &claim = *registration;

        return fresh.name == claim.name && fresh.barton.deviceClass == claim.barton.deviceClass &&
               fresh.matter.deviceTypes == claim.matter.deviceTypes && fresh.matter.vendorId == claim.matter.vendorId &&
               fresh.matter.productId == claim.matter.productId;
    }

    bool SbmdDriver::MatchesSnapshotVersions(const SbmdRegistration &fresh) const
    {
        const auto &name = registration->name;

        if (fresh.barton.deviceClassVersion != snapshot.deviceClassVersion)
        {
            icError("driver '%s' device-class version changed on disk (loaded %" PRIu32 ", spec %" PRIu32
                    "); refusing to activate",
                    name.c_str(),
                    snapshot.deviceClassVersion,
                    fresh.barton.deviceClassVersion);
            return false;
        }

        const auto freshProfiles = ProfileVersionsOf(fresh);

        // Both lists hold each profile once, so equal sizes plus every fresh profile being found
        // means the two profile sets are the same, whatever order the spec declares them in.
        if (freshProfiles.size() != snapshot.profileVersions.size())
        {
            icError("driver '%s' endpoint profile set changed on disk (loaded %zu profiles, spec %zu); refusing "
                    "to activate",
                    name.c_str(),
                    snapshot.profileVersions.size(),
                    freshProfiles.size());
            return false;
        }

        for (const auto &[profile, freshVersion] : freshProfiles)
        {
            const auto loaded = FindProfile(snapshot.profileVersions, profile);

            if (loaded == snapshot.profileVersions.end())
            {
                icError("driver '%s' endpoint profile set changed on disk (new profile '%s'); refusing to activate",
                        name.c_str(),
                        profile.c_str());
                return false;
            }

            if (freshVersion != loaded->second)
            {
                icError("driver '%s' profile '%s' version changed on disk (loaded %" PRIu32 ", spec %" PRIu32
                        "); refusing to activate",
                        name.c_str(),
                        profile.c_str(),
                        loaded->second,
                        freshVersion);
                return false;
            }
        }

        return true;
    }

    std::vector<std::pair<std::string, uint32_t>> SbmdDriver::ProfileVersionsOf(const SbmdRegistration &reg)
    {
        std::vector<std::pair<std::string, uint32_t>> versions;

        for (const auto &endpoint : reg.endpoints)
        {
            if (FindProfile(versions, endpoint.profile) == versions.end())
            {
                versions.emplace_back(endpoint.profile, endpoint.profileVersion);
            }
        }

        return versions;
    }

    SbmdDriver::SpecSnapshot SbmdDriver::TakeSnapshot(const SbmdRegistration &reg)
    {
        SpecSnapshot taken;
        taken.deviceTypes = reg.matter.deviceTypes;
        taken.vendorId = reg.matter.vendorId;
        taken.productId = reg.matter.productId;
        taken.deviceClassVersion = reg.barton.deviceClassVersion;
        taken.profileVersions = ProfileVersionsOf(reg);

        return taken;
    }

    void SbmdDriver::ReleaseHeavyRegistration()
    {
        // Reduce the registration to its claim metadata — name, file path, device class, and the
        // matter device-type/vendor/product IDs used for claiming — releasing the source, the heavy
        // parsed collections, and every non-claim metadata field (swap-with-empty releases capacity).
        // A subsequent activation re-reads the spec file and repopulates everything.
        std::string().swap(source);

        registration->schemaVersion.clear();
        registration->schemaVersion.shrink_to_fit();
        registration->driverVersion = 0;
        registration->barton.deviceClassVersion = 0;
        registration->matter.revision.reset();
        std::vector<uint32_t>().swap(registration->matter.featureClusters);
        registration->matter.defaultTimeoutMs.reset();
        registration->reporting = SbmdReporting {};

        std::vector<SbmdEndpoint>().swap(registration->endpoints);
        std::unordered_map<std::string, SbmdAlias>().swap(registration->aliases);
        std::vector<SbmdDeviceHandler>().swap(registration->attributeHandlers);
        std::vector<SbmdDeviceHandler>().swap(registration->eventHandlers);
        std::vector<SbmdDeviceHandler>().swap(registration->commandHandlers);
    }

    bool SbmdDriver::IsActivated() const
    {
        return registration && registration->activated;
    }

    const SbmdDriver::SpecSnapshot &SbmdDriver::GetSpecSnapshot() const
    {
        return snapshot;
    }

    const SbmdRegistration &SbmdDriver::GetRegistration() const
    {
        return *registration;
    }

    const std::string &SbmdDriver::GetName() const
    {
        return registration->name;
    }

    const std::string &SbmdDriver::GetSpecPath() const
    {
        return specPath;
    }

    const std::string &SbmdDriver::GetDriverStem() const
    {
        return driverStem;
    }

    const SbmdDispatchTable &SbmdDriver::GetAttributeDispatch() const
    {
        return attributeDispatch;
    }

    const SbmdDispatchTable &SbmdDriver::GetEventDispatch() const
    {
        return eventDispatch;
    }

    const SbmdDispatchTable &SbmdDriver::GetCommandDispatch() const
    {
        return commandDispatch;
    }

    void SbmdDriver::HoldIfValid(JSContext *ctx, SbmdHandler &entry)
    {
        // Handlers are rooted at extraction time, so heldFn is normally already valid here. Keep
        // that root rather than re-holding from the raw handler slot, which may have gone stale
        // after GC relocations during extraction.
        if (entry.heldFn.HasValue())
        {
            return;
        }

        if (JS_IsUndefined(entry.handler))
        {
            entry.heldFn = SafeJSValue {};
            return;
        }

        // SafeJSValue keeps the function object alive, so entry.Fn() always returns a valid function.
        entry.heldFn = SafeJSValue {ctx, entry.handler};
    }

    void SbmdDriver::HoldHandlers(JSContext *ctx)
    {
        VisitHandlers([&](SbmdHandler &entry) { HoldIfValid(ctx, entry); });
    }

    void SbmdDriver::ReleaseHandlers()
    {
        // Resetting each handler releases its SafeJSValue, dropping the held reference.
        VisitHandlers([](SbmdHandler &entry) { entry.Reset(); });
    }

    void SbmdDriver::DetachHandlers()
    {
        VisitHandlers([](SbmdHandler &entry) { entry.heldFn.Detach(); });
    }

} // namespace barton
