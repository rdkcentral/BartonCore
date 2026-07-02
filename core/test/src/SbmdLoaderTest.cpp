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
 * Unit tests for SbmdLoader — constants extraction, file evaluation,
 * and registration extraction.
 */

#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"

#include <gtest/gtest.h>
#include <optional>
#include <string>

extern "C" {
#include <mquickjs/mquickjs.h>
}

using namespace barton;

namespace
{
    class SbmdLoaderTest : public ::testing::Test
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

        std::optional<std::vector<std::pair<std::string, std::string>>> ExtractConstants(const char *source)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            return SbmdLoader::ExtractConstants(Ctx(), source, strlen(source));
        }

        std::unique_ptr<SbmdRegistration> LoadDriver(const std::string &source)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            return SbmdLoader::LoadDriver(Ctx(), "<test>", source.c_str(), source.size());
        }
    };

    // ========================================================================
    // Constants extraction tests
    // ========================================================================

    TEST_F(SbmdLoaderTest, ExtractConstantsBasic)
    {
        auto constants = ExtractConstants(R"(
            SbmdDriver({
                constants: {
                    EP_LIGHT: "1",
                    CL_ON_OFF: 0x0006,
                    ATTR_ON_OFF: 0,
                },
            });
        )");

        ASSERT_TRUE(constants.has_value());
        ASSERT_EQ(constants->size(), 3u);
        EXPECT_EQ((*constants)[0].first, "EP_LIGHT");
        EXPECT_EQ((*constants)[0].second, "\"1\"");
        EXPECT_EQ((*constants)[1].first, "CL_ON_OFF");
        EXPECT_EQ((*constants)[1].second, "6");
        EXPECT_EQ((*constants)[2].first, "ATTR_ON_OFF");
        EXPECT_EQ((*constants)[2].second, "0");
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsHexNumbers)
    {
        auto constants = ExtractConstants(R"(
            SbmdDriver({
                constants: {
                    A: 0xFF,
                    B: 0x0100,
                    C: 255,
                },
            });
        )");

        ASSERT_TRUE(constants.has_value());
        ASSERT_EQ(constants->size(), 3u);
        EXPECT_EQ((*constants)[0].first, "A");
        EXPECT_EQ((*constants)[0].second, "255");
        EXPECT_EQ((*constants)[1].first, "B");
        EXPECT_EQ((*constants)[1].second, "256");
        EXPECT_EQ((*constants)[2].first, "C");
        EXPECT_EQ((*constants)[2].second, "255");
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsBooleans)
    {
        auto constants = ExtractConstants(R"(
            SbmdDriver({ constants: { A: true, B: false } });
        )");

        ASSERT_TRUE(constants.has_value());
        ASSERT_EQ(constants->size(), 2u);
        EXPECT_EQ((*constants)[0].first, "A");
        EXPECT_EQ((*constants)[0].second, "true");
        EXPECT_EQ((*constants)[1].first, "B");
        EXPECT_EQ((*constants)[1].second, "false");
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsStringsWithEscapes)
    {
        auto constants = ExtractConstants(R"(
            SbmdDriver({ constants: { A: "hello \"world\"", B: "back\\slash" } });
        )");

        ASSERT_TRUE(constants.has_value());
        ASSERT_EQ(constants->size(), 2u);
        EXPECT_EQ((*constants)[0].first, "A");
        EXPECT_EQ((*constants)[0].second, R"("hello \"world\"")");
        EXPECT_EQ((*constants)[1].first, "B");
        EXPECT_EQ((*constants)[1].second, R"("back\\slash")");
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsControlCharacters)
    {
        // Control characters in string constant values must be re-escaped so
        // the generated 'var' declaration is valid JavaScript.
        auto constants = ExtractConstants(R"(
            SbmdDriver({ constants: { A: "hello\nworld", B: "tab\there", C: "cr\rhere" } });
        )");

        ASSERT_TRUE(constants.has_value());
        ASSERT_EQ(constants->size(), 3u);
        EXPECT_EQ((*constants)[0].first, "A");
        EXPECT_EQ((*constants)[0].second, R"("hello\nworld")");
        EXPECT_EQ((*constants)[1].first, "B");
        EXPECT_EQ((*constants)[1].second, R"("tab\there")");
        EXPECT_EQ((*constants)[2].first, "C");
        EXPECT_EQ((*constants)[2].second, R"("cr\rhere")");
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsEmptyBlock)
    {
        auto constants = ExtractConstants(R"(
            SbmdDriver({ constants: {} });
        )");

        ASSERT_TRUE(constants.has_value());
        EXPECT_TRUE(constants->empty());
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsNoConstantsBlock)
    {
        auto constants = ExtractConstants(R"(
            SbmdDriver({ name: "test" });
        )");

        ASSERT_TRUE(constants.has_value());
        EXPECT_TRUE(constants->empty());
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsRejectsNonPrimitive)
    {
        auto constants = ExtractConstants(R"(
            SbmdDriver({ constants: { A: 1, B: [1, 2] } });
        )");

        // Should fail extraction entirely since B is an array (non-primitive)
        EXPECT_FALSE(constants.has_value());
    }

    TEST_F(SbmdLoaderTest, ExtractConstantsWithNestedBraces)
    {
        // Ensure we find the right closing brace
        auto constants = ExtractConstants(R"(
            SbmdDriver({
                constants: {
                    A: 1,
                },
                barton: { deviceClass: "light" },
            });
        )");

        ASSERT_TRUE(constants.has_value());
        ASSERT_EQ(constants->size(), 1u);
        EXPECT_EQ((*constants)[0].first, "A");
        EXPECT_EQ((*constants)[0].second, "1");
    }

    TEST_F(SbmdLoaderTest, GenerateConstantsPreamble)
    {
        std::vector<std::pair<std::string, std::string>> constants = {
            {"EP_LIGHT", "\"1\""},
            {"CL_ON_OFF", "6"},
        };

        auto preamble = SbmdLoader::GenerateConstantsPreamble(constants);
        EXPECT_EQ(preamble, "var EP_LIGHT = \"1\";\nvar CL_ON_OFF = 6;\n");
    }

    TEST_F(SbmdLoaderTest, CountPreambleLines)
    {
        EXPECT_EQ(SbmdLoader::CountPreambleLines("var A = 1;\nvar B = 2;\n"), 2);
        EXPECT_EQ(SbmdLoader::CountPreambleLines(""), 0);
        EXPECT_EQ(SbmdLoader::CountPreambleLines("var A = 1;\n"), 1);
    }

    // ========================================================================
    // Full driver loading and registration extraction tests
    // ========================================================================

    TEST_F(SbmdLoaderTest, LoadMinimalDriver)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "Minimal",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
            });
        )");

        ASSERT_NE(reg, nullptr);
        EXPECT_EQ(reg->schemaVersion, "4.0");
        EXPECT_EQ(reg->driverVersion, 1u);
        EXPECT_EQ(reg->name, "Minimal");
        EXPECT_EQ(reg->barton.deviceClass, "test");
        EXPECT_EQ(reg->barton.deviceClassVersion, 0u);
        ASSERT_EQ(reg->matter.deviceTypes.size(), 1u);
        EXPECT_EQ(reg->matter.deviceTypes[0], 0x0100);
    }

    TEST_F(SbmdLoaderTest, LoadDriverWithConstants)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "WithConstants",
                constants: {
                    EP_LIGHT: "1",
                    CL_ON_OFF: 0x0006,
                },
                barton: { deviceClass: "light", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                endpoints: {
                    "1": {
                        profile: "light",
                        profileVersion: 0,
                        resources: {},
                    },
                },
            });
        )");

        ASSERT_NE(reg, nullptr);
        EXPECT_EQ(reg->name, "WithConstants");
        ASSERT_EQ(reg->endpoints.size(), 1u);
        EXPECT_EQ(reg->endpoints[0].id, "1"); // EP_LIGHT resolved to "1"
    }

    TEST_F(SbmdLoaderTest, LoadDriverWithAliases)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "WithAliases",
                constants: { CL_ON_OFF: 6, ATTR_ON_OFF: 0, CL_DOOR_LOCK: 257, EVT_LOCK_OP: 2 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: {
                    onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF, type: "bool" },
                    lockOp: { clusterId: CL_DOOR_LOCK, eventId: EVT_LOCK_OP },
                },
            });
        )");

        ASSERT_NE(reg, nullptr);
        ASSERT_EQ(reg->aliases.size(), 2u);

        auto it = reg->aliases.find("onOff");
        ASSERT_NE(it, reg->aliases.end());
        EXPECT_EQ(it->second.clusterId, 6u);
        EXPECT_TRUE(it->second.attributeId.has_value());
        EXPECT_EQ(it->second.attributeId.value(), 0u);
        EXPECT_FALSE(it->second.eventId.has_value());
        EXPECT_EQ(it->second.type, "bool");

        auto it2 = reg->aliases.find("lockOp");
        ASSERT_NE(it2, reg->aliases.end());
        EXPECT_EQ(it2->second.clusterId, 257u);
        EXPECT_TRUE(it2->second.eventId.has_value());
        EXPECT_EQ(it2->second.eventId.value(), 2u);
    }

    TEST_F(SbmdLoaderTest, LoadDriverWithResourceHandlers)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "WithHandlers",
                constants: { EP: "1", CL: 6, ATTR: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: {
                    onOff: { clusterId: CL, attributeId: ATTR, type: "bool" },
                },
                endpoints: {
                    "1": {
                        profile: "light",
                        profileVersion: 0,
                        resources: {
                            isOn: {
                                type: "boolean",
                                modes: ["read", "write"],
                                read: {
                                    supplements: { attributes: ["onOff"] },
                                    handler: readIsOn,
                                },
                                write: writeIsOn,
                            },
                        },
                    },
                },
            });

            function readIsOn(args) {
                return Sbmd.result()
                    .dataModel.updateResource("1", "isOn", "true")
                    .success();
            }

            function writeIsOn(args) {
                return Sbmd.result()
                    .device.sendCommand(CL, 1);
            }
        )");

        ASSERT_NE(reg, nullptr);
        ASSERT_EQ(reg->endpoints.size(), 1u);
        ASSERT_EQ(reg->endpoints[0].resources.size(), 1u);

        auto &res = reg->endpoints[0].resources[0];
        EXPECT_EQ(res.id, "isOn");
        EXPECT_EQ(res.type, "boolean");
        ASSERT_EQ(res.modes.size(), 2u);
        EXPECT_EQ(res.modes[0], "read");
        EXPECT_EQ(res.modes[1], "write");

        // Read handler has supplements
        ASSERT_TRUE(res.read.has_value());
        EXPECT_FALSE(JS_IsUndefined(res.read->handler));
        ASSERT_EQ(res.read->supplements.attributes.size(), 1u);
        EXPECT_EQ(res.read->supplements.attributes[0], "onOff");

        // Write handler is a plain function (no supplements)
        ASSERT_TRUE(res.write.has_value());
        EXPECT_FALSE(JS_IsUndefined(res.write->handler));
        EXPECT_TRUE(res.write->supplements.attributes.empty());
    }

    TEST_F(SbmdLoaderTest, LoadDriverWithAttributeHandlers)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "AttrHandlers",
                constants: { CL: 6, ATTR: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: {
                    onOff: { clusterId: CL, attributeId: ATTR },
                },
                attributeHandlers: {
                    onOff: {
                        aliases: ["onOff"],
                        handler: handleOnOff,
                    },
                },
            });

            function handleOnOff(args) {
                return Sbmd.result()
                    .dataModel.updateResource("1", "isOn", "true")
                    .success();
            }
        )");

        ASSERT_NE(reg, nullptr);
        ASSERT_EQ(reg->attributeHandlers.size(), 1u);
        EXPECT_EQ(reg->attributeHandlers[0].name, "onOff");
        ASSERT_EQ(reg->attributeHandlers[0].aliases.size(), 1u);
        EXPECT_EQ(reg->attributeHandlers[0].aliases[0], "onOff");
        EXPECT_FALSE(JS_IsUndefined(reg->attributeHandlers[0].handler));
    }

    TEST_F(SbmdLoaderTest, LoadDriverWithReporting)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "WithReporting",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100], revision: 2 },
                reporting: { minSecs: 1, maxSecs: 3600 },
            });
        )");

        ASSERT_NE(reg, nullptr);
        EXPECT_EQ(reg->reporting.minSecs, 1u);
        EXPECT_EQ(reg->reporting.maxSecs, 3600u);
        EXPECT_TRUE(reg->matter.revision.has_value());
        EXPECT_EQ(reg->matter.revision.value(), 2u);
    }

    TEST_F(SbmdLoaderTest, LoadDriverWithPrerequisites)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "WithPrereqs",
                constants: { EP: "1" },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: {
                    currentLevel: { clusterId: 8, attributeId: 0 },
                },
                endpoints: {
                    "1": {
                        profile: "light",
                        profileVersion: 0,
                        resources: {
                            level: {
                                type: "number",
                                modes: ["read"],
                                prerequisites: ["currentLevel"],
                                optional: true,
                                read: readLevel,
                            },
                        },
                    },
                },
            });

            function readLevel(args) {
                return Sbmd.result().success();
            }
        )");

        ASSERT_NE(reg, nullptr);
        ASSERT_EQ(reg->endpoints.size(), 1u);
        ASSERT_EQ(reg->endpoints[0].resources.size(), 1u);

        auto &res = reg->endpoints[0].resources[0];
        EXPECT_TRUE(res.optional);
        ASSERT_EQ(res.prerequisites.size(), 1u);
        EXPECT_EQ(res.prerequisites[0], "currentLevel");
    }

    TEST_F(SbmdLoaderTest, LoadDriverWithMatterOptions)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "MatterOpts",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: {
                    deviceTypes: [0x0100, 0x0101],
                    revision: 3,
                    featureClusters: [6, 8],
                    vendorId: 0x1234,
                    productId: 0x5678,
                    defaultTimeoutMs: 10000,
                },
            });
        )");

        ASSERT_NE(reg, nullptr);
        ASSERT_EQ(reg->matter.deviceTypes.size(), 2u);
        EXPECT_EQ(reg->matter.deviceTypes[0], 0x0100);
        EXPECT_EQ(reg->matter.deviceTypes[1], 0x0101);
        EXPECT_TRUE(reg->matter.revision.has_value());
        EXPECT_EQ(reg->matter.revision.value(), 3u);
        ASSERT_EQ(reg->matter.featureClusters.size(), 2u);
        EXPECT_EQ(reg->matter.featureClusters[0], 6u);
        EXPECT_EQ(reg->matter.featureClusters[1], 8u);
        EXPECT_TRUE(reg->matter.vendorId.has_value());
        EXPECT_EQ(reg->matter.vendorId.value(), 0x1234);
        EXPECT_TRUE(reg->matter.productId.has_value());
        EXPECT_EQ(reg->matter.productId.value(), 0x5678);
        EXPECT_TRUE(reg->matter.defaultTimeoutMs.has_value());
        EXPECT_EQ(reg->matter.defaultTimeoutMs.value(), 10000u);
    }

    TEST_F(SbmdLoaderTest, DefaultTimeoutAbsentWhenNotSpecified)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "NoTimeout",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
            });
        )");

        ASSERT_NE(reg, nullptr);
        EXPECT_FALSE(reg->matter.defaultTimeoutMs.has_value());
    }

    TEST_F(SbmdLoaderTest, ReportingDefaultsToZeroWhenAbsent)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "NoReporting",
                constants: {},
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
            });
        )");

        ASSERT_NE(reg, nullptr);
        EXPECT_EQ(reg->reporting.minSecs, 0u);
        EXPECT_EQ(reg->reporting.maxSecs, 0u);
    }

    TEST_F(SbmdLoaderTest, LoadDriverMissingNameFails)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                constants: {},
                barton: { deviceClass: "test" },
                matter: { deviceTypes: [] },
            });
        )");

        EXPECT_EQ(reg, nullptr);
    }

    TEST_F(SbmdLoaderTest, LoadDriverDoubleSbmdDriverCallFails)
    {
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "First",
                constants: {},
                barton: { deviceClass: "test" },
                matter: { deviceTypes: [] },
            });
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "Second",
                constants: {},
                barton: { deviceClass: "test" },
                matter: { deviceTypes: [] },
            });
        )");

        EXPECT_EQ(reg, nullptr);
    }

    TEST_F(SbmdLoaderTest, LoadDriverNoSbmdDriverCallFails)
    {
        auto reg = LoadDriver(R"(
            // Just some random code
            var x = 42;
        )");

        EXPECT_EQ(reg, nullptr);
    }

    TEST_F(SbmdLoaderTest, ConstantsAvailableInHandlers)
    {
        // Verify that constants injected as var declarations are accessible
        // inside handler functions via the IIFE scope
        auto reg = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "ConstInHandlers",
                constants: {
                    EP: "1",
                    CL_ON_OFF: 6,
                    CMD_ON: 1,
                },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                endpoints: {
                    "1": {
                        profile: "light",
                        profileVersion: 0,
                        resources: {
                            isOn: {
                                type: "boolean",
                                modes: ["write"],
                                write: writeIsOn,
                            },
                        },
                    },
                },
            });

            function writeIsOn(args) {
                // Constants should be in scope here
                return Sbmd.result()
                    .device.sendCommand(CL_ON_OFF, CMD_ON);
            }
        )");

        ASSERT_NE(reg, nullptr);
        EXPECT_EQ(reg->endpoints[0].id, "1");
        // The handler function captured — constants were in scope
        ASSERT_TRUE(reg->endpoints[0].resources[0].write.has_value());
    }

    TEST_F(SbmdLoaderTest, CrossDriverIsolation)
    {
        // Load two drivers with same function names — IIFE wrapping should prevent collision
        auto reg1 = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "Driver1",
                constants: {},
                barton: { deviceClass: "test1", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                endpoints: {
                    "1": {
                        profile: "test",
                        profileVersion: 0,
                        resources: {
                            val: { type: "string", modes: ["read"], read: myRead },
                        },
                    },
                },
            });
            function myRead(args) { return Sbmd.result().success(); }
        )");

        ASSERT_NE(reg1, nullptr);
        EXPECT_EQ(reg1->name, "Driver1");

        auto reg2 = LoadDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "Driver2",
                constants: {},
                barton: { deviceClass: "test2", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0101] },
                endpoints: {
                    "1": {
                        profile: "test",
                        profileVersion: 0,
                        resources: {
                            val: { type: "string", modes: ["read"], read: myRead },
                        },
                    },
                },
            });
            function myRead(args) { return Sbmd.result().success(); }
        )");

        ASSERT_NE(reg2, nullptr);
        EXPECT_EQ(reg2->name, "Driver2");
        EXPECT_EQ(reg2->barton.deviceClass, "test2");
    }

} // namespace
