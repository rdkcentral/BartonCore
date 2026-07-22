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
// Created by tlea on 2/19/26
//

#define LOG_TAG "SbmdBundleLoader"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdBundleLoader.h"
#include "MQuickJsRuntime.h"
#include "SbmdJsUtil.h"

#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

// Generated at build time.
#include "SbmdBundleEmbedded.h"

namespace barton
{
    using namespace mquickjs;

    bool SbmdBundleLoader::LoadBundle(JSContext *ctx)
    {
        if (!ctx)
        {
            icError("Cannot load bundle: null context");
            return false;
        }

        // Load from embedded bundle
        if (LoadFromEmbedded(ctx))
        {
            icInfo("SBMD bundle loaded from embedded");
            return true;
        }

        icError("SBMD bundle not available (not compiled in)");
        return false;
    }

    bool SbmdBundleLoader::LoadFromEmbedded(JSContext *ctx)
    {
        icDebug("Attempting to load SBMD bundle from embedded source...");

        if (!ExecuteBundle(ctx, kSbmdBundle, kSbmdBundleSize, "sbmd-bundle"))
        {
            return false;
        }

        return true;
    }

    bool SbmdBundleLoader::ExecuteBundle(JSContext *ctx, const char *bundleSource, size_t length, const char *name)
    {
        if (!ctx)
        {
            icError("Context not initialized");
            return false;
        }

        if (!bundleSource || length == 0)
        {
            icError("Empty or null bundle source");
            return false;
        }

        icDebug("Executing SBMD %s bundle (%zu bytes)...", name, length);

        // Build the source tag from the name
        std::string sourceTag = std::string("<") + name + "-bundle>";

        // Execute the bundle script (mquickjs: use JS_EVAL_REPL for default eval flags)
        JSValue result = JS_Eval(ctx, bundleSource, length, sourceTag.c_str(), JS_EVAL_REPL);

        if (JS_IsException(result))
        {
            icError("Failed to execute SBMD %s bundle: %s", name, GetExceptionString(ctx).c_str());
            MQuickJsRuntime::RecordJsException("init", nullptr);
            {
                std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
                MQuickJsRuntime::LogMemoryUsage("sbmd-bundle-load-failed", IC_LOG_ERROR, true);
            }
            return false;
        }

        // Check if bundle execution left an exception (indicates a problem we should fix)
        std::string exMsg;
        if (MQuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg))
        {
            icError("SBMD %s bundle execution left a pending exception: %s", name, exMsg.c_str());
            return false;
        }

        // Verify that Sbmd global was created
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue sbmd = JS_GetPropertyStr(ctx, global, "Sbmd");

        if (JS_IsUndefined(sbmd))
        {
            icError("SBMD %s bundle did not create expected 'Sbmd' global", name);
            return false;
        }

        icDebug("SBMD %s bundle executed successfully - Sbmd global is available", name);
        return true;
    }

} // namespace barton
