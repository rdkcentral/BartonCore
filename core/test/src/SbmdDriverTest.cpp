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

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <sys/stat.h>
#include <vector>

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
            ASSERT_TRUE(MQuickJsRuntime::Instance().Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::Instance().GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdLoader::InjectCaptureFunction(ctx));
        }

        static void TearDownTestSuite()
        {
            MQuickJsRuntime::Instance().Shutdown();
        }

        void TearDown() override
        {
            // Remove every path UniqueSpecPath() handed out, even when a test stopped at a failed
            // ASSERT before its own cleanup.
            for (const auto &path : specPaths)
            {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
            }
        }

        JSContext *Ctx()
        {
            return MQuickJsRuntime::Instance().GetSharedContext();
        }

        /**
         * A unique spec-file path under the temp directory, removed (with anything under it) after
         * the test. Per-process salt plus a per-call counter keep concurrent test runs on the same
         * host from colliding on fixed filenames.
         */
        std::filesystem::path UniqueSpecPath(const std::string &tag)
        {
            static const std::string salt = [] {
                std::random_device rd;
                return std::to_string(rd()) + "_" + std::to_string(rd());
            }();
            static std::atomic<unsigned> counter {0};

            auto path = std::filesystem::temp_directory_path() /
                        ("sbmd_" + tag + "_" + salt + "_" + std::to_string(counter.fetch_add(1)) + ".sbmd.js");
            specPaths.push_back(path);

            return path;
        }

        /**
         * Load kDriverSource from a file, reduce the driver, replace the file with kDriverSource
         * edited by replacing `from` with `to`, and try to activate it.
         */
        SbmdDriver::ActivationFailure ActivateAfterEdit(const std::string &from, const std::string &to)
        {
            auto path = UniqueSpecPath("edited");
            auto driver = CreateDriverFromFile(kDriverSource, path.string());

            if (!driver)
            {
                ADD_FAILURE() << "could not load the original spec";
                return SbmdDriver::ActivationFailure::None;
            }

            std::string edited = kDriverSource;
            auto pos = edited.find(from);

            if (pos == std::string::npos)
            {
                ADD_FAILURE() << "'" << from << "' not found in the spec";
                return SbmdDriver::ActivationFailure::None;
            }

            edited.replace(pos, from.size(), to);

            {
                std::ofstream out(path, std::ios::binary | std::ios::trunc);
                out.write(edited.data(), static_cast<std::streamsize>(edited.size()));
            }

            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();

            SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
            const bool activated = driver->Activate(Ctx(), &failure);
            EXPECT_EQ(activated, failure == SbmdDriver::ActivationFailure::None);

            if (activated)
            {
                driver->Deactivate(Ctx());
            }

            return failure;
        }

        std::vector<std::filesystem::path> specPaths;

        /**
         * Create a driver from the test source. Loads it initially to get metadata.
         */
        std::unique_ptr<SbmdDriver> CreateDriver(const std::string &source = kDriverSource)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto reg = SbmdLoader::LoadDriver(Ctx(), "<test>", source.c_str(), source.size());

            if (!reg)
            {
                return nullptr;
            }

            return std::make_unique<SbmdDriver>(std::move(reg), source);
        }

        /**
         * Write source to a file on disk and create a driver whose filePath is that file, so a
         * driver reduced to its claim metadata can re-read its spec from disk on activation.
         */
        std::unique_ptr<SbmdDriver> CreateDriverFromFile(const std::string &source, const std::string &path)
        {
            {
                std::ofstream out(path, std::ios::binary | std::ios::trunc);
                out.write(source.data(), static_cast<std::streamsize>(source.size()));
            }

            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto reg = SbmdLoader::LoadDriver(Ctx(), path, source.c_str(), source.size());

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
        std::optional<ParsedResult> CallHandler(JSValue handlerRaw)
        {
            auto *ctx = Ctx();

            // Root the handler and args: mquickjs's moving GC relocates objects on each allocation
            // (JS_Eval, JS_StackCheck), so raw JSValues held across those calls go stale.
            SafeJSValue handler(ctx, handlerRaw);

            // Create empty args object: ({})
            SafeJSValue args(ctx, JS_Eval(ctx, "({})", 4, "<test>", JS_EVAL_RETVAL));

            if (JS_IsException(args.Get()))
            {
                MQuickJsRuntime::Instance().CheckAndClearPendingException(ctx);
                return std::nullopt;
            }

            if (JS_StackCheck(ctx, 3))
            {
                return std::nullopt;
            }

            JS_PushArg(ctx, args.Get());
            JS_PushArg(ctx, handler.Get());
            JS_PushArg(ctx, JS_NULL);

            JSValue result = JS_Call(ctx, 1);

            if (JS_IsException(result))
            {
                MQuickJsRuntime::Instance().CheckAndClearPendingException(ctx);
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, MetadataPreservedAfterActivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        EXPECT_EQ(reg.name, "TestDriver");
        EXPECT_EQ(reg.barton.deviceClass, "light");
        EXPECT_EQ(reg.schemaVersion, "4.0");
        EXPECT_EQ(reg.driverVersion, 1u);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, HandlersCallableAfterActivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();

        // The write handler should produce a sendCommand terminal
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].write->Fn());
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));

            auto &cmd = std::get<ResultTerminal::SendCommand>(result->terminal.data);
            EXPECT_EQ(cmd.clusterId, 6u);  // CL_ON_OFF
            EXPECT_EQ(cmd.commandId, 1u);  // CMD_ON
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, AttributeHandlerCallableAfterActivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_EQ(reg.attributeHandlers.size(), 1u);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto result = CallHandler(reg.attributeHandlers[0].Fn());
            ASSERT_TRUE(result.has_value());

            // Should have updateResource op then success terminal
            ASSERT_EQ(result->ops.size(), 1u);
            ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(result->ops[0].data));
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, DoubleActivateSucceeds)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            EXPECT_TRUE(driver->Activate(Ctx())); // Should be a no-op
        }

        EXPECT_TRUE(driver->IsActivated());

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }

        EXPECT_FALSE(driver->IsActivated());
    }

    TEST_F(SbmdDriverTest, HandlersReleasedAfterDeactivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }

        auto &reg = driver->GetRegistration();

        // Deactivation sheds the heavy parsed registration back to the claim metadata.
        EXPECT_TRUE(reg.endpoints.empty());
        EXPECT_TRUE(reg.aliases.empty());
        EXPECT_TRUE(reg.attributeHandlers.empty());
        EXPECT_TRUE(reg.eventHandlers.empty());
        EXPECT_TRUE(reg.commandHandlers.empty());
    }

    TEST_F(SbmdDriverTest, MetadataPreservedAfterDeactivation)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
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
        // Deactivation releases the source, so re-activation re-reads the spec from disk.
        auto path = UniqueSpecPath("reactivate");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        // Handlers should work again after re-activation
        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].write->Fn());
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }

        std::filesystem::remove(path);
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
                matter: { deviceTypes: [0x0100] },
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_EQ(reg.commandHandlers.size(), 1u);
        EXPECT_EQ(reg.commandHandlers[0].name, "handleEcho");

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto result = CallHandler(reg.commandHandlers[0].Fn());
            ASSERT_TRUE(result.has_value());
            ASSERT_EQ(result->ops.size(), 1u);
            ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(result->ops[0].data));
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, CommandHandlersReleasedAfterDeactivation)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "CmdDeactivateTest",
                constants: { CL_TEST: 0xFFF10000, CMD_ECHO: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }

        auto &reg = driver->GetRegistration();
        EXPECT_TRUE(reg.commandHandlers.empty());
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }

        EXPECT_FALSE(driver->IsActivated());
    }

    // ========================================================================
    // Numeric claim/version field range validation
    //
    // These fields are stored narrower than the JS number they are read from, so a value that
    // wraps, narrows, truncates, or is coerced could alias an in-range value and slip a changed
    // claim identity or version past re-read validation. The loader must reject anything that is not
    // an integer within the field's range at parse time (so a tampered re-read fails activation, and
    // the bind is rejected).
    // ========================================================================

    TEST_F(SbmdDriverTest, ProfileVersionAboveUint8Rejected)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "BadProfileVersion",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                endpoints: { "1": { profile: "test", profileVersion: 257, resources: {} } },
            });
        )";

        EXPECT_EQ(CreateDriver(source), nullptr);
    }

    TEST_F(SbmdDriverTest, ProfileVersionWraparoundRejected)
    {
        // 4294967297 == 2^32 + 1 wraps to 1 under JS_ToUint32; the saturating range check must catch
        // it before it aliases a valid cached version.
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "WrappingProfileVersion",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                endpoints: { "1": { profile: "test", profileVersion: 4294967297, resources: {} } },
            });
        )";

        EXPECT_EQ(CreateDriver(source), nullptr);
    }

    TEST_F(SbmdDriverTest, DeviceTypeAboveUint16Rejected)
    {
        // 0x10100 narrows to 0x0100 (a valid device type); the range check must reject it.
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "BadDeviceType",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x10100] },
            });
        )";

        EXPECT_EQ(CreateDriver(source), nullptr);
    }

    TEST_F(SbmdDriverTest, VendorIdAboveUint16Rejected)
    {
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "BadVendorId",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100], vendorId: 0x10000, productId: 0x1 },
            });
        )";

        EXPECT_EQ(CreateDriver(source), nullptr);
    }

    TEST_F(SbmdDriverTest, DeviceClassVersionAboveUint8Rejected)
    {
        // deviceClassVersion is published as uint8; 256 narrows to 0 and would alias, so reject it.
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "BadDeviceClassVersion",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 256 },
                matter: { deviceTypes: [0x0100] },
            });
        )";

        EXPECT_EQ(CreateDriver(source), nullptr);
    }

    TEST_F(SbmdDriverTest, Uint16AndUint8BoundaryValuesAccepted)
    {
        // Exactly at the limits: these must load so the range check does not reject valid specs.
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "BoundaryValues",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 255 },
                matter: { deviceTypes: [0xFFFF], vendorId: 0xFFFF, productId: 0xFFFF },
                endpoints: { "1": { profile: "test", profileVersion: 255, resources: {} } },
            });
        )";

        auto driver = CreateDriver(source);
        ASSERT_NE(driver, nullptr);
        EXPECT_EQ(driver->GetRegistration().matter.deviceTypes.front(), 0xFFFF);
    }

    /**
     * Build a spec whose numeric claim/version fields are the given JS expressions.
     */
    std::string NumericFieldSpec(const std::string &deviceType,
                                 const std::string &vendorId,
                                 const std::string &deviceClassVersion,
                                 const std::string &profileVersion)
    {
        return R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "NumericFields",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: )" +
               deviceClassVersion + R"( },
                matter: { deviceTypes: [)" +
               deviceType + R"(], vendorId: )" + vendorId + R"(, productId: 0x1 },
                endpoints: { "1": { profile: "test", profileVersion: )" +
               profileVersion + R"(, resources: {} } },
            });
        )";
    }

    TEST_F(SbmdDriverTest, NonIntegerNumericFieldsRejectedAsInvalidField)
    {
        // Each value would otherwise be truncated or coerced into an in-range integer (2.5 -> 2,
        // "7" -> 7, true -> 1, null/NaN -> 0) and alias a cached claim identity or version.
        const std::vector<std::string> badValues = {
            "2.5", "-0.5", "0.5", "NaN", "Infinity", "-Infinity", "\"1\"", "true", "null", "[1]", "({})"};

        for (const auto &bad : badValues)
        {
            const std::vector<std::pair<const char *, std::string>> cases = {
                {       "deviceTypes",      NumericFieldSpec(bad, "0x1", "1", "1")},
                {          "vendorId", NumericFieldSpec("0x0100",   bad, "1", "1")},
                {"deviceClassVersion", NumericFieldSpec("0x0100", "0x1", bad, "1")},
                {    "profileVersion", NumericFieldSpec("0x0100", "0x1", "1", bad)},
            };

            for (const auto &[field, source] : cases)
            {
                std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
                SbmdLoadFailure failure = SbmdLoadFailure::None;
                auto reg = SbmdLoader::LoadDriver(Ctx(), "<numeric>", source.c_str(), source.size(), &failure);

                EXPECT_EQ(reg, nullptr) << field << " = " << bad << " must be rejected";
                EXPECT_EQ(failure, SbmdLoadFailure::InvalidField) << field << " = " << bad;
            }
        }
    }

    TEST_F(SbmdDriverTest, NonArrayDeviceTypesRejectedAsInvalidField)
    {
        // deviceTypes is an integer array; any other present value (including null) is invalid
        // rather than silently treated as "no device types".
        for (const std::string bad : {"null", "0x0100", "\"0x0100\"", "({})"})
        {
            const std::string source = R"(
                SbmdDriver({
                    schemaVersion: "4.0",
                    driverVersion: 1,
                    name: "NonArrayDeviceTypes",
                    constants: {},
                    barton: { deviceClass: "test", deviceClassVersion: 1 },
                    matter: { deviceTypes: )" +
                                       bad + R"( },
                });
            )";

            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            SbmdLoadFailure failure = SbmdLoadFailure::None;

            EXPECT_EQ(SbmdLoader::LoadDriver(Ctx(), "<device-types>", source.c_str(), source.size(), &failure), nullptr)
                << "deviceTypes = " << bad << " must be rejected";
            EXPECT_EQ(failure, SbmdLoadFailure::InvalidField) << "deviceTypes = " << bad;
        }
    }

    TEST_F(SbmdDriverTest, IntegralNumericFieldsAccepted)
    {
        // Negative zero is the integer 0; an integral double such as 2.0 is the integer 2.
        auto source = NumericFieldSpec("0x0100", "0x1", "2.0", "-0");

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
        SbmdLoadFailure failure = SbmdLoadFailure::Other;
        auto reg = SbmdLoader::LoadDriver(Ctx(), "<numeric>", source.c_str(), source.size(), &failure);

        ASSERT_NE(reg, nullptr);
        EXPECT_EQ(failure, SbmdLoadFailure::None);
        EXPECT_EQ(reg->barton.deviceClassVersion, 2u);
        ASSERT_EQ(reg->endpoints.size(), 1u);
        EXPECT_EQ(reg->endpoints[0].profileVersion, 0u);
    }

    TEST_F(SbmdDriverTest, LoadFailureDistinguishesInvalidFieldFromOtherErrors)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());

        auto outOfRange = NumericFieldSpec("0x10100", "0x1", "1", "1");
        SbmdLoadFailure failure = SbmdLoadFailure::None;
        EXPECT_EQ(SbmdLoader::LoadDriver(Ctx(), "<range>", outOfRange.c_str(), outOfRange.size(), &failure), nullptr);
        EXPECT_EQ(failure, SbmdLoadFailure::InvalidField);

        const std::string syntaxError = "SbmdDriver({ name: ";
        failure = SbmdLoadFailure::None;
        EXPECT_EQ(SbmdLoader::LoadDriver(Ctx(), "<syntax>", syntaxError.c_str(), syntaxError.size(), &failure),
                  nullptr);
        EXPECT_EQ(failure, SbmdLoadFailure::Other);
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].write->Fn());
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));

            auto &cmd = std::get<ResultTerminal::SendCommand>(result->terminal.data);
            EXPECT_EQ(cmd.clusterId, 257u);
            EXPECT_EQ(cmd.commandId, 0u);
            ASSERT_TRUE(cmd.timedInvokeTimeoutMs.has_value());
            EXPECT_EQ(*cmd.timedInvokeTimeoutMs, 10000u);
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].read.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].read->Fn());
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto &reg = driver->GetRegistration();
        ASSERT_TRUE(reg.endpoints[0].resources[0].read.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto result = CallHandler(reg.endpoints[0].resources[0].read->Fn());
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
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    // ========================================================================
    // Reduce to claim metadata + activation from disk
    // ========================================================================

    TEST_F(SbmdDriverTest, ReduceToClaimMetadataReleasesRegistrationKeepsMetadata)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
        }

        EXPECT_FALSE(driver->IsActivated());

        auto &reg = driver->GetRegistration();
        // Claim metadata remains resident.
        EXPECT_EQ(reg.name, "TestDriver");
        EXPECT_EQ(reg.barton.deviceClass, "light");
        ASSERT_EQ(reg.matter.deviceTypes.size(), 1u);
        EXPECT_EQ(reg.matter.deviceTypes[0], 0x0100);
        // Heavy collections are released.
        EXPECT_TRUE(reg.endpoints.empty());
        EXPECT_TRUE(reg.aliases.empty());
        EXPECT_TRUE(reg.attributeHandlers.empty());
    }

    TEST_F(SbmdDriverTest, ActivateAfterReduceReadsFromDisk)
    {
        auto path = UniqueSpecPath("disk_reread");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata(); // frees the in-memory source; activation must re-read from disk
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        auto &reg = driver->GetRegistration();
        ASSERT_FALSE(reg.endpoints.empty());
        ASSERT_TRUE(reg.endpoints[0].resources[0].write.has_value());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }

        std::filesystem::remove(path);
    }

    TEST_F(SbmdDriverTest, ActivateRereadsRelativeSpecPathAfterWorkingDirectoryChanges)
    {
        // A spec loaded through a relative sbmd_dirs entry must still be found at activation after
        // the process's working directory has changed.
        auto absolutePath = UniqueSpecPath("relative");
        const auto originalDir = std::filesystem::current_path();

        struct RestoreWorkingDirectory
        {
            std::filesystem::path dir;

            ~RestoreWorkingDirectory() { std::filesystem::current_path(dir); }
        } restore {originalDir};

        std::filesystem::current_path(absolutePath.parent_path());
        const auto expectedPath = std::filesystem::current_path() / absolutePath.filename();
        auto driver = CreateDriverFromFile(kDriverSource, absolutePath.filename().string());
        ASSERT_NE(driver, nullptr);
        EXPECT_EQ(driver->GetRegistration().filePath, expectedPath.string());

        std::filesystem::current_path(originalDir);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_TRUE(driver->IsActivated());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->Deactivate(Ctx());
        }

        std::filesystem::remove(absolutePath);
    }

    TEST_F(SbmdDriverTest, ActivateRereadsSpecPathThatPassesThroughSymlinkedParent)
    {
        // "linkdir/../spec.sbmd.js" where linkdir is a symlink names target/spec.sbmd.js, not
        // base/spec.sbmd.js. The recorded path must keep naming the file the factory loaded, so it
        // must not be normalized textually.
        auto base = UniqueSpecPath("symlink_base");
        base.replace_extension();
        base.replace_extension();
        const auto target = base / "target";
        std::filesystem::create_directories(target / "inner");
        std::filesystem::create_directory_symlink(target / "inner", base / "linkdir");

        struct RemoveTree
        {
            std::filesystem::path dir;

            ~RemoveTree() { std::filesystem::remove_all(dir); }
        } cleanup {base};

        const auto specPath = base / "linkdir" / ".." / "spec.sbmd.js";
        auto driver = CreateDriverFromFile(kDriverSource, specPath.string());
        ASSERT_NE(driver, nullptr);
        ASSERT_TRUE(std::filesystem::exists(target / "spec.sbmd.js"));
        EXPECT_EQ(driver->GetRegistration().filePath, specPath.string());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
            EXPECT_TRUE(driver->Activate(Ctx()));
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, LoadForActivationFailsWithoutSpecText)
    {
        // When the caller's read (done before taking the JS mutex) failed, loading must report
        // ReadFailed instead of reading the file itself while the caller holds the mutex.
        auto path = UniqueSpecPath("pre_read_failed");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
        driver->ReduceToClaimMetadata();

        SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
        EXPECT_EQ(driver->LoadForActivation(Ctx(), nullptr, &failure), nullptr);
        EXPECT_EQ(failure, SbmdDriver::ActivationFailure::ReadFailed);

        // The file itself is readable: given its contents, loading succeeds.
        std::string specText;
        ASSERT_TRUE(SbmdDriver::ReadSpecFile(driver->GetSpecPath(), specText));
        auto fresh = driver->LoadForActivation(Ctx(), &specText, &failure);
        EXPECT_NE(fresh, nullptr);
        SbmdDriver::DiscardRegistration(std::move(fresh), Ctx());

        std::filesystem::remove(path);
    }

    TEST_F(SbmdDriverTest, ActivateFailsWhenSourceUnavailable)
    {
        // A driver reduced to its claim metadata whose spec file does not exist on disk cannot activate.
        auto driver = CreateDriver(); // filePath "<test>", no file on disk
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
            SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
            EXPECT_FALSE(driver->Activate(Ctx(), &failure));
            EXPECT_EQ(failure, SbmdDriver::ActivationFailure::ReadFailed);
        }

        EXPECT_FALSE(driver->IsActivated());
    }

    TEST_F(SbmdDriverTest, ActivateRejectsMismatchedSpec)
    {
        auto path = UniqueSpecPath("mismatch");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
        }

        // Overwrite the spec on disk with a different claim identity (device type changed).
        const std::string changed = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "TestDriver",
                constants: { CL_ON_OFF: 6, ATTR_ON_OFF: 0 },
                barton: { deviceClass: "light", deviceClassVersion: 1 },
                matter: { deviceTypes: [0x0101] },
                aliases: { onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF, type: "bool" } },
                endpoints: { "1": { profile: "light", profileVersion: 1, resources: {
                    isOn: { type: "boolean", modes: ["read"], read: readIsOn } } } },
            });
            function readIsOn(args) { return Sbmd.result().success(); }
        )";
        {
            std::ofstream out(path.string(), std::ios::binary | std::ios::trunc);
            out.write(changed.data(), static_cast<std::streamsize>(changed.size()));
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
            EXPECT_FALSE(driver->Activate(Ctx(), &failure));
            EXPECT_EQ(failure, SbmdDriver::ActivationFailure::ClaimMismatch);
        }

        EXPECT_FALSE(driver->IsActivated());

        std::filesystem::remove(path);
    }

    TEST_F(SbmdDriverTest, ActivateRejectsChangedDeviceClassVersion)
    {
        EXPECT_EQ(ActivateAfterEdit("deviceClassVersion: 1", "deviceClassVersion: 2"),
                  SbmdDriver::ActivationFailure::VersionMismatch);
    }

    TEST_F(SbmdDriverTest, ActivateRejectsChangedProfileVersion)
    {
        EXPECT_EQ(ActivateAfterEdit("profileVersion: 1", "profileVersion: 2"),
                  SbmdDriver::ActivationFailure::VersionMismatch);
    }

    TEST_F(SbmdDriverTest, ActivateRejectsChangedProfileSet)
    {
        // Same number of profiles, different name.
        EXPECT_EQ(ActivateAfterEdit(R"(profile: "light")", R"(profile: "lamp")"),
                  SbmdDriver::ActivationFailure::VersionMismatch);

        // A profile added.
        EXPECT_EQ(ActivateAfterEdit(R"(endpoints: {)",
                                    R"(endpoints: { "2": { profile: "sensor", profileVersion: 1, resources: {} },)"),
                  SbmdDriver::ActivationFailure::VersionMismatch);
    }

    TEST_F(SbmdDriverTest, ActivateAcceptsEditsThatKeepVersions)
    {
        // A second endpoint, after the first, repeating its profile at another version: only the
        // first endpoint's version is published per profile, so the snapshot is unchanged.
        const std::string endOfEndpoints = "\n            },\n            attributeHandlers: {";
        const std::string secondEndpoint = R"(
                "2": { profile: "light", profileVersion: 7, resources: {} },)";
        EXPECT_EQ(ActivateAfterEdit(endOfEndpoints, secondEndpoint + endOfEndpoints),
                  SbmdDriver::ActivationFailure::None);

        // A handler change.
        EXPECT_EQ(ActivateAfterEdit(R"(.dataModel.updateResource("1", "isOn", "true"))",
                                    R"(.dataModel.updateResource("1", "isOn", "false"))"),
                  SbmdDriver::ActivationFailure::None);
    }

    TEST_F(SbmdDriverTest, ActivateAcceptsReorderedProfiles)
    {
        // The version check matches profiles by name, so declaring the same profiles at the same
        // versions in another order is not a change.
        const std::string original = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "TwoProfileDriver",
                constants: {},
                barton: { deviceClass: "light", deviceClassVersion: 1 },
                matter: { deviceTypes: [0x0100] },
                endpoints: {
                    "1": { profile: "light", profileVersion: 1, resources: {} },
                    "2": { profile: "sensor", profileVersion: 2, resources: {} },
                },
            });
        )";
        const std::string reordered = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "TwoProfileDriver",
                constants: {},
                barton: { deviceClass: "light", deviceClassVersion: 1 },
                matter: { deviceTypes: [0x0100] },
                endpoints: {
                    "1": { profile: "sensor", profileVersion: 2, resources: {} },
                    "2": { profile: "light", profileVersion: 1, resources: {} },
                },
            });
        )";

        auto path = UniqueSpecPath("reordered");
        auto driver = CreateDriverFromFile(original, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out.write(reordered.data(), static_cast<std::streamsize>(reordered.size()));
        }

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
        driver->ReduceToClaimMetadata();

        SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
        EXPECT_TRUE(driver->Activate(Ctx(), &failure));
        EXPECT_EQ(failure, SbmdDriver::ActivationFailure::None);

        if (driver->IsActivated())
        {
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDriverTest, ActivateTimesOutOnNonTerminatingSpec)
    {
        // A spec edited on disk so it never finishes evaluating must not hold the JS mutex forever:
        // activation evaluates it under the SBMD script deadline and fails.
        auto path = UniqueSpecPath("non_terminating");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
        }

        {
            const std::string looping = std::string("while (true) {}\n") + kDriverSource;
            std::ofstream out(path.string(), std::ios::binary | std::ios::trunc);
            out.write(looping.data(), static_cast<std::streamsize>(looping.size()));
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
            EXPECT_FALSE(driver->Activate(Ctx(), &failure));
            EXPECT_EQ(failure, SbmdDriver::ActivationFailure::LoadFailed);
        }

        EXPECT_FALSE(driver->IsActivated());
        std::filesystem::remove(path);
    }

    TEST_F(SbmdDriverTest, ActivateFailsFastWhenSpecIsNotARegularFile)
    {
        // Reading a FIFO would block until a writer appears; activation must refuse it instead.
        auto path = UniqueSpecPath("fifo");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
        }

        std::filesystem::remove(path);
        ASSERT_EQ(mkfifo(path.c_str(), 0600), 0);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
            EXPECT_FALSE(driver->Activate(Ctx(), &failure));
            EXPECT_EQ(failure, SbmdDriver::ActivationFailure::ReadFailed);
        }

        std::filesystem::remove(path);
    }

    TEST_F(SbmdDriverTest, ActivateFailsWhenExtractionTimesOut)
    {
        // The deadline can also fire while the loader reads the registration's properties, where an
        // interrupted read looks like an absent property. Activation must fail rather than accept a
        // partial registration, and must not leave a pending exception behind for the next load.
        auto path = UniqueSpecPath("extraction_timeout");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
        }

        {
            const std::string hanging = R"(
                SbmdDriver({
                    schemaVersion: "4.0",
                    driverVersion: 1,
                    name: "TestDriver",
                    constants: {},
                    barton: { deviceClass: "light", deviceClassVersion: 1 },
                    matter: { deviceTypes: [0x0100] },
                    get attributeHandlers() { while (true) {} },
                });
            )";
            std::ofstream out(path.string(), std::ios::binary | std::ios::trunc);
            out.write(hanging.data(), static_cast<std::streamsize>(hanging.size()));
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
            EXPECT_FALSE(driver->Activate(Ctx(), &failure));
            EXPECT_EQ(failure, SbmdDriver::ActivationFailure::LoadFailed);
        }

        EXPECT_FALSE(driver->IsActivated());
        std::filesystem::remove(path);

        // An unrelated, valid load right afterwards still succeeds.
        EXPECT_NE(CreateDriver(), nullptr);
    }

    TEST_F(SbmdDriverTest, ActivateStopsCallingSpecCodeAfterTimeout)
    {
        // The engine only polls the deadline periodically, so every further call into spec code after
        // it fires (here, each element's toString) would run for a while before being interrupted.
        // The loader must stop at the first timeout instead of paying that once per element.
        auto path = UniqueSpecPath("timeout_loop");
        auto driver = CreateDriverFromFile(kDriverSource, path.string());
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            driver->ReduceToClaimMetadata();
        }

        {
            const std::string hanging = R"(
                var hang = { toString: function () { while (true) {} } };
                var modes = [];
                for (var i = 0; i < 200; i++) { modes.push(hang); }
                SbmdDriver({
                    schemaVersion: "4.0",
                    driverVersion: 1,
                    name: "TestDriver",
                    constants: {},
                    barton: { deviceClass: "light", deviceClassVersion: 1 },
                    matter: { deviceTypes: [0x0100] },
                    endpoints: { "1": { profile: "light", profileVersion: 1, resources: {
                        isOn: { type: "boolean", modes: modes } } } },
                });
            )";
            std::ofstream out(path.string(), std::ios::binary | std::ios::trunc);
            out.write(hanging.data(), static_cast<std::streamsize>(hanging.size()));
        }

        auto start = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;
            EXPECT_FALSE(driver->Activate(Ctx(), &failure));
            EXPECT_EQ(failure, SbmdDriver::ActivationFailure::LoadFailed);
        }

        EXPECT_LT(std::chrono::steady_clock::now() - start,
                  std::chrono::milliseconds(2 * BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS));
        std::filesystem::remove(path);
    }

    TEST_F(SbmdDriverTest, ActivateFailsWithoutRuntimeContext)
    {
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
        SbmdDriver::ActivationFailure failure = SbmdDriver::ActivationFailure::None;

        EXPECT_FALSE(driver->Activate(nullptr, &failure));
        EXPECT_EQ(failure, SbmdDriver::ActivationFailure::RuntimeUnavailable);
        EXPECT_FALSE(driver->IsActivated());
    }

    TEST_F(SbmdDriverTest, DeactivateAfterRuntimeShutdownDoesNotTouchFreedContext)
    {
        // A bind can fail after Matter::Stop() has shut the JS runtime down and freed its context;
        // the resulting teardown must detach the handler roots rather than release them through the
        // freed context (AddressSanitizer reports a use-after-free otherwise).
        auto driver = CreateDriver();
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            ASSERT_TRUE(driver->Activate(Ctx()));
        }

        MQuickJsRuntime::Instance().Shutdown();

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            ASSERT_EQ(MQuickJsRuntime::Instance().GetSharedContext(), nullptr);
            driver->Deactivate(MQuickJsRuntime::Instance().GetSharedContext());
        }

        EXPECT_FALSE(driver->IsActivated());
        driver.reset();

        // Restore the runtime for the remaining tests in this suite.
        SetUpTestSuite();
    }

} // namespace
