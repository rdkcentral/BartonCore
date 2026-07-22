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
 * Unit tests for camera.sbmd.js WebRTC endpoint handlers.
 *
 * Tests load and activate the real camera.sbmd.js spec file, then exercise
 * the actual handler functions — no inline copies that can drift.
 *
 * Tests cover:
 *   - executeLocalSdp (offerer flow): TLV encoding (null webRTCSessionID, correct tags), error paths
 *   - executeLocalIceCandidates: valid JSON array → sendCommand, invalid JSON → error
 *   - handleIncomingOffer / handleIncomingAnswer / handleIncomingIceCandidates / handleIncomingEnd
 *   - executeDestroySession with streaming session: sends EndSession command
 */

#include "deviceDrivers/matter/sbmd/SbmdDriver.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"

#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

extern "C" {
#include <cjson/cJSON.h>
#include <mquickjs/mquickjs.h>
}

using namespace barton;

// ============================================================================
// Test stubs for C APIs called by ExecuteOps
// ============================================================================

namespace
{
    struct UpdateResourceCall
    {
        std::string deviceUuid;
        std::string endpointId;
        std::string resourceId;
        std::string value;
        std::string metadata;
    };

    std::vector<UpdateResourceCall> g_updateResourceCalls;
} // namespace

extern "C" {
void updateResource(const char *deviceUuid,
                    const char *endpointId,
                    const char *resourceId,
                    const char *newValue,
                    void *metadata)
{
    std::string metaStr;

    if (metadata != nullptr)
    {
        char *printed = cJSON_PrintUnformatted(static_cast<cJSON *>(metadata));

        if (printed != nullptr)
        {
            metaStr = printed;
            free(printed);
        }
    }

    g_updateResourceCalls.push_back({deviceUuid ? deviceUuid : "",
                                     endpointId ? endpointId : "",
                                     resourceId ? resourceId : "",
                                     newValue ? newValue : "",
                                     metaStr});
}

void setMetadata(const char *, const char *, const char *, const char *) {}

bool deviceServiceSetMetadata(const char *, const char *)
{
    return true;
}
}

namespace
{
    // ========================================================================
    // Constants matching camera.sbmd.js
    // ========================================================================
    constexpr uint32_t CL_WEBRTC_TRANSPORT_PROVIDER = 0x0553;
    constexpr uint32_t CL_WEBRTC_TRANSPORT_REQUESTOR = 0x0554;
    constexpr uint32_t CL_CAMERA_AV_STREAM_MGMT = 0x0551;
    constexpr uint32_t CMD_PROVIDE_OFFER = 0x02;
    constexpr uint32_t CMD_PROVIDE_OFFER_RESP = 0x03;
    constexpr uint32_t CMD_PROVIDE_ICE = 0x05;
    constexpr uint32_t CMD_END_SESSION = 0x06;
    constexpr uint32_t CMD_VIDEO_STREAM_ALLOCATE = 0x03;
    constexpr uint32_t CMD_VIDEO_STREAM_ALLOCATE_RESP = 0x04;
    constexpr uint32_t CMD_OFFER = 0x00;
    constexpr uint32_t CMD_ANSWER = 0x01;
    constexpr uint32_t CMD_ICE_CANDIDATES = 0x02;
    constexpr uint32_t CMD_END = 0x03;

    // providerAcceptedCommands (AcceptedCommandList) as base64 TLV: a top-level TLV array of
    // command IDs advertising ProvideOffer (0x02) but NOT SolicitOffer (0x00), so
    // pickNegotiationRole selects the offerer (client-offers / ProvideOffer) flow.
    // TLV bytes: 0x16(array) 0x04(uint8) 0x02 0x18(end).
    constexpr const char *OFFERER_ACCEPTED_CMDS = "FgQCGA==";

    // ========================================================================
    // Test Fixture — loads the real camera.sbmd.js via SbmdDriver
    // ========================================================================
    class SbmdCameraWebrtcTest : public ::testing::Test
    {
    protected:
        static std::unique_ptr<SbmdDriver> s_driver;

        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdLoader::InjectCaptureFunction(ctx));

            // Load the real camera.sbmd.js spec file
            std::string specPath = std::string(SBMD_SPEC_DIR) + "camera.sbmd.js";
            std::ifstream file(specPath, std::ios::binary | std::ios::ate);
            ASSERT_TRUE(file.is_open()) << "Failed to open " << specPath;
            auto fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            std::string source(static_cast<size_t>(fileSize), '\0');
            file.read(source.data(), fileSize);
            file.close();

            auto reg = SbmdLoader::LoadDriver(ctx, specPath, source.c_str(), source.size());
            ASSERT_NE(reg, nullptr) << "SbmdLoader::LoadDriver failed for camera.sbmd.js";

            s_driver = std::make_unique<SbmdDriver>(std::move(reg), std::move(source));
            ASSERT_TRUE(s_driver->Activate(ctx)) << "SbmdDriver::Activate failed";
        }

        static void TearDownTestSuite()
        {
            if (s_driver)
            {
                auto *ctx = MQuickJsRuntime::GetSharedContext();
                s_driver->Deactivate(ctx);
                s_driver.reset();
            }

            MQuickJsRuntime::Shutdown();
        }

        void SetUp() override { g_updateResourceCalls.clear(); }

        JSContext *Ctx() { return MQuickJsRuntime::GetSharedContext(); }

        HandlerContext MakeContext()
        {
            HandlerContext hctx;
            hctx.deviceUuid = "test-camera-uuid";
            hctx.endpointId = "1";

            return hctx;
        }

        JSValue EvalFunc(const char *expr) { return JS_Eval(Ctx(), expr, strlen(expr), "<test>", JS_EVAL_RETVAL); }

        /**
         * Find a resource execute handler by endpoint and resource ID.
         */
        JSValue FindResourceHandler(const std::string &endpointId, const std::string &resourceId)
        {
            const auto &reg = s_driver->GetRegistration();

            for (const auto &ep : reg.endpoints)
            {
                if (ep.id == endpointId)
                {
                    for (const auto &r : ep.resources)
                    {
                        if (r.id == resourceId && r.execute.has_value())
                        {
                            return r.execute->Fn();
                        }
                    }
                }
            }

            return JS_UNDEFINED;
        }

        /**
         * Find the supplements for a resource execute handler.
         */
        const SbmdSupplements *FindResourceSupplements(const std::string &endpointId, const std::string &resourceId)
        {
            const auto &reg = s_driver->GetRegistration();

            for (const auto &ep : reg.endpoints)
            {
                if (ep.id == endpointId)
                {
                    for (const auto &r : ep.resources)
                    {
                        if (r.id == resourceId && r.execute.has_value())
                        {
                            return &r.execute->supplements;
                        }
                    }
                }
            }

            return nullptr;
        }

        /**
         * Find a command handler by name from the registration's commandHandlers vector.
         * The camera command handlers are bound via aliases; this test drives them directly by
         * registration name rather than going through the dispatch table.
         */
        JSValue FindCommandHandler(const std::string &name)
        {
            const auto &reg = s_driver->GetRegistration();

            for (const auto &ch : reg.commandHandlers)
            {
                if (ch.name == name)
                {
                    return ch.Fn();
                }
            }

            return JS_UNDEFINED;
        }

        /**
         * Find supplements for a command handler by name.
         */
        const SbmdSupplements *FindCommandSupplements(const std::string &name)
        {
            const auto &reg = s_driver->GetRegistration();

            for (const auto &ch : reg.commandHandlers)
            {
                if (ch.name == name)
                {
                    return &ch.supplements;
                }
            }

            return nullptr;
        }

        /**
         * Build and invoke a resource execute handler from the loaded driver.
         */
        std::optional<ParsedResult> InvokeExecuteHandler(const std::string &endpointId,
                                                         const std::string &resourceId,
                                                         const std::string &input,
                                                         const std::string &sessionsJson,
                                                         const std::string &webrtcMapJson = "",
                                                         const std::map<uint32_t, uint32_t> &featureMaps = {},
                                                         const std::string &acceptedCmdsBase64 = "")
        {
            auto hctx = MakeContext();
            hctx.clusterFeatureMaps = featureMaps;
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

            JSValue handler = FindResourceHandler(endpointId, resourceId);

            if (JS_IsUndefined(handler))
            {
                ADD_FAILURE() << "No execute handler for " << endpointId << "/" << resourceId;
                return std::nullopt;
            }

            SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, resourceId, input);

            const auto *sup = FindResourceSupplements(endpointId, resourceId);
            SbmdSupplements supplements = sup ? *sup : SbmdSupplements {};

            auto fetched = SbmdHandlerInvoker::PrefetchSupplements(
                supplements,
                [&](const std::string &aliasName) -> std::optional<std::string> {
                    if (aliasName == "providerAcceptedCommands" && !acceptedCmdsBase64.empty())
                    {
                        return acceptedCmdsBase64;
                    }

                    return std::nullopt;
                },
                [](const std::string &) { return std::nullopt; },
                [](const std::string &) { return std::nullopt; },
                [&](const std::string &key) -> std::optional<std::string> {
                    if (key == "sessions")
                    {
                        return sessionsJson;
                    }

                    if (key == "webrtcSessionMap")
                    {
                        return webrtcMapJson.empty() ? std::nullopt : std::optional<std::string>(webrtcMapJson);
                    }

                    return std::nullopt;
                });

            SbmdHandlerInvoker::AddSupplements(Ctx(), args, supplements, fetched);

            return SbmdHandlerInvoker::InvokeHandler(Ctx(), handler, args);
        }

        /**
         * Build and invoke a command handler from the loaded driver by handler name.
         */
        std::optional<ParsedResult> InvokeCommandHandler(const std::string &handlerName,
                                                         uint32_t clusterId,
                                                         uint32_t commandId,
                                                         const std::string &tlvBase64,
                                                         const std::string &sessionsJson,
                                                         const std::string &webrtcMapJson = "")
        {
            auto hctx = MakeContext();
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

            JSValue handler = FindCommandHandler(handlerName);

            if (JS_IsUndefined(handler))
            {
                ADD_FAILURE() << "No command handler named '" << handlerName << "'";
                return std::nullopt;
            }

            SafeJSValue args = SbmdHandlerInvoker::BuildCommandArgs(Ctx(), hctx, clusterId, commandId, tlvBase64);

            const auto *sup = FindCommandSupplements(handlerName);
            SbmdSupplements supplements = sup ? *sup : SbmdSupplements {};

            auto fetched = SbmdHandlerInvoker::PrefetchSupplements(
                supplements,
                [](const std::string &) { return std::nullopt; },
                [](const std::string &) { return std::nullopt; },
                [](const std::string &) { return std::nullopt; },
                [&](const std::string &key) -> std::optional<std::string> {
                    if (key == "sessions")
                    {
                        return sessionsJson;
                    }

                    if (key == "webrtcSessionMap")
                    {
                        return webrtcMapJson.empty() ? std::nullopt : std::optional<std::string>(webrtcMapJson);
                    }

                    return std::nullopt;
                });

            SbmdHandlerInvoker::AddSupplements(Ctx(), args, supplements, fetched);

            return SbmdHandlerInvoker::InvokeHandler(Ctx(), handler, args);
        }

        /**
         * Invoke a callback captured in a parsed result (e.g. a requestCommand's onError/onResponse
         * continuation) with a caller-supplied args object. Used to exercise the offer-flow error
         * handlers, which are not registered execute/command handlers.
         *
         * @param fn        the callback captured in a RequestCommand terminal
         * @param argsExpr  a JS expression yielding the args object
         */
        std::optional<ParsedResult> InvokeCallback(const SafeJSValue &fn, const std::string &argsExpr)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

            if (!fn.HasValue())
            {
                ADD_FAILURE() << "Callback has no value";
                return std::nullopt;
            }

            JSValue argsVal = JS_Eval(Ctx(), argsExpr.c_str(), argsExpr.size(), "<test-args>", JS_EVAL_RETVAL);
            SafeJSValue args(Ctx(), argsVal);

            return SbmdHandlerInvoker::InvokeHandler(Ctx(), fn.Get(), args);
        }

        // ---- Assertion helpers ----

        /**
         * Encode a TLV struct and return the base64 string.
         * @param schema  JS object literal for the schema, e.g. "{f:{tag:0,type:'uint16'}}"
         * @param values  JS object literal for the values, e.g. "{f: 42}"
         */
        std::string EncodeTlv(const char *schema, const char *values)
        {
            std::string expr =
                std::string("(function(){return Sbmd.Tlv.encodeStruct(") + values + "," + schema + ");})()";
            ;
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

    std::unique_ptr<SbmdDriver> SbmdCameraWebrtcTest::s_driver;

    // ========================================================================
    // 5.1 — executeOfferSdp
    // ========================================================================

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferSdpValidSessionProducesVideoStreamAllocate)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto result =
            InvokeExecuteHandler("webrtc", "localSdp", "test-offer-sdp", sessions, "", {}, OFFERER_ACCEPTED_CMDS);

        auto &cmd = ExpectRequestCommand(result, CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE);
        EXPECT_EQ(cmd.responseCommandId, CMD_VIDEO_STREAM_ALLOCATE_RESP);
        EXPECT_FALSE(JS_IsUndefined(cmd.onResponse));
        EXPECT_FALSE(JS_IsUndefined(cmd.onError));

        ASSERT_GE(result->ops.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<ResultOp::SetTransientData>(result->ops[0].data));
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferSdpAllocateTlvHasStreamUsage)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto result =
            InvokeExecuteHandler("webrtc", "localSdp", "test-offer-sdp", sessions, "", {}, OFFERER_ACCEPTED_CMDS);
        auto &cmd = ExpectRequestCommand(result, CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE);

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        JSValue decoded = DecodeTlv(cmd.tlvBase64);

        // Tag 0 = StreamUsage (enum8), value 3 = LiveView
        JSValue tag0 = JS_GetPropertyUint32(Ctx(), decoded, 0);
        ASSERT_FALSE(JS_IsUndefined(tag0)) << "StreamUsage (tag 0) must be present";
        int32_t streamUsage = 0;
        JS_ToInt32(Ctx(), &streamUsage, tag0);
        EXPECT_EQ(streamUsage, 3) << "StreamUsage should be 3 (LiveView)";
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferSdpAllocateTlvHasCorrectFields)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto result =
            InvokeExecuteHandler("webrtc", "localSdp", "test-offer-sdp", sessions, "", {}, OFFERER_ACCEPTED_CMDS);
        auto &cmd = ExpectRequestCommand(result, CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE);

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        JSValue decoded = DecodeTlv(cmd.tlvBase64);

        // Tag 0 = StreamUsage (enum8) = 3 (LiveView)
        JSValue tag0 = JS_GetPropertyUint32(Ctx(), decoded, 0);
        int32_t streamUsage = 0;
        JS_ToInt32(Ctx(), &streamUsage, tag0);
        EXPECT_EQ(streamUsage, 3);

        // Tag 1 = VideoCodec (enum8) = 0 (H264)
        JSValue tag1 = JS_GetPropertyUint32(Ctx(), decoded, 1);
        ASSERT_FALSE(JS_IsUndefined(tag1)) << "VideoCodec (tag 1) must be present";
        int32_t videoCodec = -1;
        JS_ToInt32(Ctx(), &videoCodec, tag1);
        EXPECT_EQ(videoCodec, 0) << "VideoCodec should be 0 (H264)";

        // Tag 2 = MinFrameRate (uint16) >= 1
        JSValue tag2 = JS_GetPropertyUint32(Ctx(), decoded, 2);
        ASSERT_FALSE(JS_IsUndefined(tag2)) << "MinFrameRate (tag 2) must be present";
        int32_t minFps = 0;
        JS_ToInt32(Ctx(), &minFps, tag2);
        EXPECT_GE(minFps, 1);

        // Tag 3 = MaxFrameRate (uint16) >= MinFrameRate
        JSValue tag3 = JS_GetPropertyUint32(Ctx(), decoded, 3);
        ASSERT_FALSE(JS_IsUndefined(tag3)) << "MaxFrameRate (tag 3) must be present";
        int32_t maxFps = 0;
        JS_ToInt32(Ctx(), &maxFps, tag3);
        EXPECT_GE(maxFps, minFps);

        // Tag 4 = MinResolution (struct with Width/Height)
        JSValue tag4 = JS_GetPropertyUint32(Ctx(), decoded, 4);
        ASSERT_FALSE(JS_IsUndefined(tag4)) << "MinResolution (tag 4) must be present";

        // Tag 5 = MaxResolution (struct with Width/Height)
        JSValue tag5 = JS_GetPropertyUint32(Ctx(), decoded, 5);
        ASSERT_FALSE(JS_IsUndefined(tag5)) << "MaxResolution (tag 5) must be present";

        // Tag 6 = MinBitRate (uint32) >= 1
        JSValue tag6 = JS_GetPropertyUint32(Ctx(), decoded, 6);
        ASSERT_FALSE(JS_IsUndefined(tag6)) << "MinBitRate (tag 6) must be present";

        // Tag 7 = MaxBitRate (uint32) >= MinBitRate
        JSValue tag7 = JS_GetPropertyUint32(Ctx(), decoded, 7);
        ASSERT_FALSE(JS_IsUndefined(tag7)) << "MaxBitRate (tag 7) must be present";

        // Tag 8 = KeyFrameInterval (uint16)
        JSValue tag8 = JS_GetPropertyUint32(Ctx(), decoded, 8);
        ASSERT_FALSE(JS_IsUndefined(tag8)) << "KeyFrameInterval (tag 8) must be present";

        // Tags 9 and 10 should NOT be present when featureMap has no Watermark/OSD
        JSValue tag9 = JS_GetPropertyUint32(Ctx(), decoded, 9);
        EXPECT_TRUE(JS_IsUndefined(tag9)) << "WatermarkEnabled (tag 9) must NOT be present without feature";
        JSValue tag10 = JS_GetPropertyUint32(Ctx(), decoded, 10);
        EXPECT_TRUE(JS_IsUndefined(tag10)) << "OSDEnabled (tag 10) must NOT be present without feature";
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferSdpAllocateIncludesWatermarkAndOsdWhenFeatured)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        // Feature bits: kWatermark=0x40, kOnScreenDisplay=0x80
        std::map<uint32_t, uint32_t> featureMaps = {
            {CL_CAMERA_AV_STREAM_MGMT, 0xC0}
        };
        auto result = InvokeExecuteHandler(
            "webrtc", "localSdp", "test-offer-sdp", sessions, "", featureMaps, OFFERER_ACCEPTED_CMDS);
        auto &cmd = ExpectRequestCommand(result, CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE);

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        JSValue decoded = DecodeTlv(cmd.tlvBase64);

        // Tag 9 = WatermarkEnabled (bool)
        JSValue tag9 = JS_GetPropertyUint32(Ctx(), decoded, 9);
        ASSERT_FALSE(JS_IsUndefined(tag9)) << "WatermarkEnabled (tag 9) must be present with kWatermark feature";

        // Tag 10 = OSDEnabled (bool)
        JSValue tag10 = JS_GetPropertyUint32(Ctx(), decoded, 10);
        ASSERT_FALSE(JS_IsUndefined(tag10)) << "OSDEnabled (tag 10) must be present with kOnScreenDisplay feature";
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferSdpContextCarriesSdp)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto result =
            InvokeExecuteHandler("webrtc", "localSdp", "test-offer-sdp", sessions, "", {}, OFFERER_ACCEPTED_CMDS);
        auto &cmd = ExpectRequestCommand(result, CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE);

        // Verify context carries the SDP for the chained ProvideOffer
        ASSERT_FALSE(JS_IsUndefined(cmd.context));
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        JSValue sdpVal = JS_GetPropertyStr(Ctx(), cmd.context, "sdp");
        ASSERT_TRUE(JS_IsString(Ctx(), sdpVal)) << "context.sdp must be a string";
        JSCStringBuf buf;
        const char *sdpStr = JS_ToCString(Ctx(), sdpVal, &buf);
        EXPECT_STREQ(sdpStr, "test-offer-sdp");
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferSdpMissingSessionReturnsError)
    {
        std::string sessions = R"({"1":{"state":"created","protocol":"webrtc"}})";
        ExpectError(
            InvokeExecuteHandler("webrtc", "localSdp", "test-offer-sdp", sessions, "", {}, OFFERER_ACCEPTED_CMDS),
            "No active streaming session");
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferSdpMissingInputReturnsError)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        ExpectError(InvokeExecuteHandler("webrtc", "localSdp", "", sessions, "", {}, OFFERER_ACCEPTED_CMDS),
                    "SDP string required");
    }

    // ========================================================================
    // 5.2 — executeOfferIceCandidates
    // ========================================================================

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferIceCandidatesValidArrayProducesSendCommand)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        std::string input = R"(["candidate:1 udp 123 192.168.1.1 5000 typ host"])";
        auto result = InvokeExecuteHandler("webrtc", "localIceCandidates", input, sessions);
        ExpectSendCommand(result, CL_WEBRTC_TRANSPORT_PROVIDER, CMD_PROVIDE_ICE);
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferIceCandidatesInvalidJsonReturnsError)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        ExpectErrorContains(InvokeExecuteHandler("webrtc", "localIceCandidates", "not valid json{", sessions),
                            "Invalid JSON");
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferIceCandidatesNotArrayReturnsError)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        ExpectErrorContains(InvokeExecuteHandler("webrtc", "localIceCandidates", R"({"not":"array"})", sessions),
                            "JSON array");
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteOfferIceCandidatesTlvHasCorrectFields)
    {
        // ProvideICECandidates (0x0553 cmd 0x05): WebRTCSessionID(0), ICECandidates(1)
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        std::string input = R"(["candidate:1 udp 123 192.168.1.1 5000 typ host"])";
        auto result = InvokeExecuteHandler("webrtc", "localIceCandidates", input, sessions);
        auto &cmd = ExpectSendCommand(result, CL_WEBRTC_TRANSPORT_PROVIDER, CMD_PROVIDE_ICE);

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        JSValue decoded = DecodeTlv(cmd.tlvBase64);

        // Tag 0 = WebRTCSessionID (uint16)
        JSValue tag0 = JS_GetPropertyUint32(Ctx(), decoded, 0);
        ASSERT_FALSE(JS_IsUndefined(tag0)) << "WebRTCSessionID (tag 0) must be present";
        ASSERT_FALSE(JS_IsNull(tag0)) << "WebRTCSessionID must not be null for ICE candidates";
        int32_t sessionId = 0;
        JS_ToInt32(Ctx(), &sessionId, tag0);
        EXPECT_EQ(sessionId, 42);

        // Tag 1 = ICECandidates (array of ICECandidateStruct)
        JSValue tag1 = JS_GetPropertyUint32(Ctx(), decoded, 1);
        ASSERT_FALSE(JS_IsUndefined(tag1)) << "ICECandidates (tag 1) must be present";
        // Verify it's an array by checking it has a 'length' property
        JSValue len = JS_GetPropertyStr(Ctx(), tag1, "length");
        ASSERT_FALSE(JS_IsUndefined(len)) << "ICECandidates must be an array (has length)";
        int32_t arrLen = 0;
        JS_ToInt32(Ctx(), &arrLen, len);
        EXPECT_GE(arrLen, 1) << "ICECandidates array must have at least one entry";
    }

    TEST_F(SbmdCameraWebrtcTest, ReproLiveIceCandidates)
    {
        std::string sessions = R"({"10":{"state":"streaming","protocol":"webrtc","webRTCSessionID":10}})";
        std::string input =
            R"(["candidate:1 1 UDP 2015363327 172.29.0.2 44332 typ host","candidate:2 1 TCP 1015021823 172.29.0.2 9 typ host tcptype active","candidate:3 1 TCP 1010827519 172.29.0.2 51837 typ host tcptype passive","candidate:4 1 UDP 2015363583 fd00:c93:eb1::2 43089 typ host","candidate:5 1 TCP 1015022079 fd00:c93:eb1::2 9 typ host tcptype active","candidate:6 1 TCP 1010827775 fd00:c93:eb1::2 34401 typ host tcptype passive","candidate:7 1 UDP 2015363839 fe80::a4a6:7bff:fe56:592e 50953 typ host","candidate:8 1 TCP 1015022335 fe80::a4a6:7bff:fe56:592e 9 typ host tcptype active","candidate:9 1 TCP 1010828031 fe80::a4a6:7bff:fe56:592e 60109 typ host tcptype passive",""])";
        auto result = InvokeExecuteHandler("webrtc", "localIceCandidates", input, sessions);
        ASSERT_TRUE(result.has_value()) << "handler returned no result (threw)";

        if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
        {
            FAIL() << "handler error: " << std::get<ResultTerminal::Error>(result->terminal.data).message;
        }

        ExpectSendCommand(result, CL_WEBRTC_TRANSPORT_PROVIDER, CMD_PROVIDE_ICE);
    }

    // Reproduce the live "not a function" failure by forcing garbage collection
    // between/around handler invocations. In live, localIceCandidates and
    // destroySession run late in a long-running shared context after many GCs;
    // if any handler JSValue (or a global it depends on) is collectible, JS_Call
    // throws TypeError "not a function" before entering the handler.
    TEST_F(SbmdCameraWebrtcTest, IceCandidatesSurvivesGc)
    {
        std::string sessions = R"({"10":{"state":"streaming","protocol":"webrtc","webRTCSessionID":10}})";
        std::string input = R"(["candidate:1 1 UDP 2015363327 172.29.0.2 44332 typ host",""])";

        // Force GC repeatedly, invoking the handler after each collection.
        for (int i = 0; i < 10; i++)
        {
            {
                std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
                const char *expr = "gc()";
                JSValue v = JS_Eval(Ctx(), expr, strlen(expr), "<test>", JS_EVAL_RETVAL);
                ASSERT_FALSE(JS_IsException(v)) << "gc() threw on iteration " << i;
            }

            auto result = InvokeExecuteHandler("webrtc", "localIceCandidates", input, sessions);
            ASSERT_TRUE(result.has_value()) << "localIceCandidates threw after gc on iteration " << i;

            if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
            {
                FAIL() << "iteration " << i
                       << " error: " << std::get<ResultTerminal::Error>(result->terminal.data).message;
            }

            ExpectSendCommand(result, CL_WEBRTC_TRANSPORT_PROVIDER, CMD_PROVIDE_ICE);

            auto destroy = InvokeExecuteHandler("camera", "destroySession", "10", sessions);
            ASSERT_TRUE(destroy.has_value()) << "destroySession threw after gc on iteration " << i;
        }
    }

    // ========================================================================
    // 5.3 — EndSession TLV conformance
    // ========================================================================

    TEST_F(SbmdCameraWebrtcTest, DestroySessionTlvHasCorrectEndSessionFields)
    {
        // EndSession (0x0553 cmd 0x06): WebRTCSessionID(0), Reason(1)
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        auto result = InvokeExecuteHandler("camera", "destroySession", "1", sessions);
        auto &cmd = ExpectSendCommand(result, CL_WEBRTC_TRANSPORT_PROVIDER, CMD_END_SESSION);

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        JSValue decoded = DecodeTlv(cmd.tlvBase64);

        // Tag 0 = WebRTCSessionID (uint16)
        JSValue tag0 = JS_GetPropertyUint32(Ctx(), decoded, 0);
        ASSERT_FALSE(JS_IsUndefined(tag0)) << "WebRTCSessionID (tag 0) must be present";
        ASSERT_FALSE(JS_IsNull(tag0)) << "WebRTCSessionID must not be null for EndSession";
        int32_t sessionId = 0;
        JS_ToInt32(Ctx(), &sessionId, tag0);
        EXPECT_EQ(sessionId, 42);

        // Tag 1 = Reason (enum8)
        JSValue tag1 = JS_GetPropertyUint32(Ctx(), decoded, 1);
        ASSERT_FALSE(JS_IsUndefined(tag1)) << "Reason (tag 1) must be present";
        int32_t reason = -1;
        JS_ToInt32(Ctx(), &reason, tag1);
        EXPECT_GE(reason, 0);
        EXPECT_LE(reason, 12) << "Reason must be WebRTCEndReasonEnum (0..12)";
    }

    // ========================================================================
    // 5.4 — Incoming command handlers
    // ========================================================================

    TEST_F(SbmdCameraWebrtcTest, HandleIncomingOfferUpdatesRemoteSdp)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto tlv = EncodeTlv("{webRTCSessionID:{tag:0,type:'uint16'}, sdp:{tag:1,type:'string'}}",
                             "{webRTCSessionID: 42, sdp: 'remote-offer-sdp'}");

        auto result =
            InvokeCommandHandler("handleIncomingOffer", CL_WEBRTC_TRANSPORT_REQUESTOR, CMD_OFFER, tlv, sessions);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        auto *ur = FindUpdateResource(*result, "remoteSdp");
        ASSERT_NE(ur, nullptr) << "Expected updateResource for remoteSdp";
        EXPECT_EQ(ur->value, "remote-offer-sdp");

        if (ur->endpoint.has_value())
        {
            EXPECT_EQ(*ur->endpoint, "webrtc");
        }
    }

    TEST_F(SbmdCameraWebrtcTest, HandleIncomingAnswerUpdatesRemoteSdp)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto tlv = EncodeTlv("{webRTCSessionID:{tag:0,type:'uint16'}, sdp:{tag:1,type:'string'}}",
                             "{webRTCSessionID: 99, sdp: 'remote-answer-sdp'}");

        auto result =
            InvokeCommandHandler("handleIncomingAnswer", CL_WEBRTC_TRANSPORT_REQUESTOR, CMD_ANSWER, tlv, sessions);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        auto *ur = FindUpdateResource(*result, "remoteSdp");
        ASSERT_NE(ur, nullptr) << "Expected updateResource for remoteSdp";
        EXPECT_EQ(ur->value, "remote-answer-sdp");

        if (ur->endpoint.has_value())
        {
            EXPECT_EQ(*ur->endpoint, "webrtc");
        }
    }

    TEST_F(SbmdCameraWebrtcTest, HandleIncomingAnswerStoresWebRTCSessionID)
    {
        std::string sessions = R"({"s1":{"state":"streaming","protocol":"webrtc"}})";
        auto tlv = EncodeTlv("{webRTCSessionID:{tag:0,type:'uint16'}, sdp:{tag:1,type:'string'}}",
                             "{webRTCSessionID: 77, sdp: 'answer-sdp'}");

        auto result =
            InvokeCommandHandler("handleIncomingAnswer", CL_WEBRTC_TRANSPORT_REQUESTOR, CMD_ANSWER, tlv, sessions);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        auto *td = FindTransientData(*result, "sessions");
        ASSERT_NE(td, nullptr) << "Expected SetTransientData for sessions";
        EXPECT_TRUE(td->value.find("\"webRTCSessionID\":77") != std::string::npos)
            << "Sessions must store camera-allocated webRTCSessionID. Got: " << td->value;
    }

    TEST_F(SbmdCameraWebrtcTest, HandleIncomingIceCandidatesUpdatesRemoteCandidates)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        auto tlv = EncodeTlv("{webRTCSessionID:{tag:0,type:'uint16'}, ICECandidates:{tag:1,type:'array'}}",
                             "{webRTCSessionID: 42, ICECandidates: [{0:'candidate:1 udp host', 1:null, 2:null}]}");

        auto result = InvokeCommandHandler(
            "handleIncomingIceCandidates", CL_WEBRTC_TRANSPORT_REQUESTOR, CMD_ICE_CANDIDATES, tlv, sessions);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        auto *ur = FindUpdateResource(*result, "remoteIceCandidates");
        ASSERT_NE(ur, nullptr) << "Expected updateResource for remoteIceCandidates";

        if (ur->endpoint.has_value())
        {
            EXPECT_EQ(*ur->endpoint, "webrtc");
        }
    }

    TEST_F(SbmdCameraWebrtcTest, HandleIncomingEndEmitsWebrtcErrorEnded)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        auto tlv = EncodeTlv("{webRTCSessionID:{tag:0,type:'uint16'}, reason:{tag:1,type:'enum8'}}",
                             "{webRTCSessionID: 42, reason: 2}");

        auto result = InvokeCommandHandler("handleIncomingEnd", CL_WEBRTC_TRANSPORT_REQUESTOR, CMD_END, tlv, sessions);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        auto *ur = FindUpdateResource(*result, "webrtcError");
        ASSERT_NE(ur, nullptr) << "Expected updateResource for webrtcError";
        EXPECT_EQ(ur->value, "ended");

        if (ur->endpoint.has_value())
        {
            EXPECT_EQ(*ur->endpoint, "webrtc");
        }

        ASSERT_TRUE(ur->metadata.has_value());
        EXPECT_TRUE(ur->metadata->find("sessionId") != std::string::npos);
        EXPECT_TRUE(ur->metadata->find("reason") != std::string::npos);
    }

    TEST_F(SbmdCameraWebrtcTest, ExecuteStreamReturnsProtocolAndEntryPoint)
    {
        std::string sessions = R"({"1":{"state":"created","protocol":"webrtc"}})";
        auto result = InvokeExecuteHandler("camera", "stream", "1", sessions);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        // The stream execute returns { protocol, entryPoint } rather than emitting a status event.
        const auto &value = std::get<ResultTerminal::Success>(result->terminal.data).value;
        EXPECT_TRUE(value.find("\"protocol\":\"webrtc\"") != std::string::npos) << "Got: " << value;
        EXPECT_TRUE(value.find("/ep/webrtc/r/localSdp") != std::string::npos) << "Got: " << value;

        // It must not emit a sessionStatus event (the abstract endpoint has no such resource).
        EXPECT_EQ(FindUpdateResource(*result, "sessionStatus"), nullptr);
    }

    TEST_F(SbmdCameraWebrtcTest, HandleVideoStreamAllocateErrorEmitsWebrtcErrorFailed)
    {
        // Drive the offer flow far enough to capture the VideoStreamAllocate requestCommand, then
        // invoke its onError continuation (handleVideoStreamAllocateError) directly.
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto offer = InvokeExecuteHandler("webrtc", "localSdp", "dummy-sdp", sessions, "", {}, OFFERER_ACCEPTED_CMDS);
        auto &alloc = ExpectRequestCommand(offer, CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE);

        auto result = InvokeCallback(
            alloc.onError,
            "({error:{type:'commandError',message:'DYNAMIC_CONSTRAINT_ERROR'}, handlerContext:{sessionId:'1'}})");

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        auto *ur = FindUpdateResource(*result, "webrtcError");
        ASSERT_NE(ur, nullptr) << "Expected updateResource for webrtcError";
        EXPECT_EQ(ur->value, "failed");

        if (ur->endpoint.has_value())
        {
            EXPECT_EQ(*ur->endpoint, "webrtc");
        }

        ASSERT_TRUE(ur->metadata.has_value());
        EXPECT_TRUE(ur->metadata->find("DYNAMIC_CONSTRAINT_ERROR") != std::string::npos);
    }

    TEST_F(SbmdCameraWebrtcTest, HandleProvideOfferErrorEmitsWebrtcErrorFailed)
    {
        // Drive the offer flow through the allocate response to capture the ProvideOffer
        // requestCommand, then invoke its onError (handleProvideOfferError). A requestCommand
        // deadline is reported through onError with type 'timeout', so this also covers the
        // offer-flow timeout path.
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc"}})";
        auto offer = InvokeExecuteHandler("webrtc", "localSdp", "dummy-sdp", sessions, "", {}, OFFERER_ACCEPTED_CMDS);
        auto &alloc = ExpectRequestCommand(offer, CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE);

        auto allocRespTlv = EncodeTlv("{videoStreamID:{tag:0,type:'uint16'}}", "{videoStreamID: 5}");
        std::string allocRespArgs = "({response:{data:'" + allocRespTlv +
                                    "'}, handlerContext:{sdp:'dummy-sdp', sessionId:'1', "
                                    "sessions:{'1':{state:'streaming',protocol:'webrtc'}}}})";
        auto provide = InvokeCallback(alloc.onResponse, allocRespArgs);
        auto &po = ExpectRequestCommand(provide, CL_WEBRTC_TRANSPORT_PROVIDER, CMD_PROVIDE_OFFER);

        auto result = InvokeCallback(
            po.onError,
            "({error:{type:'timeout',message:'Overall operation deadline exceeded'}, handlerContext:{sessionId:'1'}})");

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        auto *ur = FindUpdateResource(*result, "webrtcError");
        ASSERT_NE(ur, nullptr) << "Expected updateResource for webrtcError";
        EXPECT_EQ(ur->value, "failed");

        ASSERT_TRUE(ur->metadata.has_value());
        EXPECT_TRUE(ur->metadata->find("timeout") != std::string::npos);
    }

    // ========================================================================
    // 5.4 — executeDestroySession sends EndSession when streaming
    // ========================================================================

    TEST_F(SbmdCameraWebrtcTest, DestroySessionStreamingSendsEndSession)
    {
        std::string sessions = R"({"1":{"state":"streaming","protocol":"webrtc","webRTCSessionID":42}})";
        auto result = InvokeExecuteHandler("camera", "destroySession", "1", sessions);
        ExpectSendCommand(result, CL_WEBRTC_TRANSPORT_PROVIDER, CMD_END_SESSION);

        ASSERT_GE(result->ops.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<ResultOp::SetTransientData>(result->ops[0].data));
    }

    TEST_F(SbmdCameraWebrtcTest, DestroySessionCreatedStateDoesNotSendEndSession)
    {
        std::string sessions = R"({"1":{"state":"created","protocol":"webrtc"}})";
        auto result = InvokeExecuteHandler("camera", "destroySession", "1", sessions);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        // No sendCommand ops expected for a non-streaming session
        // Just SetTransientData
        for (const auto &op : result->ops)
        {
            EXPECT_FALSE(std::holds_alternative<ResultOp::UpdateResource>(op.data))
                << "Non-streaming destroy should not update any resources";
        }
    }

} // namespace
