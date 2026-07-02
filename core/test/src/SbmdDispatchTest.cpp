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
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h"

#include <gtest/gtest.h>
#include <string>

extern "C" {
#include <mquickjs/mquickjs.h>

// Stubs for C APIs referenced by SbmdHandlerInvoker
void updateResource(const char *, const char *, const char *, const char *, void *) {}

void setMetadata(const char *, const char *, const char *, const char *) {}

bool deviceServiceSetMetadata(const char *, const char *)
{
    return true;
}
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

    TEST_F(SbmdDispatchTableTest, SingleAliasHandler)
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
        EXPECT_EQ(results[0]->priority, HandlerPriority::Single);
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

    TEST_F(SbmdDispatchTableTest, PriorityOrderSingleBeforeMulti)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);
        aliases["currentLevel"] = MakeAttrAlias("currentLevel", 0x0008, 0x0000);

        std::vector<SbmdDeviceHandler> handlers;
        // Multi handler registered first
        handlers.push_back(MakeHandler("multiHandler", {"onOff", "currentLevel"}));
        // Single-alias handler registered second
        handlers.push_back(MakeHandler("singleHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 2u);
        // Single should come first regardless of registration order
        EXPECT_EQ(results[0]->handler->name, "singleHandler");
        EXPECT_EQ(results[0]->priority, HandlerPriority::Single);
        EXPECT_EQ(results[1]->handler->name, "multiHandler");
        EXPECT_EQ(results[1]->priority, HandlerPriority::Multi);
    }

    TEST_F(SbmdDispatchTableTest, PriorityOrderSingleBeforeWildcard)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0x0000);
        aliases["anyOnOff"] = MakeWildcardAlias("anyOnOff", 0x0006);

        std::vector<SbmdDeviceHandler> handlers;
        // Wildcard first
        handlers.push_back(MakeHandler("wildcardHandler", {"anyOnOff"}));
        // Single second
        handlers.push_back(MakeHandler("singleHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 2u);
        // Single first, wildcard second
        EXPECT_EQ(results[0]->handler->name, "singleHandler");
        EXPECT_EQ(results[0]->priority, HandlerPriority::Single);
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
        handlers.push_back(MakeHandler("singleHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto results = table.Lookup(0x0006, 0x0000);
        ASSERT_EQ(results.size(), 3u);
        EXPECT_EQ(results[0]->handler->name, "singleHandler");
        EXPECT_EQ(results[0]->priority, HandlerPriority::Single);
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

    TEST_F(SbmdDispatchTableTest, GetRegisteredClusterIdsEmpty)
    {
        SbmdDispatchTable table;
        auto ids = table.GetRegisteredClusterIds();
        EXPECT_TRUE(ids.empty());
    }

    TEST_F(SbmdDispatchTableTest, GetRegisteredClusterIdsFromSpecific)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["lockDoor"] = MakeCmdAlias("lockDoor", 0x0101, 0);
        aliases["unlockDoor"] = MakeCmdAlias("unlockDoor", 0x0101, 1);
        aliases["onOff"] = MakeAttrAlias("onOff", 0x0006, 0);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("lockHandler", {"lockDoor"}));
        handlers.push_back(MakeHandler("unlockHandler", {"unlockDoor"}));
        handlers.push_back(MakeHandler("onOffHandler", {"onOff"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto ids = table.GetRegisteredClusterIds();
        EXPECT_EQ(ids.size(), 2u);
        EXPECT_TRUE(ids.count(0x0101));
        EXPECT_TRUE(ids.count(0x0006));
    }

    TEST_F(SbmdDispatchTableTest, GetRegisteredClusterIdsFromWildcard)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["anyDoorLock"] = MakeWildcardAlias("anyDoorLock", 0x0101);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("wildcardHandler", {"anyDoorLock"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto ids = table.GetRegisteredClusterIds();
        EXPECT_EQ(ids.size(), 1u);
        EXPECT_TRUE(ids.count(0x0101));
    }

    TEST_F(SbmdDispatchTableTest, GetRegisteredClusterIdsMixed)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["lockDoor"] = MakeCmdAlias("lockDoor", 0x0101, 0);
        aliases["anyOnOff"] = MakeWildcardAlias("anyOnOff", 0x0006);
        aliases["level"] = MakeAttrAlias("level", 0x0008, 0);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("lockHandler", {"lockDoor"}));
        handlers.push_back(MakeHandler("wildcardHandler", {"anyOnOff"}));
        handlers.push_back(MakeHandler("levelHandler", {"level"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);

        auto ids = table.GetRegisteredClusterIds();
        EXPECT_EQ(ids.size(), 3u);
        EXPECT_TRUE(ids.count(0x0101));
        EXPECT_TRUE(ids.count(0x0006));
        EXPECT_TRUE(ids.count(0x0008));
    }

    TEST_F(SbmdDispatchTableTest, GetRegisteredClusterIdsClearedAfterClear)
    {
        std::unordered_map<std::string, SbmdAlias> aliases;
        aliases["lockDoor"] = MakeCmdAlias("lockDoor", 0x0101, 0);

        std::vector<SbmdDeviceHandler> handlers;
        handlers.push_back(MakeHandler("handler", {"lockDoor"}));

        SbmdDispatchTable table;
        table.Build(aliases, handlers);
        EXPECT_EQ(table.GetRegisteredClusterIds().size(), 1u);

        table.Clear();
        EXPECT_TRUE(table.GetRegisteredClusterIds().empty());
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
        // Both are single-alias, so stable order (registration order preserved)
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
                driverVersion: 1,
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
            function handleOnOff(args) { return Sbmd.result().success(); }
            function handleLockOp(args) { return Sbmd.result().success(); }
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
                driverVersion: 1,
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
            function fn(args) { return Sbmd.result().success(); }
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
                driverVersion: 1,
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
                return Sbmd.result()
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

    TEST_F(SbmdDispatchDriverTest, CommandDispatchBuiltOnActivation)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "CmdDispatchTest",
                constants: {
                    CL_TEST: 0xFFF10000,
                    CMD_ECHO: 0x00,
                    CMD_PING: 0x01,
                },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0xFFF10000] },
                aliases: {
                    echoCmd: { clusterId: CL_TEST, commandId: CMD_ECHO },
                    pingCmd: { clusterId: CL_TEST, commandId: CMD_PING },
                },
                commandHandlers: {
                    handleEcho: {
                        aliases: ["echoCmd"],
                        handler: onEcho,
                    },
                    handlePing: {
                        aliases: ["pingCmd"],
                        handler: onPing,
                    },
                },
            });
            function onEcho(args) { return Sbmd.result().success(); }
            function onPing(args) { return Sbmd.result().success(); }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        // Command dispatch should match both commands
        auto echoResults = driver->GetCommandDispatch().Lookup(0xFFF10000, 0x00);
        ASSERT_EQ(echoResults.size(), 1u);
        EXPECT_EQ(echoResults[0]->handler->name, "handleEcho");

        auto pingResults = driver->GetCommandDispatch().Lookup(0xFFF10000, 0x01);
        ASSERT_EQ(pingResults.size(), 1u);
        EXPECT_EQ(pingResults[0]->handler->name, "handlePing");

        // Different cluster — no match
        EXPECT_TRUE(driver->GetCommandDispatch().Lookup(0x0006, 0x00).empty());

        // Attribute and event dispatch should be empty
        EXPECT_TRUE(driver->GetAttributeDispatch().Lookup(0xFFF10000, 0x00).empty());
        EXPECT_TRUE(driver->GetEventDispatch().Lookup(0xFFF10000, 0x00).empty());

        // GetRegisteredClusterIds should return the test cluster
        auto clusterIds = driver->GetCommandDispatch().GetRegisteredClusterIds();
        EXPECT_EQ(clusterIds.size(), 1u);
        EXPECT_TRUE(clusterIds.count(0xFFF10000));

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDispatchDriverTest, CommandDispatchClearedOnDeactivation)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "CmdClearTest",
                constants: { CL_TEST: 0xFFF10000, CMD_ECHO: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0xFFF10000] },
                aliases: { echoCmd: { clusterId: CL_TEST, commandId: CMD_ECHO } },
                commandHandlers: {
                    handleEcho: { aliases: ["echoCmd"], handler: fn },
                },
            });
            function fn(args) { return Sbmd.result().success(); }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        EXPECT_FALSE(driver->GetCommandDispatch().Lookup(0xFFF10000, 0).empty());
        EXPECT_FALSE(driver->GetCommandDispatch().GetRegisteredClusterIds().empty());

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }

        EXPECT_TRUE(driver->GetCommandDispatch().Lookup(0xFFF10000, 0).empty());
        EXPECT_TRUE(driver->GetCommandDispatch().GetRegisteredClusterIds().empty());
    }

    TEST_F(SbmdDispatchDriverTest, CommandHandlerInvocation)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "CmdInvokeTest",
                constants: { CL_TEST: 0xFFF10000, CMD_ECHO: 0, EP: "1" },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0xFFF10000] },
                aliases: { echoCmd: { clusterId: CL_TEST, commandId: CMD_ECHO } },
                commandHandlers: {
                    handleEcho: {
                        aliases: ["echoCmd"],
                        handler: handleEchoCmd,
                    },
                },
            });
            function handleEchoCmd(args) {
                return Sbmd.result()
                    .dataModel.updateResource(EP, "lastCommand", "echo")
                    .dataModel.updateResource(EP, "echoData", args.command.tlvBase64 || "empty")
                    .success();
            }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        auto results = driver->GetCommandDispatch().Lookup(0xFFF10000, 0);
        ASSERT_EQ(results.size(), 1u);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

            // Build command args with TLV data and invoke the handler
            HandlerContext hctx;
            hctx.deviceUuid = "test-device";
            hctx.endpointId = "1";
            JSValue args = SbmdHandlerInvoker::BuildCommandArgs(Ctx(), hctx, 0xFFF10000, 0, "AQID");
            auto parsed = SbmdHandlerInvoker::InvokeHandler(Ctx(), results[0]->handler->handler, args);

            ASSERT_TRUE(parsed.has_value());
            ASSERT_EQ(parsed->ops.size(), 2u);

            // First op: updateResource("1", "lastCommand", "echo")
            ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(parsed->ops[0].data));
            auto &op1 = std::get<ResultOp::UpdateResource>(parsed->ops[0].data);
            EXPECT_EQ(*op1.endpoint, "1");
            EXPECT_EQ(op1.resource, "lastCommand");
            EXPECT_EQ(op1.value, "echo");

            // Second op: updateResource("1", "echoData", "AQID")
            ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(parsed->ops[1].data));
            auto &op2 = std::get<ResultOp::UpdateResource>(parsed->ops[1].data);
            EXPECT_EQ(*op2.endpoint, "1");
            EXPECT_EQ(op2.resource, "echoData");
            EXPECT_EQ(op2.value, "AQID");

            ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(parsed->terminal.data));
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDispatchDriverTest, CommandWildcardDispatch)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "CmdWildcardTest",
                constants: { CL_TEST: 0xFFF10000, CMD_ECHO: 0 },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0xFFF10000] },
                aliases: {
                    echoCmd: { clusterId: CL_TEST, commandId: CMD_ECHO },
                    anyTestCmd: { clusterId: CL_TEST },
                },
                commandHandlers: {
                    handleEcho: {
                        aliases: ["echoCmd"],
                        handler: onEcho,
                    },
                    handleAny: {
                        aliases: ["anyTestCmd"],
                        handler: onAny,
                    },
                },
            });
            function onEcho(args) { return Sbmd.result().success(); }
            function onAny(args) {
                return Sbmd.result()
                    .log("wildcard: " + args.command.commandId)
                    .success();
            }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        // CMD_ECHO should match both single and wildcard
        auto echoResults = driver->GetCommandDispatch().Lookup(0xFFF10000, 0);
        ASSERT_EQ(echoResults.size(), 2u);
        EXPECT_EQ(echoResults[0]->handler->name, "handleEcho");
        EXPECT_EQ(echoResults[0]->priority, HandlerPriority::Single);
        EXPECT_EQ(echoResults[1]->handler->name, "handleAny");
        EXPECT_EQ(echoResults[1]->priority, HandlerPriority::Wildcard);

        // Unknown command should still match the wildcard
        auto unknownResults = driver->GetCommandDispatch().Lookup(0xFFF10000, 0xFF);
        ASSERT_EQ(unknownResults.size(), 1u);
        EXPECT_EQ(unknownResults[0]->handler->name, "handleAny");
        EXPECT_EQ(unknownResults[0]->priority, HandlerPriority::Wildcard);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

    TEST_F(SbmdDispatchDriverTest, AllThreeDispatchTables)
    {
        auto driver = CreateDriver(R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "AllTablesTest",
                constants: {
                    CL_ON_OFF: 6,
                    CL_DOOR_LOCK: 257,
                    ATTR_ON_OFF: 0,
                    EVT_LOCK_OP: 2,
                    CMD_LOCK: 0,
                },
                barton: { deviceClass: "test", deviceClassVersion: 0 },
                matter: { deviceTypes: [0x0100] },
                aliases: {
                    onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF, type: "bool" },
                    lockOp: { clusterId: CL_DOOR_LOCK, eventId: EVT_LOCK_OP },
                    lockCmd: { clusterId: CL_DOOR_LOCK, commandId: CMD_LOCK },
                },
                attributeHandlers: {
                    onOffHandler: { aliases: ["onOff"], handler: fn },
                },
                eventHandlers: {
                    lockOpHandler: { aliases: ["lockOp"], handler: fn },
                },
                commandHandlers: {
                    lockCmdHandler: { aliases: ["lockCmd"], handler: fn },
                },
            });
            function fn(args) { return Sbmd.result().success(); }
        )");
        ASSERT_NE(driver, nullptr);

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            EXPECT_TRUE(driver->Activate(Ctx()));
        }

        // All three dispatch tables populated
        EXPECT_EQ(driver->GetAttributeDispatch().Lookup(0x0006, 0x0000).size(), 1u);
        EXPECT_EQ(driver->GetEventDispatch().Lookup(0x0101, 2).size(), 1u);
        EXPECT_EQ(driver->GetCommandDispatch().Lookup(0x0101, 0).size(), 1u);

        // Cross-table: attribute lookup doesn't find commands
        EXPECT_TRUE(driver->GetAttributeDispatch().Lookup(0x0101, 0).empty());
        // Cross-table: command lookup doesn't find attributes
        EXPECT_TRUE(driver->GetCommandDispatch().Lookup(0x0006, 0).empty());

        // Command cluster IDs for registration
        auto cmdClusters = driver->GetCommandDispatch().GetRegisteredClusterIds();
        EXPECT_EQ(cmdClusters.size(), 1u);
        EXPECT_TRUE(cmdClusters.count(0x0101));

        // Clean up
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            driver->Deactivate(Ctx());
        }
    }

} // namespace
