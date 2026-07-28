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
#include "SbmdJsUtil.h"
#include "matter/sbmd/SafeJSValue.h"

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
    using namespace mquickjs;

    namespace
    {
        /**
         * Reset the __sbmd_registration global to null so the next driver load
         * starts clean. A failed write would leak stale registration state into
         * the next load, so log (and clear) any exception it raises.
         */
        void ResetRegistration(JSContext *ctx)
        {
            JSValue res = JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "__sbmd_registration", JS_NULL);

            if (JS_IsException(res))
            {
                std::string exMsg;
                MQuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg);
                icError("Failed to reset __sbmd_registration for next driver load: %s", exMsg.c_str());
            }
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

    std::optional<std::vector<std::pair<std::string, std::string>>>
    SbmdLoader::ExtractConstants(JSContext *ctx, const char *source, size_t sourceLen)
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
            icError("constants: not followed by '{'");
            return std::nullopt;
        }

        // Find matching closing brace
        size_t openPos = found - source;
        size_t closePos = FindMatchingBrace(source, sourceLen, openPos);

        if (closePos == std::string::npos)
        {
            icError("Failed to find matching '}' for constants block");
            return std::nullopt;
        }

        // Extract the block content including braces
        std::string block(source + openPos, closePos - openPos + 1);

        // Evaluate as an object literal: ({...})
        std::string evalExpr = "(" + block + ")";
        SafeJSValue objVal(ctx, JS_Eval(ctx, evalExpr.c_str(), evalExpr.size(), "<constants>", JS_EVAL_RETVAL));

        if (JS_IsException(objVal.Get()))
        {
            icError("Failed to evaluate constants block: %s", GetExceptionString(ctx).c_str());
            return std::nullopt;
        }

        // Get the keys and values
        auto keys = GetObjectKeys(ctx, objVal.Get());

        // A constant key becomes a `var <key> = <value>;` declaration in the preamble, so it must be
        // a valid JavaScript identifier (^[A-Za-z_$][A-Za-z0-9_$]*$). Reject anything else with a
        // clear error rather than emitting a syntactically invalid declaration.
        auto isValidIdentifier = [](const std::string &name) {
            if (name.empty())
            {
                return false;
            }

            auto isIdentifierStart = [](char c) { return isalpha(c) || c == '_' || c == '$'; };
            auto isIdentifierPart = [&](char c) { return isIdentifierStart(c) || isdigit(c); };

            if (!isIdentifierStart(name.front()))
            {
                return false;
            }

            for (size_t i = 1; i < name.size(); i++)
            {
                if (!isIdentifierPart(name[i]))
                {
                    return false;
                }
            }

            return true;
        };

        for (const auto &key : keys)
        {
            if (!isValidIdentifier(key))
            {
                icError("Constants block key '%s' is not a valid JavaScript identifier", key.c_str());
                return std::nullopt; // Reject the entire block
            }

            JSValue valRaw = JS_GetPropertyStr(ctx, objVal.Get(), key.c_str());
            SafeJSValue val(ctx, valRaw);
            JSCStringBuf buf;

            if (JS_IsString(ctx, val.Get()))
            {
                const char *str = JS_ToCString(ctx, val.Get(), &buf);

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
                        else if (*p == '\n')
                        {
                            escaped += "\\n";
                        }
                        else if (*p == '\r')
                        {
                            escaped += "\\r";
                        }
                        else if (*p == '\t')
                        {
                            escaped += "\\t";
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
            else if (JS_IsNumber(ctx, val.Get()))
            {
                const char *str = JS_ToCString(ctx, val.Get(), &buf);

                if (str)
                {
                    constants.emplace_back(key, std::string(str));
                }
            }
            else if (JS_IsBool(val.Get()))
            {
                int boolVal = 0;
                JS_ToInt32(ctx, &boolVal, val.Get());
                constants.emplace_back(key, boolVal ? "true" : "false");
            }
            else
            {
                icError("Constants block contains non-primitive value for key '%s'", key.c_str());
                return std::nullopt; // Reject the entire block
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

    std::unique_ptr<SbmdRegistration>
    SbmdLoader::LoadDriver(JSContext *ctx, const std::string &filePath, const char *source, size_t sourceLen)
    {
        if (!ctx || !source || sourceLen == 0)
        {
            icError("Invalid arguments to LoadDriver");
            return nullptr;
        }

        icDebug("Loading driver from %s (%zu bytes)", filePath.c_str(), sourceLen);

        // Pass 1: Extract constants
        auto constants = ExtractConstants(ctx, source, sourceLen);

        if (!constants)
        {
            icError("Failed to extract constants block from %s", filePath.c_str());
            return nullptr;
        }

        std::string preamble = GenerateConstantsPreamble(*constants);
        int preambleLines = CountPreambleLines(preamble);

        // Pass 2: Build IIFE-wrapped source with constants preamble
        std::string wrappedSource = "(function() {\n" + preamble + std::string(source, sourceLen) + "\n})()";

        icDebug("Evaluating driver (%zu bytes, %d constant vars, %d preamble lines)",
                wrappedSource.size(),
                (int) constants->size(),
                preambleLines);

        SafeJSValue result(ctx,
                           JS_Eval(ctx, wrappedSource.c_str(), wrappedSource.size(), filePath.c_str(), JS_EVAL_REPL));

        if (JS_IsException(result.Get()))
        {
            std::string msg = GetExceptionString(ctx);
            icError("Failed to evaluate driver %s: %s", filePath.c_str(), msg.c_str());
            MQuickJsRuntime::LogMemoryUsage("driver-eval-failed", IC_LOG_ERROR, true);
            // Reset registration in case SbmdDriver() was called before the error
            ResetRegistration(ctx);
            return nullptr;
        }

        std::string exMsg;

        if (MQuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg))
        {
            icError("Driver evaluation left a pending exception: %s", exMsg.c_str());
            ResetRegistration(ctx);
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

    std::unique_ptr<SbmdRegistration> SbmdLoader::ExtractRegistration(JSContext *ctx, const std::string &filePath)
    {
        SafeJSValue global(ctx, JS_GetGlobalObject(ctx));
        SafeJSValue regVal(ctx, JS_GetPropertyStr(ctx, global.Get(), "__sbmd_registration"));

        if (JS_IsUndefined(regVal.Get()) || JS_IsNull(regVal.Get()))
        {
            icError("No SbmdDriver() call found in %s (no __sbmd_registration)", filePath.c_str());
            return nullptr;
        }

        // RAII guard: reset __sbmd_registration on every exit from this scope,
        // whether by success or by any early-return failure. Keeping the global
        // set throughout extraction also anchors regVal as a live GC root.
        struct ResetGuard
        {
            JSContext *ctx;
            ~ResetGuard() { ResetRegistration(ctx); }
        } resetGuard {ctx};

        auto reg = std::make_unique<SbmdRegistration>();
        reg->filePath = filePath;

        if (!ExtractMetadata(ctx, regVal.Get(), *reg))
        {
            icError("Failed to extract metadata from %s", filePath.c_str());
            return nullptr;
        }

        // Extract aliases
        SafeJSValue aliasesVal(ctx, JS_GetPropertyStr(ctx, regVal.Get(), "aliases"));

        if (!JS_IsUndefined(aliasesVal.Get()) && !JS_IsNull(aliasesVal.Get()))
        {
            if (!ExtractAliases(ctx, aliasesVal.Get(), *reg))
            {
                icError("Failed to extract aliases from %s", filePath.c_str());
                return nullptr;
            }
        }

        // Extract endpoints
        SafeJSValue endpointsVal(ctx, JS_GetPropertyStr(ctx, regVal.Get(), "endpoints"));

        if (!JS_IsUndefined(endpointsVal.Get()) && !JS_IsNull(endpointsVal.Get()))
        {
            if (!ExtractEndpoints(ctx, endpointsVal.Get(), *reg))
            {
                icError("Failed to extract endpoints from %s", filePath.c_str());
                return nullptr;
            }
        }

        // Extract device-initiated message handlers
        SafeJSValue attrHandlers(ctx, JS_GetPropertyStr(ctx, regVal.Get(), "attributeHandlers"));

        if (!JS_IsUndefined(attrHandlers.Get()) && !JS_IsNull(attrHandlers.Get()))
        {
            if (!ExtractDeviceHandlers(ctx, attrHandlers.Get(), reg->attributeHandlers))
            {
                icError("Failed to extract attributeHandlers from %s", filePath.c_str());
                return nullptr;
            }
        }

        SafeJSValue evtHandlers(ctx, JS_GetPropertyStr(ctx, regVal.Get(), "eventHandlers"));

        if (!JS_IsUndefined(evtHandlers.Get()) && !JS_IsNull(evtHandlers.Get()))
        {
            if (!ExtractDeviceHandlers(ctx, evtHandlers.Get(), reg->eventHandlers))
            {
                icError("Failed to extract eventHandlers from %s", filePath.c_str());
                return nullptr;
            }
        }

        SafeJSValue cmdHandlers(ctx, JS_GetPropertyStr(ctx, regVal.Get(), "commandHandlers"));

        if (!JS_IsUndefined(cmdHandlers.Get()) && !JS_IsNull(cmdHandlers.Get()))
        {
            if (!ExtractDeviceHandlers(ctx, cmdHandlers.Get(), reg->commandHandlers))
            {
                icError("Failed to extract commandHandlers from %s", filePath.c_str());
                return nullptr;
            }
        }

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
        SafeJSValue regObj(ctx, reg);

        out.schemaVersion = GetStringProp(ctx, regObj.Get(), "schemaVersion");
        out.driverVersion = GetUint32Prop(ctx, regObj.Get(), "driverVersion", 0);
        out.name = GetStringProp(ctx, regObj.Get(), "name");

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
        SafeJSValue bartonVal(ctx, JS_GetPropertyStr(ctx, regObj.Get(), "barton"));

        if (!JS_IsUndefined(bartonVal.Get()) && !JS_IsNull(bartonVal.Get()))
        {
            out.barton.deviceClass = GetStringProp(ctx, bartonVal.Get(), "deviceClass");
            out.barton.deviceClassVersion = GetUint32Prop(ctx, bartonVal.Get(), "deviceClassVersion", 0);
        }

        // Matter metadata
        SafeJSValue matterVal(ctx, JS_GetPropertyStr(ctx, regObj.Get(), "matter"));

        if (!JS_IsUndefined(matterVal.Get()) && !JS_IsNull(matterVal.Get()))
        {
            SafeJSValue deviceTypes(ctx, JS_GetPropertyStr(ctx, matterVal.Get(), "deviceTypes"));

            if (!JS_IsUndefined(deviceTypes.Get()))
            {
                out.matter.deviceTypes = GetUint16Array(ctx, deviceTypes.Get());
            }

            out.matter.revision = GetOptUint32Prop(ctx, matterVal.Get(), "revision");
            out.matter.vendorId = GetOptUint16Prop(ctx, matterVal.Get(), "vendorId");
            out.matter.productId = GetOptUint16Prop(ctx, matterVal.Get(), "productId");
            out.matter.defaultTimeoutMs = GetOptUint32Prop(ctx, matterVal.Get(), "defaultTimeoutMs");

            SafeJSValue featureClusters(ctx, JS_GetPropertyStr(ctx, matterVal.Get(), "featureClusters"));

            if (!JS_IsUndefined(featureClusters.Get()))
            {
                out.matter.featureClusters = GetUint32Array(ctx, featureClusters.Get());
            }
        }

        // Reporting
        SafeJSValue reportingVal(ctx, JS_GetPropertyStr(ctx, regObj.Get(), "reporting"));

        if (!JS_IsUndefined(reportingVal.Get()) && !JS_IsNull(reportingVal.Get()))
        {
            out.reporting.minSecs = static_cast<uint16_t>(GetUint32Prop(ctx, reportingVal.Get(), "minSecs", 0));
            out.reporting.maxSecs = static_cast<uint16_t>(GetUint32Prop(ctx, reportingVal.Get(), "maxSecs", 0));
        }

        return true;
    }

    bool SbmdLoader::ExtractAliases(JSContext *ctx, JSValue aliasesObj, SbmdRegistration &out)
    {
        // Root the incoming object and every intermediate handle: mquickjs uses a moving GC, so a
        // raw JSValue held across an allocating call (GetObjectKeys / JS_GetPropertyStr) becomes a
        // stale pointer once the collector relocates it.
        SafeJSValue rootedAliases(ctx, aliasesObj);
        auto keys = GetObjectKeys(ctx, rootedAliases.Get());

        for (const auto &name : keys)
        {
            SafeJSValue aliasVal(ctx, JS_GetPropertyStr(ctx, rootedAliases.Get(), name.c_str()));

            if (JS_IsUndefined(aliasVal.Get()) || JS_IsNull(aliasVal.Get()))
            {
                continue;
            }

            SbmdAlias alias;
            alias.name = name;
            alias.clusterId = GetUint32Prop(ctx, aliasVal.Get(), "clusterId", 0);
            alias.attributeId = GetOptUint32Prop(ctx, aliasVal.Get(), "attributeId");
            alias.eventId = GetOptUint32Prop(ctx, aliasVal.Get(), "eventId");
            alias.commandId = GetOptUint32Prop(ctx, aliasVal.Get(), "commandId");
            alias.type = GetStringProp(ctx, aliasVal.Get(), "type");

            out.aliases[name] = std::move(alias);
        }

        return true;
    }

    bool SbmdLoader::ExtractEndpoints(JSContext *ctx, JSValue endpointsObj, SbmdRegistration &out)
    {
        // Root the incoming object and every intermediate handle so they survive the GC
        // relocations triggered by the many allocating property reads below.
        SafeJSValue rootedEndpoints(ctx, endpointsObj);
        auto endpointIds = GetObjectKeys(ctx, rootedEndpoints.Get());

        for (const auto &epId : endpointIds)
        {
            SafeJSValue epVal(ctx, JS_GetPropertyStr(ctx, rootedEndpoints.Get(), epId.c_str()));

            if (JS_IsUndefined(epVal.Get()) || JS_IsNull(epVal.Get()))
            {
                continue;
            }

            SbmdEndpoint endpoint;
            endpoint.id = epId;
            endpoint.profile = GetStringProp(ctx, epVal.Get(), "profile");
            endpoint.profileVersion = GetUint32Prop(ctx, epVal.Get(), "profileVersion", 0);

            // Extract resources
            SafeJSValue resourcesVal(ctx, JS_GetPropertyStr(ctx, epVal.Get(), "resources"));

            if (!JS_IsUndefined(resourcesVal.Get()) && !JS_IsNull(resourcesVal.Get()))
            {
                auto resourceIds = GetObjectKeys(ctx, resourcesVal.Get());

                for (const auto &resId : resourceIds)
                {
                    SafeJSValue resVal(ctx, JS_GetPropertyStr(ctx, resourcesVal.Get(), resId.c_str()));

                    if (JS_IsUndefined(resVal.Get()) || JS_IsNull(resVal.Get()))
                    {
                        continue;
                    }

                    SbmdResource resource;
                    resource.id = resId;
                    resource.type = GetStringProp(ctx, resVal.Get(), "type");

                    // Modes array
                    SafeJSValue modesVal(ctx, JS_GetPropertyStr(ctx, resVal.Get(), "modes"));

                    if (!JS_IsUndefined(modesVal.Get()))
                    {
                        resource.modes = GetStringArray(ctx, modesVal.Get());
                    }

                    // Optional flag
                    SafeJSValue optVal(ctx, JS_GetPropertyStr(ctx, resVal.Get(), "optional"));

                    if (JS_IsBool(optVal.Get()))
                    {
                        int boolVal = 0;
                        JS_ToInt32(ctx, &boolVal, optVal.Get());
                        resource.optional = (boolVal != 0);
                    }

                    // Prerequisites
                    SafeJSValue prereqVal(ctx, JS_GetPropertyStr(ctx, resVal.Get(), "prerequisites"));

                    if (!JS_IsUndefined(prereqVal.Get()) && !JS_IsNull(prereqVal.Get()))
                    {
                        resource.prerequisites = GetStringArray(ctx, prereqVal.Get());
                    }

                    // Resource handlers
                    SafeJSValue seedVal(ctx, JS_GetPropertyStr(ctx, resVal.Get(), "seed"));

                    if (!JS_IsUndefined(seedVal.Get()) && !JS_IsNull(seedVal.Get()))
                    {
                        resource.seed = ExtractResourceHandler(ctx, seedVal.Get());
                    }

                    SafeJSValue readVal(ctx, JS_GetPropertyStr(ctx, resVal.Get(), "read"));

                    if (!JS_IsUndefined(readVal.Get()) && !JS_IsNull(readVal.Get()))
                    {
                        resource.read = ExtractResourceHandler(ctx, readVal.Get());
                    }

                    SafeJSValue writeVal(ctx, JS_GetPropertyStr(ctx, resVal.Get(), "write"));

                    if (!JS_IsUndefined(writeVal.Get()) && !JS_IsNull(writeVal.Get()))
                    {
                        resource.write = ExtractResourceHandler(ctx, writeVal.Get());
                    }

                    SafeJSValue execVal(ctx, JS_GetPropertyStr(ctx, resVal.Get(), "execute"));

                    if (!JS_IsUndefined(execVal.Get()) && !JS_IsNull(execVal.Get()))
                    {
                        resource.execute = ExtractResourceHandler(ctx, execVal.Get());
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
        SafeJSValue rootedVal(ctx, val);
        SbmdHandler handler;

        if (JS_IsFunction(ctx, rootedVal.Get()))
        {
            // Simple form: just a function reference. Root it immediately (heldFn) so it survives
            // the GC relocations triggered while the rest of the driver is extracted; the raw
            // handler slot is only valid transiently and callers must invoke via Fn().
            handler.handler = rootedVal.Get();
            handler.heldFn = SafeJSValue {ctx, rootedVal.Get()};
            return handler;
        }

        // Object form: { supplements: {...}, handler: fn }
        SafeJSValue handlerVal(ctx, JS_GetPropertyStr(ctx, rootedVal.Get(), "handler"));

        if (!JS_IsFunction(ctx, handlerVal.Get()))
        {
            icError("Resource handler object missing 'handler' function");
            return std::nullopt;
        }

        handler.handler = handlerVal.Get();
        handler.heldFn = SafeJSValue {ctx, handlerVal.Get()};

        SafeJSValue supplementsVal(ctx, JS_GetPropertyStr(ctx, rootedVal.Get(), "supplements"));

        if (!JS_IsUndefined(supplementsVal.Get()) && !JS_IsNull(supplementsVal.Get()))
        {
            handler.supplements = ExtractSupplements(ctx, supplementsVal.Get());
        }

        return handler;
    }

    bool SbmdLoader::ExtractDeviceHandlers(JSContext *ctx, JSValue handlersObj, std::vector<SbmdDeviceHandler> &out)
    {
        SafeJSValue rootedHandlers(ctx, handlersObj);
        auto handlerNames = GetObjectKeys(ctx, rootedHandlers.Get());

        for (const auto &name : handlerNames)
        {
            SafeJSValue handlerObj(ctx, JS_GetPropertyStr(ctx, rootedHandlers.Get(), name.c_str()));

            if (JS_IsUndefined(handlerObj.Get()) || JS_IsNull(handlerObj.Get()))
            {
                continue;
            }

            SbmdDeviceHandler dh;
            dh.name = name;

            // Handler function
            SafeJSValue handlerVal(ctx, JS_GetPropertyStr(ctx, handlerObj.Get(), "handler"));

            if (!JS_IsFunction(ctx, handlerVal.Get()))
            {
                icError("Device handler '%s' missing 'handler' function", name.c_str());
                return false;
            }

            // Root the function immediately (heldFn) so it survives later extraction allocations.
            dh.handler = handlerVal.Get();
            dh.heldFn = SafeJSValue {ctx, handlerVal.Get()};

            // Aliases
            SafeJSValue aliasesVal(ctx, JS_GetPropertyStr(ctx, handlerObj.Get(), "aliases"));

            if (!JS_IsUndefined(aliasesVal.Get()) && !JS_IsNull(aliasesVal.Get()))
            {
                dh.aliases = GetStringArray(ctx, aliasesVal.Get());
            }

            // Supplements
            SafeJSValue supplementsVal(ctx, JS_GetPropertyStr(ctx, handlerObj.Get(), "supplements"));

            if (!JS_IsUndefined(supplementsVal.Get()) && !JS_IsNull(supplementsVal.Get()))
            {
                dh.supplements = ExtractSupplements(ctx, supplementsVal.Get());
            }

            out.push_back(std::move(dh));
        }

        return true;
    }

    SbmdSupplements SbmdLoader::ExtractSupplements(JSContext *ctx, JSValue supplementsObj)
    {
        SafeJSValue rootedSupplements(ctx, supplementsObj);
        SbmdSupplements supplements;

        SafeJSValue attrsVal(ctx, JS_GetPropertyStr(ctx, rootedSupplements.Get(), "attributes"));

        if (!JS_IsUndefined(attrsVal.Get()) && !JS_IsNull(attrsVal.Get()))
        {
            supplements.attributes = GetStringArray(ctx, attrsVal.Get());
        }

        SafeJSValue resVal(ctx, JS_GetPropertyStr(ctx, rootedSupplements.Get(), "resources"));

        if (!JS_IsUndefined(resVal.Get()) && !JS_IsNull(resVal.Get()))
        {
            supplements.resources = GetStringArray(ctx, resVal.Get());
        }

        SafeJSValue persistVal(ctx, JS_GetPropertyStr(ctx, rootedSupplements.Get(), "persistentData"));

        if (!JS_IsUndefined(persistVal.Get()) && !JS_IsNull(persistVal.Get()))
        {
            supplements.persistentData = GetStringArray(ctx, persistVal.Get());
        }

        SafeJSValue transientVal(ctx, JS_GetPropertyStr(ctx, rootedSupplements.Get(), "transientData"));

        if (!JS_IsUndefined(transientVal.Get()) && !JS_IsNull(transientVal.Get()))
        {
            supplements.transientData = GetStringArray(ctx, transientVal.Get());
        }

        return supplements;
    }

} // namespace barton
