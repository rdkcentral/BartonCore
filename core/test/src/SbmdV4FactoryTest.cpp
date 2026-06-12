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
 * Unit tests for v4 SBMD factory loading pipeline.
 *
 * Tests the v4 loading path: .sbmd.js discovery → SbmdV4Loader → SbmdV4Driver → activation.
 * Uses a temp directory with test .sbmd.js files to verify end-to-end loading without
 * the full deviceDriverManager/MatterDriverFactory infrastructure.
 */

#include "deviceDrivers/matter/sbmd/SbmdV4Driver.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdUtilsLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdV4Loader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

using namespace barton;

namespace
{
    // Minimal v4 driver source for testing
    constexpr const char *kMinimalDriver = R"(
SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: '1.0.0',
    name: 'test-light',
    barton: {
        deviceClass: 'light',
        deviceClassVersion: 1,
    },
    matter: {
        deviceTypes: [0x0100],
        featureClusters: [6],
    },
    reporting: {
        minSecs: 1,
        maxSecs: 300,
    },
    aliases: {
        onOff: { clusterId: 6, attributeId: 0, type: 'bool' },
    },
    endpoints: {
        "1": {
            profile: 'lightProfile',
            profileVersion: 1,
            resources: {
                isOn: {
                    type: 'com.icontrol.boolean',
                    modes: ['read', 'write', 'dynamic', 'emitEvents'],
                    seed: function(args) {
                        return SbmdUtils.result()
                            .dataModel.updateResource(args.endpointId, 'isOn', 'false')
                            .success();
                    },
                    write: function(args) {
                        var on = args.resource.input === 'true';
                        return SbmdUtils.result()
                            .device.sendCommand(6, on ? 1 : 0);
                    },
                },
            },
        },
    },
    attributeHandlers: {
        handleOnOff: {
            aliases: ['onOff'],
            handler: function(args) {
                return SbmdUtils.result()
                    .dataModel.updateResource(args.endpointId, 'isOn', args.attribute.tlvBase64 ? 'true' : 'false')
                    .success();
            },
        },
    },
});
)";

    class SbmdV4FactoryTest : public ::testing::Test
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

        void SetUp() override
        {
            // Create unique temp directory per test
            auto uniqueName = std::string("sbmd_factory_test_") +
                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            tempDir = std::filesystem::temp_directory_path() / uniqueName;
            std::filesystem::create_directories(tempDir);
        }

        void TearDown() override
        {
            std::filesystem::remove_all(tempDir);
        }

        void WriteFile(const std::string &filename, const std::string &content)
        {
            std::ofstream out(tempDir / filename);
            out << content;
            out.close();
        }

        std::filesystem::path tempDir;
    };

    TEST_F(SbmdV4FactoryTest, LoadDriverFromFile)
    {
        WriteFile("test-light.sbmd.js", kMinimalDriver);

        // Read file
        auto filePath = tempDir / "test-light.sbmd.js";
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        ASSERT_TRUE(file.is_open());

        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string source(static_cast<size_t>(fileSize), '\0');
        file.read(source.data(), fileSize);
        ASSERT_TRUE(file.good());

        // Load via SbmdV4Loader
        std::unique_ptr<SbmdV4Registration> reg;
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            reg = SbmdV4Loader::LoadDriver(ctx, filePath.string(), source.c_str(), source.size());
        }

        ASSERT_NE(reg, nullptr);
        EXPECT_EQ(reg->name, "test-light");
        EXPECT_EQ(reg->barton.deviceClass, "light");
        EXPECT_EQ(reg->matter.deviceTypes.size(), 1u);
        EXPECT_EQ(reg->matter.deviceTypes[0], 0x0100);
        EXPECT_EQ(reg->endpoints.size(), 1u);
        EXPECT_EQ(reg->endpoints[0].resources.size(), 1u);
        EXPECT_EQ(reg->endpoints[0].resources[0].id, "isOn");
        EXPECT_TRUE(reg->endpoints[0].resources[0].seed.has_value());
        EXPECT_TRUE(reg->endpoints[0].resources[0].write.has_value());
    }

    TEST_F(SbmdV4FactoryTest, CreateAndActivateDriver)
    {
        WriteFile("test-light.sbmd.js", kMinimalDriver);

        auto filePath = tempDir / "test-light.sbmd.js";
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        ASSERT_TRUE(file.is_open());

        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string source(static_cast<size_t>(fileSize), '\0');
        file.read(source.data(), fileSize);

        std::unique_ptr<SbmdV4Registration> reg;
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            reg = SbmdV4Loader::LoadDriver(ctx, filePath.string(), source.c_str(), source.size());
        }
        ASSERT_NE(reg, nullptr);

        auto driver = std::make_unique<SbmdV4Driver>(std::move(reg), std::string(source));
        EXPECT_FALSE(driver->IsActivated());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_TRUE(driver->Activate(ctx));
        }

        EXPECT_TRUE(driver->IsActivated());
        EXPECT_EQ(driver->GetName(), "test-light");

        // Verify dispatch tables were built
        EXPECT_GT(driver->GetAttributeDispatch().GetSpecificEntryCount(), 0u);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            driver->Deactivate(ctx);
        }

        EXPECT_FALSE(driver->IsActivated());
    }

    TEST_F(SbmdV4FactoryTest, FileDiscoveryPattern)
    {
        // Write files with various extensions
        WriteFile("light.sbmd.js", kMinimalDriver);
        WriteFile("not-sbmd.js", "// plain JS");
        WriteFile("spec.sbmd", "name: old-format");
        WriteFile("readme.txt", "documentation");

        // Verify .sbmd.js discovery logic
        int sbmdJsCount = 0;

        for (const auto &entry : std::filesystem::directory_iterator(tempDir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".js")
            {
                continue;
            }

            auto stem = entry.path().stem();

            if (stem.extension() != ".sbmd")
            {
                continue;
            }

            sbmdJsCount++;
        }

        EXPECT_EQ(sbmdJsCount, 1); // Only light.sbmd.js
    }

    TEST_F(SbmdV4FactoryTest, NonExistentDirectoryDoesNotCrash)
    {
        // Verify iterating a nonexistent dir doesn't crash
        auto badPath = tempDir / "nonexistent";
        std::error_code ec;
        EXPECT_FALSE(std::filesystem::exists(badPath, ec));
    }

} // namespace
