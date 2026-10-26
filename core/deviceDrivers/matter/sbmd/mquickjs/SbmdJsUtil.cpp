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

#include "SbmdJsUtil.h"
#include "MQuickJsRuntime.h"
#include "matter/sbmd/SafeJSValue.h"

#include <chrono>
#include <cmath>
#include <cstring>

namespace barton
{
    namespace mquickjs
    {
        std::string GetStringProp(JSContext *ctx, JSValue obj, const char *name)
        {
            // Root the property value: JS_ToCString may allocate and relocate objects (moving GC),
            // which would invalidate a raw JSValue held here.
            SafeJSValue val(ctx, JS_GetPropertyStr(ctx, obj, name));

            if (JS_IsUndefined(val.Get()) || JS_IsNull(val.Get()))
            {
                return "";
            }

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, val.Get(), &buf);

            return str ? std::string(str) : "";
        }

        uint32_t GetUint32Prop(JSContext *ctx, JSValue obj, const char *name, uint32_t defaultValue)
        {
            SafeJSValue val(ctx, JS_GetPropertyStr(ctx, obj, name));

            if (JS_IsUndefined(val.Get()) || JS_IsNull(val.Get()))
            {
                return defaultValue;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val.Get());

            return result;
        }

        std::optional<uint32_t> GetOptUint32Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            SafeJSValue val(ctx, JS_GetPropertyStr(ctx, obj, name));

            if (JS_IsUndefined(val.Get()) || JS_IsNull(val.Get()))
            {
                return std::nullopt;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val.Get());

            return result;
        }

        std::optional<uint16_t> GetOptUint16Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            auto opt = GetOptUint32Prop(ctx, obj, name);

            if (!opt.has_value())
            {
                return std::nullopt;
            }

            return static_cast<uint16_t>(*opt);
        }

        bool ToBoundedUint(JSContext *ctx, JSValue val, uint32_t maxValue, uint32_t &out)
        {
            // Only a JS number is accepted: strings, booleans, null and objects would otherwise be
            // coerced by ToNumber ("7" -> 7, true -> 1, null -> 0) into an aliasing value.
            if (!JS_IsNumber(ctx, val))
            {
                return false;
            }

            double number = 0;

            if (JS_ToNumber(ctx, &number, val) < 0)
            {
                return false;
            }

            // Reject NaN/Inf and any fractional part rather than truncating (2.5 must not become 2).
            if (!std::isfinite(number) || number != std::trunc(number) || number < 0 ||
                number > static_cast<double>(maxValue))
            {
                return false;
            }

            out = static_cast<uint32_t>(number);

            return true;
        }

        bool ReadOptBoundedUintProp(JSContext *ctx,
                                    JSValue obj,
                                    const char *name,
                                    uint32_t maxValue,
                                    std::optional<uint32_t> &out)
        {
            SafeJSValue val(ctx, JS_GetPropertyStr(ctx, obj, name));

            // Only an absent property means "not set"; an explicit null is a non-integer value.
            if (JS_IsUndefined(val.Get()))
            {
                out = std::nullopt;

                return true;
            }

            uint32_t value = 0;

            if (!ToBoundedUint(ctx, val.Get(), maxValue, value))
            {
                return false;
            }

            out = value;

            return true;
        }

        bool ReadOptBoundedUint16Prop(JSContext *ctx, JSValue obj, const char *name, std::optional<uint16_t> &out)
        {
            std::optional<uint32_t> value;

            if (!ReadOptBoundedUintProp(ctx, obj, name, UINT16_MAX, value))
            {
                return false;
            }

            out = value.has_value() ? std::optional<uint16_t>(static_cast<uint16_t>(*value)) : std::nullopt;

            return true;
        }

        uint32_t GetArrayLength(JSContext *ctx, JSValue arr)
        {
            SafeJSValue lenVal(ctx, JS_GetPropertyStr(ctx, arr, "length"));

            if (JS_IsUndefined(lenVal.Get()))
            {
                return 0;
            }

            uint32_t len = 0;
            JS_ToUint32(ctx, &len, lenVal.Get());

            return len;
        }

        bool ScriptTimedOut()
        {
            // The fired flag persists until the next SetDeadline(), so only trust it while a deadline
            // is active; a load without a deadline must not see a stale timeout from an earlier call.
            return MQuickJsRuntime::Instance().GetDeadline() != std::chrono::steady_clock::time_point {} &&
                   MQuickJsRuntime::Instance().WasTimedOut();
        }

        bool HasProperty(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            return !JS_IsUndefined(val);
        }

        std::vector<std::string> GetStringArray(JSContext *ctx, JSValue arr)
        {
            SafeJSValue rootedArray(ctx, arr);
            std::vector<std::string> result;
            uint32_t len = GetArrayLength(ctx, rootedArray.Get());

            for (uint32_t i = 0; i < len; i++)
            {
                if (ScriptTimedOut())
                {
                    break;
                }

                SafeJSValue elem(ctx, JS_GetPropertyUint32(ctx, rootedArray.Get(), i));
                JSCStringBuf buf;
                const char *str = JS_ToCString(ctx, elem.Get(), &buf);

                if (str)
                {
                    result.emplace_back(str);
                }
            }

            return result;
        }

        std::vector<uint32_t> GetUint32Array(JSContext *ctx, JSValue arr)
        {
            SafeJSValue rootedArray(ctx, arr);
            std::vector<uint32_t> result;
            uint32_t len = GetArrayLength(ctx, rootedArray.Get());

            for (uint32_t i = 0; i < len; i++)
            {
                if (ScriptTimedOut())
                {
                    break;
                }

                SafeJSValue elem(ctx, JS_GetPropertyUint32(ctx, rootedArray.Get(), i));
                uint32_t val = 0;
                JS_ToUint32(ctx, &val, elem.Get());
                result.push_back(val);
            }

            return result;
        }

        std::vector<std::string> GetObjectKeys(JSContext *ctx, JSValue obj)
        {
            // Root the global object and input object: JS_SetPropertyStr/JS_Eval can allocate and
            // trigger mquickjs's moving GC, which would relocate raw JSValue arguments mid-call.
            SafeJSValue rootedGlobal(ctx, JS_GetGlobalObject(ctx));
            SafeJSValue rootedObj(ctx, obj);

            // Store object as a temp global
            JS_SetPropertyStr(ctx, rootedGlobal.Get(), "__sbmd_tmp", rootedObj.Get());

            const char *script = "Object.keys(__sbmd_tmp)";
            SafeJSValue keysArr(ctx, JS_Eval(ctx, script, strlen(script), "<keys>", JS_EVAL_RETVAL));

            // Clean up temp global
            JS_SetPropertyStr(ctx, rootedGlobal.Get(), "__sbmd_tmp", JS_UNDEFINED);

            if (JS_IsException(keysArr.Get()))
            {
                MQuickJsRuntime::Instance().CheckAndClearPendingException(ctx);
                return {};
            }

            // Object.keys() already returns a JS array of strings
            return GetStringArray(ctx, keysArr.Get());
        }

        std::string GetExceptionString(JSContext *ctx)
        {
            // Root the exception: JS_ToCString may allocate and relocate objects (moving GC), which
            // would invalidate a raw JSValue held here.
            SafeJSValue ex(ctx, JS_GetException(ctx));

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, ex.Get(), &buf);

            if (str)
            {
                return std::string(str);
            }

            return "unknown error";
        }
    } // namespace mquickjs
} // namespace barton
