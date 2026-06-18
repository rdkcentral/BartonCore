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

#define LOG_TAG "SbmdDriver"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdDriver.h"
#include "mquickjs/SbmdLoader.h"

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{
    SbmdDriver::SbmdDriver(std::unique_ptr<SbmdRegistration> registration, std::string source)
        : registration(std::move(registration)), source(std::move(source))
    {
    }

    SbmdDriver::~SbmdDriver()
    {
        // If still activated at destruction, the GC refs are leaked.
        // This shouldn't happen in normal operation.
        if (registration && registration->activated)
        {
            icWarn("driver '%s' destroyed while still activated", registration->name.c_str());
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

        // Re-evaluate the source to get fresh handler JSValues
        auto freshReg = SbmdLoader::LoadDriver(ctx, registration->filePath, source.c_str(), source.size());

        if (!freshReg)
        {
            icError("failed to re-evaluate driver '%s' during activation", registration->name.c_str());
            return false;
        }

        // Replace registration with the fresh one (preserves metadata, gets new handler JSValues)
        registration = std::move(freshReg);

        // GC-root all handler JSValues
        RootHandlers(ctx);

        // Build dispatch tables from aliases and handlers
        attributeDispatch.Build(registration->aliases, registration->attributeHandlers);
        eventDispatch.Build(registration->aliases, registration->eventHandlers);
        commandDispatch.Build(registration->aliases, registration->commandHandlers);

        registration->activated = true;
        icDebug("driver '%s' activated with %zu GC roots, dispatch: %zu attr, %zu event, %zu cmd entries",
                registration->name.c_str(),
                gcRefs.size(),
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

        UnrootHandlers(ctx);

        attributeDispatch.Clear();
        eventDispatch.Clear();
        commandDispatch.Clear();

        registration->activated = false;
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

    void SbmdDriver::RootIfValid(JSContext *ctx, JSValue &handler)
    {
        if (JS_IsUndefined(handler))
        {
            return;
        }

        auto &ref = gcRefs.emplace_back();
        JS_AddGCRef(ctx, &ref);
        ref.val = handler;
    }

    void SbmdDriver::RootHandlers(JSContext *ctx)
    {
        gcRefs.clear();

        // Root resource handlers across all endpoints
        for (auto &endpoint : registration->endpoints)
        {
            for (auto &resource : endpoint.resources)
            {
                if (resource.seed.has_value())
                {
                    RootIfValid(ctx, resource.seed->handler);
                }

                if (resource.read.has_value())
                {
                    RootIfValid(ctx, resource.read->handler);
                }

                if (resource.write.has_value())
                {
                    RootIfValid(ctx, resource.write->handler);
                }

                if (resource.execute.has_value())
                {
                    RootIfValid(ctx, resource.execute->handler);
                }
            }
        }

        // Root device handlers (attribute, event, command)
        for (auto &handler : registration->attributeHandlers)
        {
            RootIfValid(ctx, handler.handler);
        }

        for (auto &handler : registration->eventHandlers)
        {
            RootIfValid(ctx, handler.handler);
        }

        for (auto &handler : registration->commandHandlers)
        {
            RootIfValid(ctx, handler.handler);
        }
    }

    void SbmdDriver::UnrootHandlers(JSContext *ctx)
    {
        // Remove all GC roots
        for (auto &ref : gcRefs)
        {
            JS_DeleteGCRef(ctx, &ref);
        }

        gcRefs.clear();

        // Reset handler JSValues to undefined
        for (auto &endpoint : registration->endpoints)
        {
            for (auto &resource : endpoint.resources)
            {
                if (resource.seed.has_value())
                {
                    resource.seed->handler = JS_UNDEFINED;
                }

                if (resource.read.has_value())
                {
                    resource.read->handler = JS_UNDEFINED;
                }

                if (resource.write.has_value())
                {
                    resource.write->handler = JS_UNDEFINED;
                }

                if (resource.execute.has_value())
                {
                    resource.execute->handler = JS_UNDEFINED;
                }
            }
        }

        for (auto &handler : registration->attributeHandlers)
        {
            handler.handler = JS_UNDEFINED;
        }

        for (auto &handler : registration->eventHandlers)
        {
            handler.handler = JS_UNDEFINED;
        }

        for (auto &handler : registration->commandHandlers)
        {
            handler.handler = JS_UNDEFINED;
        }
    }

} // namespace barton
