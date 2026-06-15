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
 * Unit tests for SbmdDispatchTable — dispatch table construction, lookup,
 * and priority ordering.
 *
 * Also tests integration with SbmdDriver — dispatch tables built during
 * activation and cleared during deactivation.
 */

#include "deviceDrivers/matter/sbmd/SbmdDispatch.h"
#include "deviceDrivers/matter/sbmd/SbmdDriver.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdUtilsLoader.h"
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
    // ========================================================================
    // Pure dispatch table tests (no JS engine needed)
    // ========================================================================

    class SbmdDispatchTableTest : public ::testing::Test
    {
    protected:
        // Helper to create a simple alias
        static SbmdAlias MakeAttrAlias(const std::string &name, uint32_t clusterId, uint32_t attrId)
        {
            SbmdAlias alias;
            alias.name = name;
            alias.clusterId = clusterId;
            alias.attributeId = attrId;

            return alias;
        }

        static SbmdAlias MakeEventAlias(const std::string &name, uint32_t clusterId, uint32_t eventId)
        {
            SbmdAlias alias;
            alias.name = name;
            alias.clusterId = clusterId;
            alias.eventId = eventId;

            return alias;
        }

        static SbmdAlias MakeCmdAlias(const std::string &name, uint32_t clusterId, uint32_t cmdId)
        {
            SbmdAlias alias;
            alias.name = name;
            alias.clusterId = clusterId;
            alias.commandId = cmdId;

            return alias;
        }

        // Helper to create a wildcard alias (no element ID set)
        static SbmdAlias MakeWildcardAlias(const std::string &name, uint32_t clusterId)
        {
            SbmdAlias alias;
            alias.name = name;
            alias.clusterId = clusterId;

            return alias;
        }

        // Helper to create a handler with given aliases
        static SbmdDeviceHandler MakeHandler(const std::string &name, const std::vector<std::string> &aliases)
        {
            SbmdDeviceHandler handler;
            handler.name = name;
            handler.aliases = aliases;
            handler.handler = JS_UNDEFINED; // Not needed for table tests

            return handler;
        }
    };

    TEST_F(SbmdDispatchTableTest, EmptyTableLookupReturnsEmpty)
    {
        SbmdDispatchTable table;
        auto results = table.Lookup(0x0006, 0x0000);
        EXPECT_TRUE(results.empty());
    }

    TEST_F(SbmdDispatchTableTest, SingleSpecificHandler)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("onOffHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0]->handler->name, "onOffHandler");
        EXPECT_EQ(results[0]->priority, HandlerPriority::Specific);
    }

    TEST_F(SbmdDispatchTableTest, NoMatchReturnsEmpty)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("onOffHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        // Different cluster
        EXPECT_TRUE(table.Lookup(0x0008, 0x0000).empty());
        // Different attribute
        EXPECT_TRUE(table.Lookup(0x0006, 0x0001).empty());
    }

    TEST_F(SbmdDispatchTableTest, MultiAliasHandlerMatchesAll)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);
        aliases["currentLevel"] = MakeAttrAlias("currentLevel", 0x0008, 0x0000);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("lightState", {"onOff", "currentLevel"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        // Should match both
        auto r1 = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(r1.size(), 1u);
        EXPECT_EQ(r1[0]->handler->name, "lightState");
        EXPECT_EQ(r1[0]->priority, HandlerPriority::Multi);

        auto r2 = table.Lookup(0x0008, 0x0000);
        ASSERT_EQ(r2.size(), 1u);
        EXPECT_EQ(r2[0]->handler->name, "lightState");
        EXPECT_EQ(r2[0]->priority, HandlerPriority::Multi);
    }

    TEST_F(SbmdDispatchTableTest, WildcardHandlerMatchesAnyElementInCluster)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["anyOnOff"] = MakeWildcardAlias("anyOnOff", 0x0006);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("wildcardHandler", {"anyOnOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        // Matches any attribute in cluster 0x0006
        auto r1 = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(r1.size(), 1u);
        EXPECT_EQ(r1[0]->handler->name, "wildcardHandler");
        EXPECT_EQ(r1[0]->priority, HandlerPriority::Wildcard);

        auto r2 = table.Lookup(0x0006, 0x0001);
        ASSERT_EQ(r2.size(), 1u);

        auto r3 = table.Lookup(0x0006, 0xFFFF);
        ASSERT_EQ(r3.size(), 1u);

        // Different cluster — no match
        EXPECT_TRUE(table.Lookup(0x0008, 0x0000).empty());
    }

    TEST_F(SbmdDispatchTableTest, PriorityOrderSpecificBeforeMulti)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);
        aliases["currentLevel"] = MakeAttrAlias("currentLevel", 0x0008, 0x0000);

        std::vector<SbmdDeviceHandler> handlers;
        // Multi handler registered first
        handlers.push_back(MakeHandler("multiHandler", {"onOff", "currentLevel"}));
        // Specific handler registered second
        handlers.push_back(MakeHandler("specificHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 2u);
        // Specific should come first regardless of registration order
        EXPECT_EQ(results[0]->handler->name, "specificHandler");
        EXPECT_EQ(results[0]->priority, HandlerPriority::Specific);
        EXPECT_EQ(results[1]->handler->name, "multiHandler");
        EXPECT_EQ(results[1]->priority, HandlerPriority::Multi);
    }

    TEST_F(SbmdDispatchTableTest, PriorityOrderSpecificBeforeWildcard)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);
        aliases["anyOnOff"] = MakeWildcardAlias("anyOnOff", 0x0006);

        std::vector<SbmdDeviceHandler> handlers;
        // Wildcard first
        handlers.push_back(MakeHandler("wildcardHandler", {"anyOnOff"}));
        // Specific second
        handlers.push_back(MakeHandler("specificHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 2u);
        // Specific first, wildcard second
        EXPECT_EQ(results[0]->handler->name, "specificHandler");
        EXPECT_EQ(results[0]->priority, HandlerPriority::Specific);
        EXPECT_EQ(results[1]->handler->name, "wildcardHandler");
        EXPECT_EQ(results[1]->priority, HandlerPriority::Wildcard);
    }

    TEST_F(SbmdDispatchTableTest, AllThreePriorities)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);
        aliases["currentLevel"] = MakeAttrAlias("currentLevel", 0x0008, 0x0000);
        aliases["anyOnOff"] = MakeWildcardAlias("anyOnOff", 0x0006);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("wildcardHandler", {"anyOnOff"}));
        handlers.push_back(MakeHandler("multiHandler", {"onOff", "currentLevel"}));
        handlers.push_back(MakeHandler("specificHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 3u);
        EXPECT_EQ(results[0]->handler->name, "specificHandler");
        EXPECT_EQ(results[0]->priority, HandlerPriority::Specific);
        EXPECT_EQ(results[1]->handler->name, "multiHandler");
        EXPECT_EQ(results[1]->priority, HandlerPriority::Multi);
        EXPECT_EQ(results[2]->handler->name, "wildcardHandler");
        EXPECT_EQ(results[2]->priority, HandlerPriority::Wildcard);
    }

    TEST_F(SbmdDispatchTableTest, UnknownAliasSkipped)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        // "onOff" alias is NOT defined

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("brokenHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        EXPECT_EQ(table.GetSpecificEntryCount(), 0u);
        EXPECT_EQ(table.GetWildcardEntryCount(), 0u);
    }

    TEST_F(SbmdDispatchTableTest, EventDispatch)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["lockOp"] = MakeEventAlias("lockOp", 0x0101, 2);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("lockOpHandler", {"lockOp"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0101, 2);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0]->handler->name, "lockOpHandler");
    }

    TEST_F(SbmdDispatchTableTest, CommandDispatch)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["lockDoor"] = MakeCmdAlias("lockDoor", 0x0101, 0);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("lockCmdHandler", {"lockDoor"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0101, 0);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0]->handler->name, "lockCmdHandler");
    }

    TEST_F(SbmdDispatchTableTest, ClearRemovesAllEntries)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("handler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);
        EXPECT_EQ(table.GetSpecificEntryCount(), 1u);

        table.Clear();
        EXPECT_EQ(table.GetSpecificEntryCount(), 0u);
        EXPECT_TRUE(table.Lookup(0x0006, 0x0000).empty());
    }

    TEST_F(SbmdDispatchTableTest, MultipleHandlersSameKey)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("handler1", {"onOff"}));
        handlers.push_back(MakeHandler("handler2", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 2u);
        // Both are specific, so stable order (registration order preserved)
        EXPECT_EQ(results[0]->handler->name, "handler1");
        EXPECT_EQ(results[1]->handler->name, "handler2");
    }

    // ========================================================================
    // Integration with SbmdDriver (requires JS engine)
    // ========================================================================

    class SbmdDispatchDriverTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx));
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

        std::unique_ptr<SbmdDriver> CreateDriver(const std::string &source)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto reg = SbmdLoader::LoadDriver(Ctx(), "<test>", source.c_str(), source.size());

            if (!reg)
            {
                return nullptr;
            }

            return std::make_unique<SbmdDriver>(std::move(reg), source);
        }

        std::optional<ParsedResult> CallHandler(JSValue handler)
        {
            auto *ctx = Ctx();

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

    TEST_F(SbmdDispatchDriverTest, DispatchTablesBuiltOnActivation)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: "1.0",
                name: "DispatchTest",
                constants: { CL_ON_OFF: 6, ATTR_ON_OFF: 0, CL_DOOR_LOCK: 257, EVT_LOCK_OP: 2 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: {
                    onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF, type: "bool" },
                    lockOp: { clusterId: CL_DOOR_LOCK, eventId: EVT_LOCK_OP },
                },
                attributeHandlers: {
                    onOffHandler: {
                        aliases: ["onOff"],
                        handler: handleOnOff,
                    },
                },
                eventHandlers: {
                    lockHandler: {
                        aliases: ["lockOp"],
                        handler: handleLockOp,
                    },
                },
            });
            function handleOnOff(args) { return SbmdUtils.result().success(); }
            function handleLockOp(args) { return SbmdUtils.result().success(); }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        // Attribute dispatch should match onOff
        auto attrResults = driver->GetAttributeDispatch().Lookup(0x0006, 0x0000);
        ASSERT_EQ(attrResults.size(), 1u);
        EXPECT_EQ(attrResults[0]->handler->name, "onOffHandler");

        // Event dispatch should match lockOp
        auto eventResults = driver->GetEventDispatch().Lookup(0x0101, 2);
        ASSERT_EQ(eventResults.size(), 1u);
        EXPECT_EQ(eventResults[0]->handler->name, "lockHandler");

        // Command dispatch should be empty
        EXPECT_TRUE(driver->GetCommandDispatch().Lookup(0x0006, 0x0000).empty());

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDispatchDriverTest, DispatchTablesClearedOnDeactivation)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: "1.0",
                name: "ClearTest",
                constants: { CL_ON_OFF: 6, ATTR_ON_OFF: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: { onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF } },
                attributeHandlers: {
                    handler: {
                        aliases: ["onOff"],
                        handler: fn,
                    },
                },
            });
            function fn(args) { return SbmdUtils.result().success(); }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_FALSE(driver->GetAttributeDispatch().Lookup(0x0006, 0x0000).empty());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }

        EXPECT_TRUE(driver->GetAttributeDispatch().Lookup(0x0006, 0x0000).empty());
    }

    TEST_F(SbmdDispatchDriverTest, DispatchToHandlerAndInvoke)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: "1.0",
                name: "InvokeTest",
                constants: { CL_ON_OFF: 6, ATTR_ON_OFF: 0, CMD_ON: 1 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: { onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF } },
                attributeHandlers: {
                    onOffHandler: {
                        aliases: ["onOff"],
                        handler: handleOnOff,
                    },
                },
            });
            function handleOnOff(args) {
                return SbmdUtils.result()
                    .dataModel.updateResource("1", "isOn", "true")
                    .success();
            }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        // Look up the handler
        auto results = driver->GetAttributeDispatch().Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 1u);

        // Call it and verify the result
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto parsed = CallHandler(results[0]->handler->handler);
            ASSERT_TRUE(parsed.has_value());
            ASSERT_EQ(parsed->ops.size(), 1u);
            ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(parsed->ops[0].data));

            auto &ur = std::get<ResultOp::UpdateResource>(parsed->ops[0].data);
            EXPECT_EQ(*ur.endpoint, "1");
            EXPECT_EQ(ur.resource, "isOn");
            EXPECT_EQ(ur.value, "true");
            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(parsed->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

} // namespace
