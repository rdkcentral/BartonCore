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

#define LOG_TAG "SbmdUtilsLoader"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdUtilsLoader.h"
#include "MQuickJsRuntime.h"

#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

// Try to include the embedded bundle header if it was generated
#if __has_include("SbmdUtilsEmbedded.h")
#include "SbmdUtilsEmbedded.h"
#define HAS_EMBEDDED_UTILS 1
#else
#define HAS_EMBEDDED_UTILS 0
#endif

namespace barton
{

    // Static member initialization
    const char *SbmdUtilsLoader::source = "none";

    namespace
    {
        /**
         * Extract mquickjs exception as a string.
         */
        std::string GetExceptionString(JSContext *ctx)
        {
            JSValue ex = JS_GetException(ctx);
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, ex, &buf);
            if (str)
            {
                return std::string(str);
            }
            return "unknown error";
        }

    } // anonymous namespace

    bool SbmdUtilsLoader::LoadBundle(JSContext *ctx)
    {
        if (!ctx)
        {
            icError("Cannot load bundle: null context");
            return false;
        }

        // Load from embedded bundle
        if (LoadFromEmbedded(ctx))
        {
            source = "embedded";
            icInfo("SBMD utilities loaded from embedded");
            return true;
        }

        icError("SBMD utilities bundle not available (not compiled in)");
        return false;
    }

    bool SbmdUtilsLoader::IsAvailable()
    {
#if HAS_EMBEDDED_UTILS
        return true;
#else
        return false;
#endif
    }

    const char *SbmdUtilsLoader::GetSource()
    {
        return source;
    }

    bool SbmdUtilsLoader::LoadFromEmbedded(JSContext *ctx)
    {
#if HAS_EMBEDDED_UTILS
        icDebug("Attempting to load SBMD utilities bundle from embedded source...");
        return ExecuteBundle(ctx, kSbmdUtilsBundle, kSbmdUtilsBundleSize);
#else
        (void) ctx;
        icDebug("Embedded SBMD utilities bundle not available");
        return false;
#endif
    }

    bool SbmdUtilsLoader::ExecuteBundle(JSContext *ctx, const char *bundleSource, size_t length)
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

        icDebug("Executing SBMD utilities bundle (%zu bytes)...", length);

        // Execute the bundle script (mquickjs: use JS_EVAL_REPL for default eval flags)
        JSValue result = JS_Eval(ctx, bundleSource, length, "<sbmd-utils-bundle>", JS_EVAL_REPL);

        if (JS_IsException(result))
        {
            icError("Failed to execute SBMD utilities bundle: %s", GetExceptionString(ctx).c_str());
            MQuickJsRuntime::LogMemoryUsage("sbmd-utils-load-failed", IC_LOG_ERROR, true);
            return false;
        }

        // Check if bundle execution left an exception (indicates a problem we should fix)
        std::string exMsg;
        if (MQuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg))
        {
            icError("SbmdUtils bundle execution left a pending exception: %s", exMsg.c_str());
            return false;
        }

        // Verify that SbmdUtils global was created
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue utils = JS_GetPropertyStr(ctx, global, "SbmdUtils");

        if (JS_IsUndefined(utils))
        {
            icError("SBMD utilities bundle did not create expected 'SbmdUtils' global");
            return false;
        }

        icDebug("SBMD utilities bundle executed successfully - SbmdUtils global is available");
        return true;
    }

} // namespace barton
