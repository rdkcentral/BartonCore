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
 * Cold-cache seed-handler regression tests.
 *
 * Loads and activates the real door-lock, contact-sensor, and water-leak
 * driver specs, then invokes their actual `seed` handlers through the
 * production arg-building and supplement-attaching path
 * (BuildResourceArgs + PrefetchSupplements + AddSupplements + InvokeHandler).
 *
 * The seeds re-run on every synchronize/reconnect. When the backing attribute
 * is unavailable (a cold attribute cache), each seed must preserve the
 * existing resource value rather than overwrite it with the commission
 * default; otherwise a locked door is reported unlocked, or a live fault is
 * cleared. These tests guard that behavior against the shipped handlers (no
 * copied handler strings that can drift).
 */

#include "SbmdDriverTestBase.h"

#include "deviceDrivers/matter/sbmd/SbmdDriver.h"
#include "deviceDrivers/matter/sbmd/SbmdRegistration.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h"

#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>

using namespace barton;
using namespace barton::test;

namespace
{
    // Loads the real driver specs once per suite and invokes their seed handlers
    // through the production invoke path, mirroring SbmdCameraWebrtcTest.
    class SbmdSeedHandlerTest : public SbmdDriverTestBase
    {
    protected:
        static std::unique_ptr<SbmdDriver> s_doorLock;
        static std::unique_ptr<SbmdDriver> s_contact;
        static std::unique_ptr<SbmdDriver> s_waterLeak;

        static std::unique_ptr<SbmdDriver> LoadSpec(const char *filename)
        {
            auto *ctx = MQuickJsRuntime::Instance().GetSharedContext();
            std::string path = std::string(SBMD_SPEC_DIR) + filename;

            std::ifstream file(path, std::ios::binary | std::ios::ate);
            EXPECT_TRUE(file.is_open()) << "Failed to open " << path;

            if (!file.is_open())
            {
                return nullptr;
            }

            auto size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::string source(static_cast<size_t>(size), '\0');
            file.read(source.data(), size);
            file.close();

            // SbmdLoader::LoadDriver, SbmdDriver construction, and Activate touch the
            // shared JS runtime; hold the runtime mutex per their contracts. Scope the
            // lock so it is released before a failed driver is destroyed below
            // (~SbmdDriver re-acquires this non-recursive mutex).
            std::unique_ptr<SbmdDriver> driver;
            bool activated = false;

            {
                std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());

                auto reg = SbmdLoader::LoadDriver(ctx, path, source.c_str(), source.size());
                EXPECT_NE(reg, nullptr) << "LoadDriver failed for " << filename;

                if (!reg)
                {
                    return nullptr;
                }

                driver = std::make_unique<SbmdDriver>(std::move(reg), std::move(source));
                activated = driver->Activate(ctx);
            }

            if (!activated)
            {
                ADD_FAILURE() << "Activate failed for " << filename;
                return nullptr;
            }

            return driver;
        }

        static void SetUpTestSuite()
        {
            InitRuntime();
            s_doorLock = LoadSpec("door-lock.sbmd.js");
            s_contact = LoadSpec("contact-sensor.sbmd.js");
            s_waterLeak = LoadSpec("water-leak-detector.sbmd.js");
        }

        static void TearDownTestSuite()
        {
            auto *ctx = MQuickJsRuntime::Instance().GetSharedContext();

            for (auto *slot : {&s_doorLock, &s_contact, &s_waterLeak})
            {
                if (*slot)
                {
                    {
                        std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
                        (*slot)->Deactivate(ctx);
                    }

                    // Release the mutex before reset(): ~SbmdDriver re-acquires this
                    // non-recursive mutex.
                    slot->reset();
                }
            }

            ShutdownRuntime();
        }

        void SetUp() override
        {
            SbmdDriverTestBase::SetUp();
            ASSERT_NE(s_doorLock, nullptr) << "door-lock.sbmd.js failed to load";
            ASSERT_NE(s_contact, nullptr) << "contact-sensor.sbmd.js failed to load";
            ASSERT_NE(s_waterLeak, nullptr) << "water-leak-detector.sbmd.js failed to load";
        }

        static const SbmdResource *
        FindResource(const SbmdDriver &driver, const std::string &endpointId, const std::string &resourceId)
        {
            for (const auto &ep : driver.GetRegistration().endpoints)
            {
                if (ep.id != endpointId)
                {
                    continue;
                }

                for (const auto &res : ep.resources)
                {
                    if (res.id == resourceId)
                    {
                        return &res;
                    }
                }
            }

            return nullptr;
        }

        // Invoke a resource's production seed handler through the real arg-building
        // and supplement-attaching path, returning the value of the first
        // updateResource op. attrValue == nullopt models a cold attribute cache.
        std::string SeedUpdateValue(const SbmdDriver &driver,
                                    const std::string &endpointId,
                                    const std::string &resourceId,
                                    const std::string &attrAlias,
                                    const std::optional<std::string> &attrValue,
                                    const std::string &resourceKey,
                                    const std::optional<std::string> &resourceValue)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto *ctx = Ctx();

            const SbmdResource *resource = FindResource(driver, endpointId, resourceId);

            if (resource == nullptr || !resource->seed.has_value())
            {
                ADD_FAILURE() << "No seed handler for " << endpointId << "/" << resourceId;
                return "";
            }

            // Root the handler: the arg-building/supplement calls below allocate, and
            // mquickjs's moving GC can relocate an unrooted function object.
            SafeJSValue handler(ctx, resource->seed->Fn());
            HandlerContext hctx = MakeContext("test-device-uuid", endpointId);
            SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, resourceId, std::nullopt);

            const SbmdSupplements &supplements = resource->seed->supplements;
            FetchAndAddSupplements(
                ctx,
                args,
                supplements,
                [&](const std::string &alias) -> std::optional<std::string> {
                    return alias == attrAlias ? attrValue : std::nullopt;
                },
                [&](const std::string &key) -> std::optional<std::string> {
                    return key == resourceKey ? resourceValue : std::nullopt;
                },
                [](const std::string &) { return std::nullopt; },
                [](const std::string &) { return std::nullopt; });

            auto result = SbmdHandlerInvoker::InvokeHandler(ctx, handler.Get(), args);

            if (!result.has_value())
            {
                ADD_FAILURE() << "Seed handler returned no result for " << endpointId << "/" << resourceId;
                return "";
            }

            for (const auto &op : result->ops)
            {
                if (std::holds_alternative<ResultOp::UpdateResource>(op.data))
                {
                    return std::get<ResultOp::UpdateResource>(op.data).value;
                }
            }

            ADD_FAILURE() << "Seed produced no updateResource op for " << endpointId << "/" << resourceId;
            return "";
        }
    };

    std::unique_ptr<SbmdDriver> SbmdSeedHandlerTest::s_doorLock;
    std::unique_ptr<SbmdDriver> SbmdSeedHandlerTest::s_contact;
    std::unique_ptr<SbmdDriver> SbmdSeedHandlerTest::s_waterLeak;

    // ------------------------------------------------------------------------
    // Door lock `locked` seed
    // ------------------------------------------------------------------------

    TEST_F(SbmdSeedHandlerTest, DoorLockPreservesLockedWhenLockStateUnavailable)
    {
        // Cold cache on a re-seed must not report a physically locked door as unlocked.
        EXPECT_EQ(SeedUpdateValue(*s_doorLock, "1", "locked", "lockState", std::nullopt, "1/locked", "true"), "true");
    }

    TEST_F(SbmdSeedHandlerTest, DoorLockDefaultsUnlockedAtCommission)
    {
        EXPECT_EQ(SeedUpdateValue(*s_doorLock, "1", "locked", "lockState", std::nullopt, "1/locked", std::nullopt),
                  "false");
    }

    TEST_F(SbmdSeedHandlerTest, DoorLockUnlockedAttributeOverridesExistingLocked)
    {
        // LockState present (Unlocked = 2; TLV bytes 0x04 0x02) wins over existing "true".
        EXPECT_EQ(SeedUpdateValue(*s_doorLock, "1", "locked", "lockState", std::string("BAI="), "1/locked", "true"),
                  "false");
    }

    TEST_F(SbmdSeedHandlerTest, DoorLockLockedAttributeOverridesExistingUnlocked)
    {
        // LockState present (Locked = 1; TLV bytes 0x04 0x01) wins over existing "false".
        EXPECT_EQ(SeedUpdateValue(*s_doorLock, "1", "locked", "lockState", std::string("BAE="), "1/locked", "false"),
                  "true");
    }

    TEST_F(SbmdSeedHandlerTest, DoorLockNotFullyLockedMapsToUnlocked)
    {
        // LockState NotFullyLocked (0; TLV bytes 0x04 0x00) maps to "false".
        EXPECT_EQ(SeedUpdateValue(*s_doorLock, "1", "locked", "lockState", std::string("BAA="), "1/locked", "true"),
                  "false");
    }

    TEST_F(SbmdSeedHandlerTest, DoorLockUnlatchedMapsToUnlocked)
    {
        // LockState Unlatched (3; TLV bytes 0x04 0x03) maps to "false".
        EXPECT_EQ(SeedUpdateValue(*s_doorLock, "1", "locked", "lockState", std::string("BAM="), "1/locked", "true"),
                  "false");
    }

    // ------------------------------------------------------------------------
    // Contact sensor `faulted` seed (fail-safe default: faulted)
    // ------------------------------------------------------------------------

    TEST_F(SbmdSeedHandlerTest, ContactPreservesFaultedWhenStateValueUnavailable)
    {
        EXPECT_EQ(SeedUpdateValue(*s_contact, "1", "faulted", "stateValue", std::nullopt, "1/faulted", "false"),
                  "false");
    }

    TEST_F(SbmdSeedHandlerTest, ContactFailsSafeToFaultedAtCommission)
    {
        EXPECT_EQ(SeedUpdateValue(*s_contact, "1", "faulted", "stateValue", std::nullopt, "1/faulted", std::nullopt),
                  "true");
    }

    TEST_F(SbmdSeedHandlerTest, ContactStateValuePresentOverridesExisting)
    {
        // StateValue closed (true; TLV byte 0x09) maps to not-faulted, overriding existing "true".
        EXPECT_EQ(SeedUpdateValue(*s_contact, "1", "faulted", "stateValue", std::string("CQ=="), "1/faulted", "true"),
                  "false");
    }

    // ------------------------------------------------------------------------
    // Water leak `faulted` seed (fail-safe default: not faulted)
    // ------------------------------------------------------------------------

    TEST_F(SbmdSeedHandlerTest, WaterLeakPreservesFaultedWhenStateValueUnavailable)
    {
        EXPECT_EQ(SeedUpdateValue(*s_waterLeak, "1", "faulted", "stateValue", std::nullopt, "1/faulted", "true"),
                  "true");
    }

    TEST_F(SbmdSeedHandlerTest, WaterLeakDefaultsNotFaultedAtCommission)
    {
        EXPECT_EQ(SeedUpdateValue(*s_waterLeak, "1", "faulted", "stateValue", std::nullopt, "1/faulted", std::nullopt),
                  "false");
    }

    TEST_F(SbmdSeedHandlerTest, WaterLeakStateValuePresentOverridesExisting)
    {
        // StateValue water-present (true; TLV byte 0x09) maps to faulted, overriding existing "false".
        EXPECT_EQ(
            SeedUpdateValue(*s_waterLeak, "1", "faulted", "stateValue", std::string("CQ=="), "1/faulted", "false"),
            "true");
    }
} // namespace
