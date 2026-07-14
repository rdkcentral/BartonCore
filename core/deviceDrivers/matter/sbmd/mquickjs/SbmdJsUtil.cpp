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

        std::vector<uint16_t> GetUint16Array(JSContext *ctx, JSValue arr)
        {
            SafeJSValue rootedArray(ctx, arr);
            std::vector<uint16_t> result;
            uint32_t len = GetArrayLength(ctx, rootedArray.Get());

            for (uint32_t i = 0; i < len; i++)
            {
                JSValue elem = JS_GetPropertyUint32(ctx, rootedArray.Get(), i);
                uint32_t val = 0;
                JS_ToUint32(ctx, &val, elem);
                result.push_back(static_cast<uint16_t>(val));
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
                JSValue elem = JS_GetPropertyUint32(ctx, rootedArray.Get(), i);
                uint32_t val = 0;
                JS_ToUint32(ctx, &val, elem);
                result.push_back(val);
            }

            return result;
        }

        std::vector<std::string> GetObjectKeys(JSContext *ctx, JSValue obj)
        {
            // Store object as a temp global
            JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "__sbmd_tmp", obj);

            const char *script = "Object.keys(__sbmd_tmp)";
            SafeJSValue keysArr(ctx, JS_Eval(ctx, script, strlen(script), "<keys>", JS_EVAL_RETVAL));

            // Clean up temp global
            JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "__sbmd_tmp", JS_UNDEFINED);

            if (JS_IsException(keysArr.Get()))
            {
                MQuickJsRuntime::CheckAndClearPendingException(ctx);
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
