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

#include <fstream>

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{
    SbmdDriver::SbmdDriver(std::unique_ptr<SbmdRegistration> registration, std::string source) :
        registration(std::move(registration)), source(std::move(source))
    {
        driverStem = driverStemFromPath(this->registration->filePath);
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
                VisitHandlers([](SbmdHandler &entry) { entry.heldFn.Detach(); });
            }
            else
            {
                VisitHandlers([](SbmdHandler &entry) { entry.heldFn = SafeJSValue {}; });
            }
        }
    }

    bool SbmdDriver::Activate(JSContext *ctx)
    {
        if (registration->activated)
        {
            icWarn("driver '%s' already activated", registration->name.c_str());
            return true;
        }

        icDebug("activating driver '%s'", registration->name.c_str());

        // Obtain the spec content: re-use the retained source if present, otherwise read the spec
        // file from disk. A shrunk (claim-stub) driver has released its source, so it re-reads.
        std::string diskSource;
        const char *specData = nullptr;
        size_t specLen = 0;

        if (!source.empty())
        {
            specData = source.c_str();
            specLen = source.size();
        }
        else if (ReadSpecFromDisk(diskSource))
        {
            specData = diskSource.c_str();
            specLen = diskSource.size();
        }
        else
        {
            icError("failed to read spec file '%s' during activation of driver '%s'",
                    registration->filePath.c_str(),
                    registration->name.c_str());
            return false;
        }

        // Re-evaluate the source to get fresh handler JSValues
        auto freshReg = SbmdLoader::LoadDriver(ctx, registration->filePath, specData, specLen);

        if (!freshReg)
        {
            icError("failed to re-evaluate driver '%s' during activation", registration->name.c_str());
            return false;
        }

        // The spec on disk may have changed since load. Refuse to activate against metadata that no
        // longer matches what the device claimed rather than dispatching to a mismatched driver.
        if (!MatchesClaimStub(*freshReg))
        {
            icError("driver '%s' spec no longer matches its claim metadata; refusing to activate",
                    registration->name.c_str());
            return false;
        }

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

        return true;
    }

    void SbmdDriver::Deactivate(JSContext *ctx)
    {
        if (!registration->activated)
        {
            icWarn("driver '%s' already deactivated", registration->name.c_str());
            return;
        }

        icDebug("deactivating driver '%s'", registration->name.c_str());

        ReleaseHandlers();

        attributeDispatch.Clear();
        eventDispatch.Clear();
        commandDispatch.Clear();

        ReleaseHeavyRegistration();

        registration->activated = false;
    }

    void SbmdDriver::Shrink()
    {
        if (registration->activated)
        {
            icWarn("refusing to shrink active driver '%s'", registration->name.c_str());
            return;
        }

        // Already a stub: source released and heavy collections empty.
        if (source.empty() && registration->endpoints.empty() && registration->aliases.empty() &&
            registration->attributeHandlers.empty() && registration->eventHandlers.empty() &&
            registration->commandHandlers.empty())
        {
            return;
        }

        icDebug("shrinking driver '%s' to claim stub", registration->name.c_str());

        // Release the load-time handler roots before freeing the collections that hold them.
        ReleaseHandlers();

        ReleaseHeavyRegistration();
    }

    bool SbmdDriver::ReadSpecFromDisk(std::string &out) const
    {
        std::ifstream file(registration->filePath, std::ios::binary | std::ios::ate);

        if (!file.is_open())
        {
            return false;
        }

        auto fileSize = file.tellg();

        if (fileSize < 0)
        {
            return false;
        }

        file.seekg(0, std::ios::beg);
        out.assign(static_cast<size_t>(fileSize), '\0');
        file.read(out.data(), static_cast<std::streamsize>(fileSize));

        return static_cast<bool>(file);
    }

    bool SbmdDriver::MatchesClaimStub(const SbmdRegistration &fresh) const
    {
        const auto &stub = *registration;

        return fresh.name == stub.name && fresh.barton.deviceClass == stub.barton.deviceClass &&
               fresh.matter.deviceTypes == stub.matter.deviceTypes && fresh.matter.vendorId == stub.matter.vendorId &&
               fresh.matter.productId == stub.matter.productId;
    }

    void SbmdDriver::ReleaseHeavyRegistration()
    {
        // Reduce the registration to its claim stub — name, file path, device class, and the matter
        // device-type/vendor/product IDs used for claiming — releasing the source, the heavy parsed
        // collections, and every non-claim metadata field (swap-with-empty releases capacity). A
        // subsequent activation re-reads the spec from disk and repopulates everything.
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

    const SbmdRegistration &SbmdDriver::GetRegistration() const
    {
        return *registration;
    }

    const std::string &SbmdDriver::GetName() const
    {
        return registration->name;
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

} // namespace barton
