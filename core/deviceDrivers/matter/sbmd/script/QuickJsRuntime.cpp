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

#define LOG_TAG "QuickJsRuntime"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "QuickJsRuntime.h"

#include <chrono>
#include <cstring>
#include <string>

extern "C" {
#include <icLog/logging.h>
#include <quickjs/quickjs.h>
}

namespace barton
{

// Static member initialization
JSRuntime *QuickJsRuntime::runtime_ = nullptr;
JSContext *QuickJsRuntime::ctx_ = nullptr;
std::mutex QuickJsRuntime::mutex_;
bool QuickJsRuntime::initialized_ = false;

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

    /**
     * Native performance.now() implementation using high-resolution clock.
     */
    JSValue PerformanceNow(JSContext *ctx, JSValueConst /*this_val*/, int /*argc*/, JSValueConst * /*argv*/)
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
        return JS_NewFloat64(ctx, millis);
    }

    /**
     * Native console.log implementation that forwards to icDebug.
     */
    JSValue ConsoleLog(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
    {
        std::string message;
        for (int i = 0; i < argc; i++)
        {
            const char *str = JS_ToCString(ctx, argv[i]);
            if (str)
            {
                if (i > 0)
                    message += " ";
                message += str;
                JS_FreeCString(ctx, str);
            }
        }
        icDebug("[JS] %s", message.c_str());
        return JS_UNDEFINED;
    }

    /**
     * Native console.warn implementation that forwards to icWarn.
     */
    JSValue ConsoleWarn(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
    {
        std::string message;
        for (int i = 0; i < argc; i++)
        {
            const char *str = JS_ToCString(ctx, argv[i]);
            if (str)
            {
                if (i > 0)
                    message += " ";
                message += str;
                JS_FreeCString(ctx, str);
            }
        }
        icWarn("[JS] %s", message.c_str());
        return JS_UNDEFINED;
    }

    /**
     * Native console.error implementation that forwards to icError.
     */
    JSValue ConsoleError(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
    {
        std::string message;
        for (int i = 0; i < argc; i++)
        {
            const char *str = JS_ToCString(ctx, argv[i]);
            if (str)
            {
                if (i > 0)
                    message += " ";
                message += str;
                JS_FreeCString(ctx, str);
            }
        }
        icError("[JS] %s", message.c_str());
        return JS_UNDEFINED;
    }

} // anonymous namespace

bool QuickJsRuntime::InstallBrowserShims()
{
    if (!ctx_)
    {
        return false;
    }

    // Create console object
    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue console = JS_NewObject(ctx_);

    // Add console methods
    JS_SetPropertyStr(ctx_, console, "log", JS_NewCFunction(ctx_, ConsoleLog, "log", 1));
    JS_SetPropertyStr(ctx_, console, "debug", JS_NewCFunction(ctx_, ConsoleLog, "debug", 1));
    JS_SetPropertyStr(ctx_, console, "info", JS_NewCFunction(ctx_, ConsoleLog, "info", 1));
    JS_SetPropertyStr(ctx_, console, "warn", JS_NewCFunction(ctx_, ConsoleWarn, "warn", 1));
    JS_SetPropertyStr(ctx_, console, "error", JS_NewCFunction(ctx_, ConsoleError, "error", 1));

    // Set console on global object
    JS_SetPropertyStr(ctx_, global, "console", console);

    // Create performance object with now() method
    JSValue performance = JS_NewObject(ctx_);
    JS_SetPropertyStr(ctx_, performance, "now", JS_NewCFunction(ctx_, PerformanceNow, "now", 0));
    JS_SetPropertyStr(ctx_, global, "performance", performance);

    // Install minimal URL class polyfill (required by some JS libraries)
    const char *urlPolyfill = R"(
        globalThis.URL = class URL {
            constructor(url, base) {
                this.href = url;
                this.protocol = '';
                this.host = '';
                this.hostname = '';
                this.port = '';
                this.pathname = url;
                this.search = '';
                this.hash = '';
                this.origin = '';
            }
            toString() { return this.href; }
        };
    )";
    JSValue urlResult = JS_Eval(ctx_, urlPolyfill, strlen(urlPolyfill), "<url-polyfill>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(urlResult))
    {
        icWarn("Failed to install URL polyfill: %s", GetExceptionString(ctx_).c_str());
    }
    JS_FreeValue(ctx_, urlResult);

    // Check if polyfill installation left an exception (indicates a problem we should fix)
    std::string exMsg;
    if (CheckAndClearPendingException(ctx_, &exMsg))
    {
        icError("Polyfill installation left a pending exception: %s - this is a bug", exMsg.c_str());
        // Don't fail - polyfills are optional
    }

    JS_FreeValue(ctx_, global);

    icDebug("Browser shims installed (console, performance, URL)");
    return true;
}

bool QuickJsRuntime::Initialize()
{
    if (initialized_)
    {
        icDebug("Shared QuickJS runtime already initialized");
        return true;
    }

    icInfo("Initializing shared QuickJS runtime for SBMD scripts...");

    runtime_ = JS_NewRuntime();
    if (!runtime_)
    {
        icError("Failed to create shared QuickJS runtime");
        return false;
    }

    ctx_ = JS_NewContext(runtime_);
    if (!ctx_)
    {
        icError("Failed to create shared QuickJS context");
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
        return false;
    }

    // Install browser shims (console, performance, URL)
    if (!InstallBrowserShims())
    {
        icWarn("Failed to install browser shims - some JS libraries may not work");
    }

    initialized_ = true;
    icInfo("Shared QuickJS runtime initialized successfully");
    return true;
}

void QuickJsRuntime::Shutdown()
{
    if (!initialized_)
    {
        return;
    }

    icInfo("Shutting down shared QuickJS runtime...");

    if (ctx_)
    {
        JS_FreeContext(ctx_);
        ctx_ = nullptr;
    }

    if (runtime_)
    {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }

    initialized_ = false;
    icInfo("Shared QuickJS runtime shutdown complete");
}

JSContext *QuickJsRuntime::GetSharedContext()
{
    return ctx_;
}

std::mutex &QuickJsRuntime::GetMutex()
{
    return mutex_;
}

bool QuickJsRuntime::IsInitialized()
{
    return initialized_;
}

bool QuickJsRuntime::CheckAndClearPendingException(JSContext *ctx, std::string *outExceptionMsg)
{
    if (!ctx)
    {
        return false;
    }

    JSValue pendingEx = JS_GetException(ctx);
    int tag = JS_VALUE_GET_TAG(pendingEx);

    // JS_GetException returns JS_NULL or JS_TAG_UNINITIALIZED when no exception is pending
    bool hasException = (tag != JS_TAG_NULL && tag != JS_TAG_UNDEFINED && tag != JS_TAG_UNINITIALIZED);

    if (!hasException)
    {
        JS_FreeValue(ctx, pendingEx);
        return false;
    }

    // Extract exception message if caller wants it
    if (outExceptionMsg)
    {
        std::string exMsg;

        // Try ToCString for string exceptions
        if (JS_IsString(pendingEx))
        {
            const char *str = JS_ToCString(ctx, pendingEx);
            if (str)
            {
                exMsg = str;
                JS_FreeCString(ctx, str);
            }
        }
        else if (JS_IsObject(pendingEx))
        {
            // Try "message" property for Error objects
            JSValue msgVal = JS_GetPropertyStr(ctx, pendingEx, "message");
            if (JS_IsString(msgVal))
            {
                const char *msgStr = JS_ToCString(ctx, msgVal);
                if (msgStr)
                {
                    exMsg = msgStr;
                    JS_FreeCString(ctx, msgStr);
                }
            }
            JS_FreeValue(ctx, msgVal);

            // Also try to get stack trace for debugging
            JSValue stackVal = JS_GetPropertyStr(ctx, pendingEx, "stack");
            if (JS_IsString(stackVal))
            {
                const char *stackStr = JS_ToCString(ctx, stackVal);
                if (stackStr)
                {
                    if (!exMsg.empty())
                    {
                        exMsg += " | Stack: ";
                    }
                    exMsg += stackStr;
                    JS_FreeCString(ctx, stackStr);
                }
            }
            JS_FreeValue(ctx, stackVal);
        }
        else
        {
            // Try ToCString as fallback for other types
            const char *str = JS_ToCString(ctx, pendingEx);
            if (str)
            {
                exMsg = str;
                JS_FreeCString(ctx, str);
            }
        }

        if (exMsg.empty())
        {
            exMsg = "unknown exception (tag=" + std::to_string(tag) + ")";
        }

        *outExceptionMsg = std::move(exMsg);
    }

    JS_FreeValue(ctx, pendingEx);
    return true;
}

bool QuickJsRuntime::FreezeGlobalObject(const char *name)
{
    if (!ctx_)
    {
        return false;
    }

    // Deep freeze the object to prevent any modifications by scripts
    // This provides isolation - scripts cannot pollute or modify shared libraries
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

    JSValue result = JS_Eval(ctx_, script.c_str(), script.length(), "<freeze>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result))
    {
        icWarn("Failed to freeze %s: %s", name, GetExceptionString(ctx_).c_str());
        JS_FreeValue(ctx_, result);
        return false;
    }

    bool success = JS_ToBool(ctx_, result);
    JS_FreeValue(ctx_, result);

    // Check if freeze operation left an exception (indicates a problem we should fix)
    if (CheckAndClearPendingException(ctx_, nullptr))
    {
        icError("SbmdUtils freeze operation left a pending exception - this is a bug");
        return false;
    }

    return success;
}

} // namespace barton
