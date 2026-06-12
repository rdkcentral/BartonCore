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
 * Unit tests for SbmdV4Driver activate/deactivate lifecycle.
 */

#include "deviceDrivers/matter/sbmd/SbmdV4Driver.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdUtilsLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdV4Loader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdV4ResultExecutor.h"

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
            driverVersion: "1.0",
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
            attributeHandlers: [
                {
                    name: "onOffHandler",
                    aliases: ["onOff"],
                    handler: handleOnOff,
                },
            ],
        });

        function readIsOn(args) {
            return SbmdUtils.result().success();
        }

        function writeIsOn(args) {
            return SbmdUtils.result()
                .device.sendCommand(CL_ON_OFF, CMD_ON);
        }

        function handleOnOff(args) {
            return SbmdUtils.result()
                .dataModel.updateResource("1", "isOn", "true")
                .success();
        }
    )";

    class SbmdV4DriverTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdV4Loader::InjectCaptureFunction(ctx));
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
        std::unique_ptr<SbmdV4Driver> CreateDriver(const std::string &source = kDriverSource)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto reg = SbmdV4Loader::LoadDriver(Ctx(), "<test>", source.c_str(), source.size());

            if (!reg)
            {
                return nullptr;
            }

            return std::make_unique<SbmdV4Driver>(std::move(reg), source);
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

            return SbmdV4ResultExecutor::Parse(ctx, result);
        }
    };

    // ========================================================================
    // Initial state (metadata-only)
    // ========================================================================

    TEST_F(SbmdV4DriverTest, InitiallyNotActivated)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);
        EXPECT_FALSE(driver->IsActivated());
    }

    TEST_F(SbmdV4DriverTest, MetadataAvailableBeforeActivation)
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

    TEST_F(SbmdV4DriverTest, ActivateSetsActivatedFlag)
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

    TEST_F(SbmdV4DriverTest, MetadataPreservedAfterActivation)
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
        EXPECT_EQ(reg.driverVersion, "1.0");

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdV4DriverTest, HandlersCallableAfterActivation)
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

    TEST_F(SbmdV4DriverTest, AttributeHandlerCallableAfterActivation)
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

    TEST_F(SbmdV4DriverTest, DoubleActivateSucceeds)
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

    TEST_F(SbmdV4DriverTest, DeactivateClearsActivatedFlag)
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

    TEST_F(SbmdV4DriverTest, HandlersUndefinedAfterDeactivation)
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

    TEST_F(SbmdV4DriverTest, MetadataPreservedAfterDeactivation)
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

    TEST_F(SbmdV4DriverTest, DoubleDeactivateIsSafe)
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

    TEST_F(SbmdV4DriverTest, ReactivateAfterDeactivate)
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

    // ========================================================================
    // Edge cases
    // ========================================================================

    TEST_F(SbmdV4DriverTest, DriverWithNoHandlers)
    {
        const char *minimalSource = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: "1.0",
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

} // namespace
