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
// Created by tlea on 2/17/26
//

#define LOG_TAG "MatterJsClusterLoader"

#include "MatterJsClusterLoader.h"

#include <string>

extern "C" {
#include <icLog/logging.h>
#include <quickjs/quickjs.h>
}

// Try to include the embedded bundle header if it was generated
#if __has_include("MatterJsClustersEmbedded.h")
#include "MatterJsClustersEmbedded.h"
#define HAS_EMBEDDED_BUNDLE 1
#else
#define HAS_EMBEDDED_BUNDLE 0
#endif

namespace barton
{

    // Static member initialization
    const char *MatterJsClusterLoader::source_ = "none";

    namespace
    {
        /**
         * RAII wrapper for QuickJS JSValue.
         */
        class JsValueGuard
        {
        public:
            JsValueGuard(JSContext *ctx, JSValue value) : ctx_(ctx), value_(value) {}
            ~JsValueGuard()
            {
                if (ctx_)
                {
                    JS_FreeValue(ctx_, value_);
                }
            }

            JsValueGuard(const JsValueGuard &) = delete;
            JsValueGuard &operator=(const JsValueGuard &) = delete;

            JSValue get() const { return value_; }

        private:
            JSContext *ctx_;
            JSValue value_;
        };

        /**
         * Extract QuickJS exception as a string.
         */
        std::string GetExceptionString(JSContext *ctx)
        {
            JsValueGuard exceptionGuard(ctx, JS_GetException(ctx));
            const char *str = JS_ToCString(ctx, exceptionGuard.get());
            if (str)
            {
                std::string result(str);
                JS_FreeCString(ctx, str);
                return result;
            }
            return "unknown error";
        }

} // anonymous namespace

bool MatterJsClusterLoader::LoadBundle(JSContext *ctx)
{
#ifndef BARTON_CONFIG_MATTER_USE_MATTERJS
    icLogInfo(LOG_TAG, "matter.js integration disabled at build time (BCORE_MATTER_USE_MATTERJS=OFF)");
    return false;
#else
    if (!ctx)
    {
        icLogError(LOG_TAG, "Cannot load bundle: null context");
        return false;
    }

    // Load from embedded bundle
    if (LoadFromEmbedded(ctx))
    {
        source_ = "embedded";
        icLogInfo(LOG_TAG, "matter.js cluster bundle loaded from embedded source");

        // Freeze MatterClusters to prevent script modifications
        if (FreezeGlobalObject(ctx, "MatterClusters"))
        {
            icLogDebug(LOG_TAG, "MatterClusters frozen for script isolation");
        }

        return true;
    }

    icLogWarn(LOG_TAG, "matter.js cluster bundle not available (not compiled in)");
    return false;
#endif
}

bool MatterJsClusterLoader::IsAvailable()
{
#if defined(BARTON_CONFIG_MATTER_USE_MATTERJS) && HAS_EMBEDDED_BUNDLE
    return true;
#else
    return false;
#endif
}

const char *MatterJsClusterLoader::GetSource()
{
    return source_;
}

bool MatterJsClusterLoader::LoadFromEmbedded(JSContext *ctx)
{
#if HAS_EMBEDDED_BUNDLE
    icLogDebug(LOG_TAG, "Attempting to load matter.js cluster bundle from embedded source...");
    return ExecuteBundle(ctx, kMatterJsClustersBundle, kMatterJsClustersBundleSize);
#else
    (void) ctx;
    icLogDebug(LOG_TAG, "Embedded matter.js cluster bundle not available");
    return false;
#endif
}

bool MatterJsClusterLoader::ExecuteBundle(JSContext *ctx, const char *bundleSource, size_t length)
{
    if (!ctx)
    {
        icLogError(LOG_TAG, "Context not initialized");
        return false;
    }

    if (!bundleSource || length == 0)
    {
        icLogError(LOG_TAG, "Empty or null bundle source");
        return false;
    }

    icLogDebug(LOG_TAG, "Executing matter.js cluster bundle (%zu bytes)...", length);

    // Execute the bundle script
    JSValue result = JS_Eval(ctx, bundleSource, length, "<matter-clusters-bundle>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result))
    {
        icLogError(LOG_TAG, "Failed to execute matter.js cluster bundle: %s", GetExceptionString(ctx).c_str());
        JS_FreeValue(ctx, result);
        return false;
    }

    JS_FreeValue(ctx, result);

    // Verify that MatterClusters global was created
    JsValueGuard globalGuard(ctx, JS_GetGlobalObject(ctx));
    JsValueGuard clustersGuard(ctx, JS_GetPropertyStr(ctx, globalGuard.get(), "MatterClusters"));

    if (JS_IsUndefined(clustersGuard.get()))
    {
        icLogError(LOG_TAG, "matter.js cluster bundle did not create expected 'MatterClusters' global");
        return false;
    }

    icLogDebug(LOG_TAG, "matter.js cluster bundle executed successfully - MatterClusters global is available");
    return true;
}

bool MatterJsClusterLoader::FreezeGlobalObject(JSContext *ctx, const char *name)
{
    if (!ctx)
    {
        return false;
    }

    // Deep freeze the object to prevent any modifications by scripts
    // This provides isolation - scripts cannot pollute or modify the shared library
    const char *freezeScript = R"(
        (function() {
            function deepFreeze(obj) {
                if (obj === null || typeof obj !== 'object') return obj;
                Object.freeze(obj);
                Object.getOwnPropertyNames(obj).forEach(function(prop) {
                    var val = obj[prop];
                    if (val !== null && typeof val === 'object' && !Object.isFrozen(val)) {
                        deepFreeze(val);
                    }
                });
                return obj;
            }
            if (typeof %s !== 'undefined') {
                deepFreeze(%s);
                return true;
            }
            return false;
        })()
    )";

    // Format the script with the object name (simple string replacement)
    std::string script = freezeScript;
    size_t pos;
    while ((pos = script.find("%s")) != std::string::npos)
    {
        script.replace(pos, 2, name);
    }

    JSValue result = JS_Eval(ctx, script.c_str(), script.length(), "<freeze>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result))
    {
        icLogWarn(LOG_TAG, "Failed to freeze %s: %s", name, GetExceptionString(ctx).c_str());
        JS_FreeValue(ctx, result);
        return false;
    }

    bool success = JS_ToBool(ctx, result);
    JS_FreeValue(ctx, result);

    return success;
}

} // namespace barton
