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
 * Unit tests for SbmdDriver activate/deactivate lifecycle.
 */

#include "deviceDrivers/matter/sbmd/SbmdDriver.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h"

#include <gtest/gtest.h>
#include <string>

extern "C" {
#include <mquickjs/mquickjs.h>
}

using namespace barton;

namespace
{
    // A driver source with resource handlers and device handlers
    const char *kDriverSource = R"(
        SbmdDriver({
            schemaVersion: "4.0",
            driverVersion: 1,
            name: "TestDriver",
            constants: {
                EP: "1",
                CL_ON_OFF: 6,
                ATTR_ON_OFF: 0,
                CMD_ON: 1,
                CMD_OFF: 0,
            },
            barton: { deviceClass: "light", deviceClassVersion: 1 },
            matter: { deviceTypes: [0x0100], defaultTimeoutMs: 5000 },
            aliases: {
                onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF, type: "bool" },
            },
            endpoints: {
                "1": {
                    profile: "light",
                    profileVersion: 1,
                    resources: {
                        isOn: {
                            type: "boolean",
                            modes: ["read", "write"],
                            seed: readIsOn,
                            read: readIsOn,
                            write: writeIsOn,
                        },
                    },
                },
            },
            attributeHandlers: {
                onOffHandler: {
                    aliases: ["onOff"],
                    handler: handleOnOff,
                },
            },
        });

        function readIsOn(args) {
            return Sbmd.result().success();
        }

        function writeIsOn(args) {
            return Sbmd.result()
                .device.sendCommand(CL_ON_OFF, CMD_ON);
        }

        function handleOnOff(args) {
            return Sbmd.result()
                .dataModel.updateResource("1", "isOn", "true")
                .success();
        }
    )";

    class SbmdDriverTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdLoader::InjectCaptureFunction(ctx));
        }

        static void TearDownTestSuite()
        {
            MQuickJsRuntime::Shutdown();
        }

        JSContext *Ctx()
        {
            return MQuickJsRuntime::GetSharedContext();
        }

        /**
         * Create a driver from the test source. Loads it initially to get metadata.
         */
        std::unique_ptr<SbmdDriver> CreateDriver(const std::string &source = kDriverSource)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto reg = SbmdLoader::LoadDriver(Ctx(), "<test>", source.c_str(), source.size());

            if (!reg)
            {
                return nullptr;
            }

            return std::make_unique<SbmdDriver>(std::move(reg), source);
        }

        /**
         * Call a handler function with an empty args object and parse the result.
         * Caller must hold the mutex.
         */
        std::optional<ParsedResult> CallHandler(JSValue handler)
        {
            auto *ctx = Ctx();

            // Create empty args object: ({})
            JSValue args = JS_Eval(ctx, "({})", 4, "<test>", JS_EVAL_RETVAL);

            if (JS_IsException(args))
            {
                MQuickJsRuntime::CheckAndClearPendingException(ctx);
                return std::nullopt;
            }

            if (JS_StackCheck(ctx, 3))
            {
                return std::nullopt;
            }

            JS_PushArg(ctx, args);
            JS_PushArg(ctx, handler);
            JS_PushArg(ctx, JS_NULL);

            JSValue result = JS_Call(ctx, 1);

            if (JS_IsException(result))
            {
                MQuickJsRuntime::CheckAndClearPendingException(ctx);
                return std::nullopt;
            }

            return SbmdResultExecutor::Parse(ctx, result);
        }
    };

    // ========================================================================
    // Initial state (metadata-only)
    // ========================================================================

    TEST_F(SbmdDriverTest, InitiallyNotActivated)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);
        EXPECT_FALSE(driver->IsActivated());
    }

    TEST_F(SbmdDriverTest, MetadataAvailableBeforeActivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        auto &reg = driver->GetRegistration();
        EXPECT_EQ(reg.name, "TestDriver");
        EXPECT_EQ(reg.barton.deviceClass, "light");
        EXPECT_EQ(reg.matter.deviceTypes.size(), 1u);
        EXPECT_EQ(reg.matter.deviceTypes[0], 0x0100);
        EXPECT_EQ(driver->GetName(), "TestDriver");
    }

    // ========================================================================
    // Activation
    // ========================================================================

    TEST_F(SbmdDriverTest, ActivateSetsActivatedFlag)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, MetadataPreservedAfterActivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        EXPECT_EQ(reg.name, "TestDriver");
        EXPECT_EQ(reg.barton.deviceClass, "light");
        EXPECT_EQ(reg.schemaVersion, "4.0");
        EXPECT_EQ(reg.driverVersion, 1u);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, HandlersCallableAfterActivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();

        // The write handler should produce a sendCommand terminal
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].write->handler);
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));

            auto &cmd = std::get<ResultTerminal::SendCommand>(result->terminal.data);
            EXPECT_EQ(cmd.clusterId, 6u);  // CL_ON_OFF
            EXPECT_EQ(cmd.commandId, 1u);  // CMD_ON
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, AttributeHandlerCallableAfterActivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_EQ(reg.attributeHandlers.size(), 1u);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto result = CallHandler(reg.attributeHandlers[0].handler);
            ASSERT_TRUE(result.has_value());

            // Should have updateResource op then success terminal
            ASSERT_EQ(result->ops.size(), 1u);
            ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(result->ops[0].data));
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, DoubleActivateSucceeds)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            EXPECT_TRUE(driver->Activate(Ctx())); // Should be a no-op
        }

        EXPECT_TRUE(driver->IsActivated());

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    // ========================================================================
    // Deactivation
    // ========================================================================

    TEST_F(SbmdDriverTest, DeactivateClearsActivatedFlag)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }

        EXPECT_FALSE(driver->IsActivated());
    }

    TEST_F(SbmdDriverTest, HandlersUndefinedAfterDeactivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }

        auto &reg = driver->GetRegistration();

        // Resource handlers should be reset
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());
        EXPECT_TRUE(JS_IsUndefined(reg.endpoints[0].resources[0].write->handler));
        EXPECT_TRUE(JS_IsUndefined(reg.endpoints[0].resources[0].read->handler));
        EXPECT_TRUE(JS_IsUndefined(reg.endpoints[0].resources[0].seed->handler));

        // Device handlers should be reset
        ASSERT_EQ(reg.attributeHandlers.size(), 1u);
        EXPECT_TRUE(JS_IsUndefined(reg.attributeHandlers[0].handler));
    }

    TEST_F(SbmdDriverTest, MetadataPreservedAfterDeactivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }

        auto &reg = driver->GetRegistration();
        EXPECT_EQ(reg.name, "TestDriver");
        EXPECT_EQ(reg.barton.deviceClass, "light");
    }

    TEST_F(SbmdDriverTest, DoubleDeactivateIsSafe)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
            driver->Deactivate(Ctx()); // Should be a no-op
        }

        EXPECT_FALSE(driver->IsActivated());
    }

    // ========================================================================
    // Re-activation
    // ========================================================================

    TEST_F(SbmdDriverTest, ReactivateAfterDeactivate)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        // Handlers should work again after re-activation
        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].write->handler);
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, CommandHandlerCallableAfterActivation)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "CmdLifecycleTest",
                constants: { CL_TEST: 0xFFF10000, CMD_ECHO: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0xFFF10000] },
                aliases: {
                    echoCmd: { clusterId: CL_TEST, commandId: CMD_ECHO },
                },
                commandHandlers: {
                    handleEcho: {
                        aliases: ["echoCmd"],
                        handler: handleEchoCmd,
                    },
                },
            });
            function handleEchoCmd(args) {
                return Sbmd.result()
                    .dataModel.updateResource("1", "lastCommand", "echo")
                    .success();
            }
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_EQ(reg.commandHandlers.size(), 1u);
        EXPECT_EQ(reg.commandHandlers[0].name, "handleEcho");

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto result = CallHandler(reg.commandHandlers[0].handler);
            ASSERT_TRUE(result.has_value());
            ASSERT_EQ(result->ops.size(), 1u);
            ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(result->ops[0].data));
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, CommandHandlersUndefinedAfterDeactivation)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "CmdDeactivateTest",
                constants: { CL_TEST: 0xFFF10000, CMD_ECHO: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0xFFF10000] },
                aliases: { echoCmd: { clusterId: CL_TEST, commandId: CMD_ECHO } },
                commandHandlers: {
                    handleEcho: { aliases: ["echoCmd"], handler: fn },
                },
            });
            function fn(args) { return Sbmd.result().success(); }
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }

        auto &reg = driver->GetRegistration();
        ASSERT_EQ(reg.commandHandlers.size(), 1u);
        EXPECT_TRUE(JS_IsUndefined(reg.commandHandlers[0].handler));
    }

    // ========================================================================
    // Edge cases
    // ========================================================================

    TEST_F(SbmdDriverTest, DriverWithNoHandlers)
    {
        const char *minimalSource = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "Minimal",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
            });
        )";

        auto driver = CreateDriver(minimalSource);
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }

        EXPECT_FALSE(driver->IsActivated());
    }

    // ========================================================================
    // Timeout configuration
    // ========================================================================

    TEST_F(SbmdDriverTest, DefaultTimeoutMsParsedFromMatterBlock)
    {
        // kDriverSource has defaultTimeoutMs: 5000 in the matter block
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.matter.defaultTimeoutMs.has_value());
        EXPECT_EQ(reg.matter.defaultTimeoutMs.value(), 5000u);
    }

    TEST_F(SbmdDriverTest, DefaultTimeoutMsAbsentWhenNotSpecified)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "NoTimeout",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
            });
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);
        EXPECT_FALSE(driver->GetRegistration().matter.defaultTimeoutMs.has_value());
    }

    TEST_F(SbmdDriverTest, ReportingIntervalsParsed)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "WithReporting",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                reporting: { minSecs: 5, maxSecs: 600 },
            });
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);

        auto &reg = driver->GetRegistration();
        EXPECT_EQ(reg.reporting.minSecs, 5u);
        EXPECT_EQ(reg.reporting.maxSecs, 600u);
    }

    TEST_F(SbmdDriverTest, HandlerWithTimedInvokeTimeout)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "TimedInvokeTest",
                constants: { CL_DOOR_LOCK: 257, CMD_LOCK: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x000A] },
                endpoints: {
                    "1": {
                        profile: "lock",
                        profileVersion: 1,
                        resources: {
                            lockState: {
                                type: "boolean",
                                modes: ["write"],
                                write: writeLock,
                            },
                        },
                    },
                },
            });
            function writeLock(args) {
                return Sbmd.result()
                    .device.sendCommand(CL_DOOR_LOCK, CMD_LOCK, null, {timedInvokeTimeoutMs: 10000});
            }
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].write->handler);
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));

            auto &cmd = std::get<ResultTerminal::SendCommand>(result->terminal.data);
            EXPECT_EQ(cmd.clusterId, 257u);
            EXPECT_EQ(cmd.commandId, 0u);
            ASSERT_TRUE(cmd.timedInvokeTimeoutMs.has_value());
            EXPECT_EQ(*cmd.timedInvokeTimeoutMs, 10000u);
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, HandlerWithDeferredTimeoutAndTimedInvoke)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "DeferredTimeoutTest",
                constants: { CL_DOOR_LOCK: 257, CMD_GET_USER: 0x1C, CMD_GET_USER_RESP: 0x1D },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x000A] },
                endpoints: {
                    "1": {
                        profile: "lock",
                        profileVersion: 1,
                        resources: {
                            users: {
                                type: "string",
                                modes: ["read"],
                                read: readUsers,
                            },
                        },
                    },
                },
            });
            function readUsers(args) {
                return Sbmd.result()
                    .device.requestCommand(CL_DOOR_LOCK, CMD_GET_USER, null, {
                        responseCommandId: CMD_GET_USER_RESP,
                        onResponse: function(a) { return Sbmd.result().success('data'); },
                        onError: function(a) { return Sbmd.result().error(a.error.type); },
                        timeoutMs: 5000,
                        timedInvokeTimeoutMs: 10000,
                    });
            }
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].read.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].read->handler);
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::RequestCommand>(result->terminal.data));

            auto &rc = std::get<ResultTerminal::RequestCommand>(result->terminal.data);
            EXPECT_EQ(rc.clusterId, 257u);
            EXPECT_EQ(rc.commandId, 0x1Cu);
            EXPECT_EQ(rc.responseCommandId, 0x1Du);
            ASSERT_TRUE(rc.timeoutMs.has_value());
            EXPECT_EQ(*rc.timeoutMs, 5000u);
            ASSERT_TRUE(rc.timedInvokeTimeoutMs.has_value());
            EXPECT_EQ(*rc.timedInvokeTimeoutMs, 10000u);
            EXPECT_FALSE(JS_IsUndefined(rc.onResponse));
            EXPECT_FALSE(JS_IsUndefined(rc.onError));
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, HandlerWithDeferredReadAttributeTimeout)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "DeferredReadTest",
                constants: { CL_COLOR: 0x0300, ATTR_HUE: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                endpoints: {
                    "1": {
                        profile: "light",
                        profileVersion: 1,
                        resources: {
                            hue: {
                                type: "number",
                                modes: ["read"],
                                read: readHue,
                            },
                        },
                    },
                },
            });
            function readHue(args) {
                return Sbmd.result()
                    .device.readAttribute(CL_COLOR, ATTR_HUE, {
                        onResponse: function(a) { return Sbmd.result().success(String(a.attribute.value)); },
                        onError: function(a) { return Sbmd.result().error(a.error.message); },
                        timeoutMs: 3000,
                    });
            }
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].read.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].read->handler);
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::ReadAttribute>(result->terminal.data));

            auto &ra = std::get<ResultTerminal::ReadAttribute>(result->terminal.data);
            EXPECT_EQ(ra.clusterId, 0x0300u);
            EXPECT_EQ(ra.attributeId, 0u);
            ASSERT_TRUE(ra.timeoutMs.has_value());
            EXPECT_EQ(*ra.timeoutMs, 3000u);
            EXPECT_FALSE(JS_IsUndefined(ra.onResponse));
            EXPECT_FALSE(JS_IsUndefined(ra.onError));
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

} // namespace
