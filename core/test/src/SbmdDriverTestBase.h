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
 * Shared harness for SBMD driver/handler unit tests.
 *
 * Provides the generic, driver-agnostic scaffolding that every SBMD test needs:
 *   - the C API stubs (updateResource / setMetadata / deviceServiceSetMetadata) that
 *     ExecuteOps links against, recording their calls into the g_*Calls vectors
 *     (defined in SbmdDriverTestSupport.cpp)
 *   - MQuickJS runtime setup/teardown
 *   - JS helpers (Ctx / EvalFunc / GetStringProp / GetUint32Prop / EncodeTlv / DecodeTlv)
 *   - supplement prefetch-and-attach plumbing
 *   - ParsedResult assertion helpers (ExpectError / ExpectSuccess / ExpectSendCommand /
 *     ExpectRequestCommand / FindUpdateResource / ExpectUpdateResource / FindTransientData)
 *
 * Driver-specific concerns (loading a particular .sbmd.js and finding its handlers) live
 * in the concrete fixtures that derive from SbmdDriverTestBase.
 */

#pragma once

#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"

#include <gtest/gtest.h>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton::test
{
    // ========================================================================
    // Recorded C-API calls (populated by the stubs in SbmdDriverTestSupport.cpp)
    // ========================================================================
    struct UpdateResourceCall
    {
        std::string deviceUuid;
        std::string endpointId;
        std::string resourceId;
        std::string value;
        std::string metadata; // JSON string, empty if null
    };

    struct SetMetadataCall
    {
        std::string deviceUuid;
        std::string endpointId;
        std::string key;
        std::string value;
    };

    struct SetPersistentDataCall
    {
        std::string uri;
        std::string value;
    };

    extern std::vector<UpdateResourceCall> g_updateResourceCalls;
    extern std::vector<SetMetadataCall> g_setMetadataCalls;
    extern std::vector<SetPersistentDataCall> g_setPersistentDataCalls;

    // ========================================================================
    // Base fixture: generic SBMD test harness
    // ========================================================================
    class SbmdDriverTestBase : public ::testing::Test
    {
    protected:
        /**
         * Bring up the shared MQuickJS runtime and load the SBMD JS bundle. Concrete
         * fixtures call this from their SetUpTestSuite before loading a specific driver.
         */
        static void InitRuntime()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdLoader::InjectCaptureFunction(ctx));
        }

        static void ShutdownRuntime() { MQuickJsRuntime::Shutdown(); }

        void SetUp() override
        {
            g_updateResourceCalls.clear();
            g_setMetadataCalls.clear();
            g_setPersistentDataCalls.clear();
        }

        JSContext *Ctx() { return MQuickJsRuntime::GetSharedContext(); }

        HandlerContext MakeContext(const std::string &deviceUuid,
                                   const std::string &endpointId = "1",
                                   const std::map<uint32_t, uint32_t> &featureMaps = {})
        {
            HandlerContext hctx;
            hctx.deviceUuid = deviceUuid;
            hctx.endpointId = endpointId;
            hctx.clusterFeatureMaps = featureMaps;

            return hctx;
        }

        JSValue EvalFunc(const char *expr) { return JS_Eval(Ctx(), expr, strlen(expr), "<test>", JS_EVAL_RETVAL); }

        /**
         * Get a string property from a JSValue object ("" if absent/null).
         */
        std::string GetStringProp(JSValue obj, const char *name)
        {
            auto *ctx = Ctx();
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return "";
            }

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, val, &buf);

            return str ? std::string(str) : "";
        }

        uint32_t GetUint32Prop(JSValue obj, const char *name)
        {
            auto *ctx = Ctx();
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val))
            {
                return 0;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val);

            return result;
        }

        /**
         * Encode a TLV struct and return the base64 string.
         * @param schema  JS object literal for the schema, e.g. "{f:{tag:0,type:'uint16'}}"
         * @param values  JS object literal for the values, e.g. "{f: 42}"
         */
        std::string EncodeTlv(const char *schema, const char *values)
        {
            std::string expr =
                std::string("(function(){return Sbmd.Tlv.encodeStruct(") + values + "," + schema + ");})()";
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            JSValue val = JS_Eval(Ctx(), expr.c_str(), expr.size(), "<test>", JS_EVAL_RETVAL);
            EXPECT_FALSE(JS_IsException(val)) << "TLV encode failed for: " << expr;
            JSCStringBuf buf;
            const char *str = JS_ToCString(Ctx(), val, &buf);
            EXPECT_NE(str, nullptr);

            return str ? std::string(str) : std::string();
        }

        /**
         * Decode a TLV base64 string and return the JS decoded object.
         * Caller must hold MQuickJsRuntime::GetMutex().
         */
        JSValue DecodeTlv(const std::string &tlvBase64)
        {
            std::string expr = "(function(){ return Sbmd.Tlv.decode('" + tlvBase64 + "'); })()";
            JSValue decoded = JS_Eval(Ctx(), expr.c_str(), expr.size(), "<test>", JS_EVAL_RETVAL);
            EXPECT_FALSE(JS_IsException(decoded)) << "TLV decode failed";

            return decoded;
        }

        /**
         * Resolve the declared supplements via PrefetchSupplements, then attach them with
         * AddSupplements — the full fetch-then-attach path through the two-phase seam.
         */
        template<typename AttrFetcher, typename ResFetcher, typename PersistFetcher, typename TransientFetcher>
        void FetchAndAddSupplements(JSContext *ctx,
                                    SafeJSValue &args,
                                    const SbmdSupplements &supplements,
                                    AttrFetcher attrFetcher,
                                    ResFetcher resFetcher,
                                    PersistFetcher persistFetcher,
                                    TransientFetcher transientFetcher)
        {
            FetchedSupplements fetched = SbmdHandlerInvoker::PrefetchSupplements(
                supplements, attrFetcher, resFetcher, persistFetcher, transientFetcher);
            SbmdHandlerInvoker::AddSupplements(ctx, args, supplements, fetched);
        }

        // ---- ParsedResult assertion helpers ----

        /**
         * Assert the result is an error with the given message.
         */
        void ExpectError(const std::optional<ParsedResult> &result, const std::string &message)
        {
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Error>(result->terminal.data));
            EXPECT_EQ(std::get<ResultTerminal::Error>(result->terminal.data).message, message);
        }

        /**
         * Assert the result is an error containing the given substring.
         */
        void ExpectErrorContains(const std::optional<ParsedResult> &result, const std::string &substr)
        {
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Error>(result->terminal.data));
            EXPECT_TRUE(std::get<ResultTerminal::Error>(result->terminal.data).message.find(substr) !=
                        std::string::npos)
                << "Expected error containing '" << substr
                << "', got: " << std::get<ResultTerminal::Error>(result->terminal.data).message;
        }

        /**
         * Assert the result is a SendCommand and return the command data.
         */
        const ResultTerminal::SendCommand &
        ExpectSendCommand(const std::optional<ParsedResult> &result, uint32_t clusterId, uint32_t commandId)
        {
            EXPECT_TRUE(result.has_value());
            EXPECT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));

            auto &cmd = std::get<ResultTerminal::SendCommand>(result->terminal.data);
            EXPECT_EQ(cmd.clusterId, clusterId);
            EXPECT_EQ(cmd.commandId, commandId);
            EXPECT_FALSE(cmd.tlvBase64.empty());

            return cmd;
        }

        /**
         * Assert the result is a RequestCommand and return the command data.
         */
        const ResultTerminal::RequestCommand &
        ExpectRequestCommand(const std::optional<ParsedResult> &result, uint32_t clusterId, uint32_t commandId)
        {
            EXPECT_TRUE(result.has_value());
            EXPECT_TRUE(std::holds_alternative<ResultTerminal::RequestCommand>(result->terminal.data));

            auto &cmd = std::get<ResultTerminal::RequestCommand>(result->terminal.data);
            EXPECT_EQ(cmd.clusterId, clusterId);
            EXPECT_EQ(cmd.commandId, commandId);

            return cmd;
        }

        /**
         * Find the first UpdateResource op matching the given resource ID, or nullptr.
         */
        const ResultOp::UpdateResource *FindUpdateResource(const ParsedResult &result, const std::string &resourceId)
        {
            for (const auto &op : result.ops)
            {
                if (std::holds_alternative<ResultOp::UpdateResource>(op.data))
                {
                    auto &ur = std::get<ResultOp::UpdateResource>(op.data);

                    if (ur.resource == resourceId)
                    {
                        return &ur;
                    }
                }
            }

            return nullptr;
        }

        /**
         * Assert the result is a Success terminal.
         */
        void ExpectSuccess(const std::optional<ParsedResult> &result)
        {
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));
        }

        /**
         * Assert an UpdateResource op for the given resource exists (on the webrtc endpoint when an
         * endpoint is set) and return it.
         */
        const ResultOp::UpdateResource *ExpectUpdateResource(const ParsedResult &result, const std::string &resourceId)
        {
            const auto *ur = FindUpdateResource(result, resourceId);
            EXPECT_NE(ur, nullptr) << "Expected updateResource for " << resourceId;

            if (ur != nullptr && ur->endpoint.has_value())
            {
                EXPECT_EQ(*ur->endpoint, "webrtc");
            }

            return ur;
        }

        /**
         * As above, additionally asserting the resource's value.
         */
        const ResultOp::UpdateResource *
        ExpectUpdateResource(const ParsedResult &result, const std::string &resourceId, const std::string &value)
        {
            const auto *ur = ExpectUpdateResource(result, resourceId);

            if (ur != nullptr)
            {
                EXPECT_EQ(ur->value, value);
            }

            return ur;
        }

        /**
         * Find the first SetTransientData op matching the given key, or nullptr.
         */
        const ResultOp::SetTransientData *FindTransientData(const ParsedResult &result, const std::string &key)
        {
            for (const auto &op : result.ops)
            {
                if (std::holds_alternative<ResultOp::SetTransientData>(op.data))
                {
                    auto &td = std::get<ResultOp::SetTransientData>(op.data);

                    if (td.key == key)
                    {
                        return &td;
                    }
                }
            }

            return nullptr;
        }
    };
} // namespace barton::test
