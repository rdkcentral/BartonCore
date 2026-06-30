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

#define LOG_TAG "SbmdLoader"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdLoader.h"
#include "MQuickJsRuntime.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    namespace
    {
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

        /**
         * Get a string property from a JS object, or empty string if missing.
         */
        std::string GetStringProp(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return "";
            }

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, val, &buf);

            return str ? std::string(str) : "";
        }

        /**
         * Get a uint32 property from a JS object, or 0 if missing.
         */
        uint32_t GetUint32Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return 0;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val);

            return result;
        }

        /**
         * Get an optional uint32 property from a JS object.
         */
        std::optional<uint32_t> GetOptUint32Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return std::nullopt;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val);

            return result;
        }

        /**
         * Get an optional uint16 property from a JS object.
         */
        std::optional<uint16_t> GetOptUint16Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            auto opt = GetOptUint32Prop(ctx, obj, name);

            if (!opt.has_value())
            {
                return std::nullopt;
            }

            return static_cast<uint16_t>(*opt);
        }

        /**
         * Get the length of a JS array.
         */
        uint32_t GetArrayLength(JSContext *ctx, JSValue arr)
        {
            JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");

            if (JS_IsUndefined(lenVal))
            {
                return 0;
            }

            uint32_t len = 0;
            JS_ToUint32(ctx, &len, lenVal);

            return len;
        }

        /**
         * Read a JS array of strings.
         */
        std::vector<std::string> GetStringArray(JSContext *ctx, JSValue arr)
        {
            std::vector<std::string> result;
            uint32_t len = GetArrayLength(ctx, arr);

            for (uint32_t i = 0; i < len; i++)
            {
                JSValue elem = JS_GetPropertyUint32(ctx, arr, i);
                JSCStringBuf buf;
                const char *str = JS_ToCString(ctx, elem, &buf);

                if (str)
                {
                    result.emplace_back(str);
                }
            }

            return result;
        }

        /**
         * Read a JS array of uint16_t values.
         */
        std::vector<uint16_t> GetUint16Array(JSContext *ctx, JSValue arr)
        {
            std::vector<uint16_t> result;
            uint32_t len = GetArrayLength(ctx, arr);

            for (uint32_t i = 0; i < len; i++)
            {
                JSValue elem = JS_GetPropertyUint32(ctx, arr, i);
                uint32_t val = 0;
                JS_ToUint32(ctx, &val, elem);
                result.push_back(static_cast<uint16_t>(val));
            }

            return result;
        }

        /**
         * Read a JS array of uint32_t values.
         */
        std::vector<uint32_t> GetUint32Array(JSContext *ctx, JSValue arr)
        {
            std::vector<uint32_t> result;
            uint32_t len = GetArrayLength(ctx, arr);

            for (uint32_t i = 0; i < len; i++)
            {
                JSValue elem = JS_GetPropertyUint32(ctx, arr, i);
                uint32_t val = 0;
                JS_ToUint32(ctx, &val, elem);
                result.push_back(val);
            }

            return result;
        }

        /**
         * Get the keys of a JS object by evaluating Object.keys().
         * We store the object on a temporary global, evaluate Object.keys(),
         * then clean up.
         */
        std::vector<std::string> GetObjectKeys(JSContext *ctx, JSValue obj)
        {
            // Store object as a temp global
            JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "__sbmd_tmp", obj);

            const char *script = "JSON.stringify(Object.keys(__sbmd_tmp))";
            JSValue result = JS_Eval(ctx, script, strlen(script), "<keys>", JS_EVAL_RETVAL);

            // Clean up temp global
            JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "__sbmd_tmp", JS_UNDEFINED);

            if (JS_IsException(result))
            {
                MQuickJsRuntime::CheckAndClearPendingException(ctx);
                return {};
            }

            JSCStringBuf buf;
            const char *jsonStr = JS_ToCString(ctx, result, &buf);

            if (!jsonStr)
            {
                return {};
            }

            // Parse the JSON array of strings manually (simple format: ["key1","key2",...])
            std::vector<std::string> keys;
            std::string json(jsonStr);

            if (json.size() < 2 || json[0] != '[')
            {
                return keys;
            }

            size_t pos = 1;

            while (pos < json.size())
            {
                // Skip whitespace and commas
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == ','))
                {
                    pos++;
                }

                if (pos >= json.size() || json[pos] == ']')
                {
                    break;
                }

                if (json[pos] != '"')
                {
                    break;
                }

                pos++; // skip opening quote
                std::string key;

                while (pos < json.size() && json[pos] != '"')
                {
                    if (json[pos] == '\\' && pos + 1 < json.size())
                    {
                        pos++;
                    }

                    key += json[pos];
                    pos++;
                }

                pos++; // skip closing quote
                keys.push_back(key);
            }

            return keys;
        }

        /**
         * Find the end of a brace-delimited block starting at the opening brace.
         * Handles nested braces, string literals, and comments.
         * Returns the position of the closing brace, or std::string::npos on failure.
         */
        size_t FindMatchingBrace(const char *source, size_t sourceLen, size_t openPos)
        {
            if (openPos >= sourceLen || source[openPos] != '{')
            {
                return std::string::npos;
            }

            int depth = 1;
            size_t pos = openPos + 1;

            while (pos < sourceLen && depth > 0)
            {
                char c = source[pos];

                if (c == '/' && pos + 1 < sourceLen)
                {
                    if (source[pos + 1] == '/')
                    {
                        // Line comment — skip to end of line
                        while (pos < sourceLen && source[pos] != '\n')
                        {
                            pos++;
                        }

                        continue;
                    }

                    if (source[pos + 1] == '*')
                    {
                        // Block comment — skip to */
                        pos += 2;

                        while (pos + 1 < sourceLen && !(source[pos] == '*' && source[pos + 1] == '/'))
                        {
                            pos++;
                        }

                        pos += 2;
                        continue;
                    }
                }

                if (c == '"' || c == '\'')
                {
                    // String literal — skip to matching unescaped quote
                    char quote = c;
                    pos++;

                    while (pos < sourceLen && source[pos] != quote)
                    {
                        if (source[pos] == '\\')
                        {
                            pos++;
                        }

                        pos++;
                    }

                    pos++; // skip closing quote
                    continue;
                }

                if (c == '`')
                {
                    // Template literal — skip to matching unescaped backtick
                    pos++;

                    while (pos < sourceLen && source[pos] != '`')
                    {
                        if (source[pos] == '\\')
                        {
                            pos++;
                        }

                        pos++;
                    }

                    pos++; // skip closing backtick
                    continue;
                }

                if (c == '{')
                {
                    depth++;
                }
                else if (c == '}')
                {
                    depth--;
                }

                pos++;
            }

            if (depth != 0)
            {
                return std::string::npos;
            }

            return pos - 1; // position of the closing brace
        }

    } // anonymous namespace

    bool SbmdLoader::InjectCaptureFunction(JSContext *ctx)
    {
        if (!ctx)
        {
            icError("Cannot inject capture function: null context");
            return false;
        }

        const char *captureScript = R"(
            var __sbmd_registration = null;
            function SbmdDriver(reg) {
                if (__sbmd_registration !== null)
                    throw new Error("SbmdDriver() called more than once");
                __sbmd_registration = reg;
            }
        )";

        JSValue result = JS_Eval(ctx, captureScript, strlen(captureScript), "<sbmd-capture>", JS_EVAL_REPL);

        if (JS_IsException(result))
        {
            icError("Failed to inject SbmdDriver capture function: %s", GetExceptionString(ctx).c_str());
            return false;
        }

        std::string exMsg;

        if (MQuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg))
        {
            icError("SbmdDriver injection left a pending exception: %s", exMsg.c_str());
            return false;
        }

        icDebug("SbmdDriver capture function injected");
        return true;
    }

    std::vector<std::pair<std::string, std::string>> SbmdLoader::ExtractConstants(JSContext *ctx,
                                                                                     const char *source,
                                                                                     size_t sourceLen)
    {
        std::vector<std::pair<std::string, std::string>> constants;

        // Scan for "constants" followed by optional whitespace and ":"
        const char *needle = "constants";
        const char *found = nullptr;
        const char *searchStart = source;
        size_t remaining = sourceLen;

        while (remaining > 0)
        {
            const char *match = static_cast<const char *>(memmem(searchStart, remaining, needle, strlen(needle)));

            if (!match)
            {
                break;
            }

            // Check that this is a standalone word (not part of another identifier)
            if (match > source)
            {
                char before = *(match - 1);

                if (isalnum(before) || before == '_')
                {
                    // Part of a longer identifier — skip
                    searchStart = match + 1;
                    remaining = sourceLen - (searchStart - source);
                    continue;
                }
            }

            // Find the colon after "constants" (skip whitespace)
            const char *afterKeyword = match + strlen(needle);
            const char *end = source + sourceLen;

            while (afterKeyword < end && (*afterKeyword == ' ' || *afterKeyword == '\t' || *afterKeyword == '\n' ||
                                          *afterKeyword == '\r'))
            {
                afterKeyword++;
            }

            if (afterKeyword < end && *afterKeyword == ':')
            {
                found = afterKeyword + 1;
                break;
            }

            // Not followed by colon — skip
            searchStart = match + 1;
            remaining = sourceLen - (searchStart - source);
        }

        if (!found)
        {
            icDebug("No constants block found in source");
            return constants;
        }

        // Skip whitespace after the colon
        const char *end = source + sourceLen;

        while (found < end && (*found == ' ' || *found == '\t' || *found == '\n' || *found == '\r'))
        {
            found++;
        }

        if (found >= end || *found != '{')
        {
            icWarn("constants: not followed by '{'");
            return constants;
        }

        // Find matching closing brace
        size_t openPos = found - source;
        size_t closePos = FindMatchingBrace(source, sourceLen, openPos);

        if (closePos == std::string::npos)
        {
            icError("Failed to find matching '}' for constants block");
            return constants;
        }

        // Extract the block content including braces
        std::string block(source + openPos, closePos - openPos + 1);

        // Evaluate as an object literal: ({...})
        std::string evalExpr = "(" + block + ")";
        JSValue objVal = JS_Eval(ctx, evalExpr.c_str(), evalExpr.size(), "<constants>", JS_EVAL_RETVAL);

        if (JS_IsException(objVal))
        {
            icError("Failed to evaluate constants block: %s", GetExceptionString(ctx).c_str());
            return constants;
        }

        // Get the keys and values
        auto keys = GetObjectKeys(ctx, objVal);

        for (const auto &key : keys)
        {
            JSValue val = JS_GetPropertyStr(ctx, objVal, key.c_str());
            JSCStringBuf buf;

            if (JS_IsString(ctx, val))
            {
                const char *str = JS_ToCString(ctx, val, &buf);

                if (str)
                {
                    // Emit as a quoted string literal
                    std::string escaped;
                    escaped += '"';

                    for (const char *p = str; *p; p++)
                    {
                        if (*p == '"')
                        {
                            escaped += "\\\"";
                        }
                        else if (*p == '\\')
                        {
                            escaped += "\\\\";
                        }
                        else
                        {
                            escaped += *p;
                        }
                    }

                    escaped += '"';
                    constants.emplace_back(key, escaped);
                }
            }
            else if (JS_IsNumber(ctx, val))
            {
                const char *str = JS_ToCString(ctx, val, &buf);

                if (str)
                {
                    constants.emplace_back(key, std::string(str));
                }
            }
            else if (JS_IsBool(val))
            {
                int boolVal = 0;
                JS_ToInt32(ctx, &boolVal, val);
                constants.emplace_back(key, boolVal ? "true" : "false");
            }
            else
            {
                icError("Constants block contains non-primitive value for key '%s'", key.c_str());
                return {}; // Reject the entire block
            }
        }

        icDebug("Extracted %zu constants from source", constants.size());
        return constants;
    }

    std::string SbmdLoader::GenerateConstantsPreamble(
        const std::vector<std::pair<std::string, std::string>> &constants)
    {
        std::string preamble;

        for (const auto &[name, value] : constants)
        {
            preamble += "var " + name + " = " + value + ";\n";
        }

        return preamble;
    }

    int SbmdLoader::CountPreambleLines(const std::string &preamble)
    {
        return static_cast<int>(std::count(preamble.begin(), preamble.end(), '\n'));
    }

    std::unique_ptr<SbmdRegistration> SbmdLoader::LoadDriver(JSContext *ctx,
                                                                   const std::string &filePath,
                                                                   const char *source,
                                                                   size_t sourceLen)
    {
        if (!ctx || !source || sourceLen == 0)
        {
            icError("Invalid arguments to LoadDriver");
            return nullptr;
        }

        icDebug("Loading driver from %s (%zu bytes)", filePath.c_str(), sourceLen);

        // Pass 1: Extract constants
        auto constants = ExtractConstants(ctx, source, sourceLen);
        std::string preamble = GenerateConstantsPreamble(constants);
        int preambleLines = CountPreambleLines(preamble);

        // Pass 2: Build IIFE-wrapped source with constants preamble
        std::string wrappedSource = "(function() {\n" + preamble + std::string(source, sourceLen) + "\n})()";

        icDebug("Evaluating driver (%zu bytes, %d constant vars, %d preamble lines)",
                wrappedSource.size(),
                (int) constants.size(),
                preambleLines);

        JSValue result = JS_Eval(ctx, wrappedSource.c_str(), wrappedSource.size(), filePath.c_str(), JS_EVAL_REPL);

        if (JS_IsException(result))
        {
            std::string msg = GetExceptionString(ctx);
            icError("Failed to evaluate driver %s: %s", filePath.c_str(), msg.c_str());
            MQuickJsRuntime::LogMemoryUsage("driver-eval-failed", IC_LOG_ERROR, true);
            // Reset registration in case SbmdDriver() was called before the error
            JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "__sbmd_registration", JS_NULL);
            return nullptr;
        }

        std::string exMsg;

        if (MQuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg))
        {
            icError("Driver evaluation left a pending exception: %s", exMsg.c_str());
            JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "__sbmd_registration", JS_NULL);
            return nullptr;
        }

        // Extract registration
        auto reg = ExtractRegistration(ctx, filePath);

        if (!reg)
        {
            icError("Failed to extract registration from %s", filePath.c_str());
            return nullptr;
        }

        icInfo("Loaded driver '%s' from %s (schema %s, driver %" PRIu32 ")",
               reg->name.c_str(),
               filePath.c_str(),
               reg->schemaVersion.c_str(),
               reg->driverVersion);

        return reg;
    }

    std::unique_ptr<SbmdRegistration> SbmdLoader::ExtractRegistration(JSContext *ctx,
                                                                           const std::string &filePath)
    {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue regVal = JS_GetPropertyStr(ctx, global, "__sbmd_registration");

        if (JS_IsUndefined(regVal) || JS_IsNull(regVal))
        {
            icError("No SbmdDriver() call found in %s (no __sbmd_registration)", filePath.c_str());
            return nullptr;
        }

        auto reg = std::make_unique<SbmdRegistration>();
        reg->filePath = filePath;

        if (!ExtractMetadata(ctx, regVal, *reg))
        {
            icError("Failed to extract metadata from %s", filePath.c_str());
            return nullptr;
        }

        // Extract aliases
        JSValue aliasesVal = JS_GetPropertyStr(ctx, regVal, "aliases");

        if (!JS_IsUndefined(aliasesVal) && !JS_IsNull(aliasesVal))
        {
            if (!ExtractAliases(ctx, aliasesVal, *reg))
            {
                icError("Failed to extract aliases from %s", filePath.c_str());
                return nullptr;
            }
        }

        // Extract endpoints
        JSValue endpointsVal = JS_GetPropertyStr(ctx, regVal, "endpoints");

        if (!JS_IsUndefined(endpointsVal) && !JS_IsNull(endpointsVal))
        {
            if (!ExtractEndpoints(ctx, endpointsVal, *reg))
            {
                icError("Failed to extract endpoints from %s", filePath.c_str());
                return nullptr;
            }
        }

        // Extract device-initiated message handlers
        JSValue attrHandlers = JS_GetPropertyStr(ctx, regVal, "attributeHandlers");

        if (!JS_IsUndefined(attrHandlers) && !JS_IsNull(attrHandlers))
        {
            if (!ExtractDeviceHandlers(ctx, attrHandlers, reg->attributeHandlers))
            {
                icError("Failed to extract attributeHandlers from %s", filePath.c_str());
                return nullptr;
            }
        }

        JSValue evtHandlers = JS_GetPropertyStr(ctx, regVal, "eventHandlers");

        if (!JS_IsUndefined(evtHandlers) && !JS_IsNull(evtHandlers))
        {
            if (!ExtractDeviceHandlers(ctx, evtHandlers, reg->eventHandlers))
            {
                icError("Failed to extract eventHandlers from %s", filePath.c_str());
                return nullptr;
            }
        }

        JSValue cmdHandlers = JS_GetPropertyStr(ctx, regVal, "commandHandlers");

        if (!JS_IsUndefined(cmdHandlers) && !JS_IsNull(cmdHandlers))
        {
            if (!ExtractDeviceHandlers(ctx, cmdHandlers, reg->commandHandlers))
            {
                icError("Failed to extract commandHandlers from %s", filePath.c_str());
                return nullptr;
            }
        }

        // Reset __sbmd_registration to null for the next driver
        JS_SetPropertyStr(ctx, global, "__sbmd_registration", JS_NULL);

        icDebug("Extracted registration: name=%s, %zu aliases, %zu endpoints, %zu attrHandlers, %zu evtHandlers, "
                "%zu cmdHandlers",
                reg->name.c_str(),
                reg->aliases.size(),
                reg->endpoints.size(),
                reg->attributeHandlers.size(),
                reg->eventHandlers.size(),
                reg->commandHandlers.size());

        return reg;
    }

    bool SbmdLoader::ExtractMetadata(JSContext *ctx, JSValue reg, SbmdRegistration &out)
    {
        out.schemaVersion = GetStringProp(ctx, reg, "schemaVersion");
        out.driverVersion = GetUint32Prop(ctx, reg, "driverVersion");
        out.name = GetStringProp(ctx, reg, "name");

        if (out.name.empty())
        {
            icError("Registration missing required 'name' field");
            return false;
        }

        if (out.schemaVersion.empty())
        {
            icError("Registration missing required 'schemaVersion' field");
            return false;
        }

        // Barton metadata
        JSValue bartonVal = JS_GetPropertyStr(ctx, reg, "barton");

        if (!JS_IsUndefined(bartonVal) && !JS_IsNull(bartonVal))
        {
            out.barton.deviceClass = GetStringProp(ctx, bartonVal, "deviceClass");
            out.barton.deviceClassVersion = GetUint32Prop(ctx, bartonVal, "deviceClassVersion");
        }

        // Matter metadata
        JSValue matterVal = JS_GetPropertyStr(ctx, reg, "matter");

        if (!JS_IsUndefined(matterVal) && !JS_IsNull(matterVal))
        {
            JSValue deviceTypes = JS_GetPropertyStr(ctx, matterVal, "deviceTypes");

            if (!JS_IsUndefined(deviceTypes))
            {
                out.matter.deviceTypes = GetUint16Array(ctx, deviceTypes);
            }

            out.matter.revision = GetOptUint32Prop(ctx, matterVal, "revision");
            out.matter.vendorId = GetOptUint16Prop(ctx, matterVal, "vendorId");
            out.matter.productId = GetOptUint16Prop(ctx, matterVal, "productId");
            out.matter.defaultTimeoutMs = GetOptUint32Prop(ctx, matterVal, "defaultTimeoutMs");

            JSValue featureClusters = JS_GetPropertyStr(ctx, matterVal, "featureClusters");

            if (!JS_IsUndefined(featureClusters))
            {
                out.matter.featureClusters = GetUint32Array(ctx, featureClusters);
            }
        }

        // Reporting
        JSValue reportingVal = JS_GetPropertyStr(ctx, reg, "reporting");

        if (!JS_IsUndefined(reportingVal) && !JS_IsNull(reportingVal))
        {
            out.reporting.minSecs = static_cast<uint16_t>(GetUint32Prop(ctx, reportingVal, "minSecs"));
            out.reporting.maxSecs = static_cast<uint16_t>(GetUint32Prop(ctx, reportingVal, "maxSecs"));
        }

        return true;
    }

    bool SbmdLoader::ExtractAliases(JSContext *ctx, JSValue aliasesObj, SbmdRegistration &out)
    {
        auto keys = GetObjectKeys(ctx, aliasesObj);

        for (const auto &name : keys)
        {
            JSValue aliasVal = JS_GetPropertyStr(ctx, aliasesObj, name.c_str());

            if (JS_IsUndefined(aliasVal) || JS_IsNull(aliasVal))
            {
                continue;
            }

            SbmdAlias alias;
            alias.name = name;
            alias.clusterId = GetUint32Prop(ctx, aliasVal, "clusterId");
            alias.attributeId = GetOptUint32Prop(ctx, aliasVal, "attributeId");
            alias.eventId = GetOptUint32Prop(ctx, aliasVal, "eventId");
            alias.commandId = GetOptUint32Prop(ctx, aliasVal, "commandId");
            alias.type = GetStringProp(ctx, aliasVal, "type");

            out.aliases[name] = std::move(alias);
        }

        return true;
    }

    bool SbmdLoader::ExtractEndpoints(JSContext *ctx, JSValue endpointsObj, SbmdRegistration &out)
    {
        auto endpointIds = GetObjectKeys(ctx, endpointsObj);

        for (const auto &epId : endpointIds)
        {
            JSValue epVal = JS_GetPropertyStr(ctx, endpointsObj, epId.c_str());

            if (JS_IsUndefined(epVal) || JS_IsNull(epVal))
            {
                continue;
            }

            SbmdEndpoint endpoint;
            endpoint.id = epId;
            endpoint.profile = GetStringProp(ctx, epVal, "profile");
            endpoint.profileVersion = GetUint32Prop(ctx, epVal, "profileVersion");

            // Extract resources
            JSValue resourcesVal = JS_GetPropertyStr(ctx, epVal, "resources");

            if (!JS_IsUndefined(resourcesVal) && !JS_IsNull(resourcesVal))
            {
                auto resourceIds = GetObjectKeys(ctx, resourcesVal);

                for (const auto &resId : resourceIds)
                {
                    JSValue resVal = JS_GetPropertyStr(ctx, resourcesVal, resId.c_str());

                    if (JS_IsUndefined(resVal) || JS_IsNull(resVal))
                    {
                        continue;
                    }

                    SbmdResource resource;
                    resource.id = resId;
                    resource.type = GetStringProp(ctx, resVal, "type");

                    // Modes array
                    JSValue modesVal = JS_GetPropertyStr(ctx, resVal, "modes");

                    if (!JS_IsUndefined(modesVal))
                    {
                        resource.modes = GetStringArray(ctx, modesVal);
                    }

                    // Optional flag
                    JSValue optVal = JS_GetPropertyStr(ctx, resVal, "optional");

                    if (JS_IsBool(optVal))
                    {
                        int boolVal = 0;
                        JS_ToInt32(ctx, &boolVal, optVal);
                        resource.optional = (boolVal != 0);
                    }

                    // Prerequisites
                    JSValue prereqVal = JS_GetPropertyStr(ctx, resVal, "prerequisites");

                    if (!JS_IsUndefined(prereqVal) && !JS_IsNull(prereqVal))
                    {
                        resource.prerequisites = GetStringArray(ctx, prereqVal);
                    }

                    // Resource handlers
                    JSValue seedVal = JS_GetPropertyStr(ctx, resVal, "seed");

                    if (!JS_IsUndefined(seedVal) && !JS_IsNull(seedVal))
                    {
                        resource.seed = ExtractResourceHandler(ctx, seedVal);
                    }

                    JSValue readVal = JS_GetPropertyStr(ctx, resVal, "read");

                    if (!JS_IsUndefined(readVal) && !JS_IsNull(readVal))
                    {
                        resource.read = ExtractResourceHandler(ctx, readVal);
                    }

                    JSValue writeVal = JS_GetPropertyStr(ctx, resVal, "write");

                    if (!JS_IsUndefined(writeVal) && !JS_IsNull(writeVal))
                    {
                        resource.write = ExtractResourceHandler(ctx, writeVal);
                    }

                    JSValue execVal = JS_GetPropertyStr(ctx, resVal, "execute");

                    if (!JS_IsUndefined(execVal) && !JS_IsNull(execVal))
                    {
                        resource.execute = ExtractResourceHandler(ctx, execVal);
                    }

                    endpoint.resources.push_back(std::move(resource));
                }
            }

            out.endpoints.push_back(std::move(endpoint));
        }

        return true;
    }

    std::optional<SbmdHandler> SbmdLoader::ExtractResourceHandler(JSContext *ctx, JSValue val)
    {
        SbmdHandler handler;

        if (JS_IsFunction(ctx, val))
        {
            // Simple form: just a function reference
            handler.handler = val;
            return handler;
        }

        // Object form: { supplements: {...}, handler: fn }
        JSValue handlerVal = JS_GetPropertyStr(ctx, val, "handler");

        if (!JS_IsFunction(ctx, handlerVal))
        {
            icError("Resource handler object missing 'handler' function");
            return std::nullopt;
        }

        handler.handler = handlerVal;

        JSValue supplementsVal = JS_GetPropertyStr(ctx, val, "supplements");

        if (!JS_IsUndefined(supplementsVal) && !JS_IsNull(supplementsVal))
        {
            handler.supplements = ExtractSupplements(ctx, supplementsVal);
        }

        return handler;
    }

    bool SbmdLoader::ExtractDeviceHandlers(JSContext *ctx,
                                              JSValue handlersObj,
                                              std::vector<SbmdDeviceHandler> &out)
    {
        auto handlerNames = GetObjectKeys(ctx, handlersObj);

        for (const auto &name : handlerNames)
        {
            JSValue handlerObj = JS_GetPropertyStr(ctx, handlersObj, name.c_str());

            if (JS_IsUndefined(handlerObj) || JS_IsNull(handlerObj))
            {
                continue;
            }

            SbmdDeviceHandler dh;
            dh.name = name;

            // Handler function
            JSValue handlerVal = JS_GetPropertyStr(ctx, handlerObj, "handler");

            if (!JS_IsFunction(ctx, handlerVal))
            {
                icError("Device handler '%s' missing 'handler' function", name.c_str());
                return false;
            }

            dh.handler = handlerVal;

            // Aliases
            JSValue aliasesVal = JS_GetPropertyStr(ctx, handlerObj, "aliases");

            if (!JS_IsUndefined(aliasesVal) && !JS_IsNull(aliasesVal))
            {
                dh.aliases = GetStringArray(ctx, aliasesVal);
            }

            // Supplements
            JSValue supplementsVal = JS_GetPropertyStr(ctx, handlerObj, "supplements");

            if (!JS_IsUndefined(supplementsVal) && !JS_IsNull(supplementsVal))
            {
                dh.supplements = ExtractSupplements(ctx, supplementsVal);
            }

            out.push_back(std::move(dh));
        }

        return true;
    }

    SbmdSupplements SbmdLoader::ExtractSupplements(JSContext *ctx, JSValue supplementsObj)
    {
        SbmdSupplements supplements;

        JSValue attrsVal = JS_GetPropertyStr(ctx, supplementsObj, "attributes");

        if (!JS_IsUndefined(attrsVal) && !JS_IsNull(attrsVal))
        {
            supplements.attributes = GetStringArray(ctx, attrsVal);
        }

        JSValue resVal = JS_GetPropertyStr(ctx, supplementsObj, "resources");

        if (!JS_IsUndefined(resVal) && !JS_IsNull(resVal))
        {
            supplements.resources = GetStringArray(ctx, resVal);
        }

        JSValue persistVal = JS_GetPropertyStr(ctx, supplementsObj, "persistentData");

        if (!JS_IsUndefined(persistVal) && !JS_IsNull(persistVal))
        {
            supplements.persistentData = GetStringArray(ctx, persistVal);
        }

        JSValue transientVal = JS_GetPropertyStr(ctx, supplementsObj, "transientData");

        if (!JS_IsUndefined(transientVal) && !JS_IsNull(transientVal))
        {
            supplements.transientData = GetStringArray(ctx, transientVal);
        }

        return supplements;
    }

} // namespace barton
