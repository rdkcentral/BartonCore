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
 * Created by Raiyan Chowdhury on 7/17/2026
 *
 * Unit tests for SBMD observability instrumentation.
 *
 * These tests verify that metrics are populated by the instrumented code paths.
 * The in-memory backend is required to inspect metric values via
 * observabilityDumpJson(). The test binary is only built when
 * BCORE_OBSERVABILITY_BACKEND is "memory".
 */

#include "deviceDrivers/matter/sbmd/SbmdDriver.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"

#include <filesystem>
#include <fstream>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

extern "C" {
#include <cjson/cJSON.h>
#include <mquickjs/mquickjs.h>

// Observability memory backend
#include "observability/observability.h"
#include "observability/observabilityMetrics.h"

// Stubs for C APIs referenced by SbmdHandlerInvoker
void updateResource(const char *, const char *, const char *, const char *, void *) {}

void setMetadata(const char *, const char *, const char *, const char *) {}

bool deviceServiceSetMetadata(const char *, const char *)
{
    return true;
}
}

using namespace barton;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    using CJsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

    // Parse observabilityDumpJson() and return the cJSON object for a named metric.
    CJsonPtr GetMetricJson(const std::string &name)
    {
        std::unique_ptr<char, decltype(&free)> raw(observabilityDumpJson(), free);

        if (!raw)
        {
            return CJsonPtr(nullptr, cJSON_Delete);
        }

        CJsonPtr root(cJSON_Parse(raw.get()), cJSON_Delete);

        if (!root)
        {
            return CJsonPtr(nullptr, cJSON_Delete);
        }

        // JSON structure: { "metrics": { "metric.name": { ... } } }
        cJSON *metrics = cJSON_GetObjectItemCaseSensitive(root.get(), "metrics");
        cJSON *entry = nullptr;

        if (metrics)
        {
            cJSON *item = cJSON_GetObjectItemCaseSensitive(metrics, name.c_str());

            if (item)
            {
                entry = cJSON_Duplicate(item, 1);
            }
        }

        return CJsonPtr(entry, cJSON_Delete);
    }

    // Return the sum of all data_points[].sum values for a given metric.
    double GetHistogramSum(const std::string &metricName)
    {
        auto metric = GetMetricJson(metricName);

        if (!metric)
        {
            return 0.0;
        }

        double total = 0.0;
        cJSON *points = cJSON_GetObjectItemCaseSensitive(metric.get(), "dataPoints");

        if (cJSON_IsArray(points))
        {
            cJSON *pt = nullptr;

            cJSON_ArrayForEach(pt, points)
            {
                cJSON *s = cJSON_GetObjectItemCaseSensitive(pt, "sum");

                if (cJSON_IsNumber(s))
                {
                    total += s->valuedouble;
                }
            }
        }

        return total;
    }

    // Return the total count of observations across all data_points for a histogram.
    int64_t GetHistogramCount(const std::string &metricName)
    {
        auto metric = GetMetricJson(metricName);

        if (!metric)
        {
            return 0;
        }

        int64_t total = 0;
        cJSON *points = cJSON_GetObjectItemCaseSensitive(metric.get(), "dataPoints");

        if (cJSON_IsArray(points))
        {
            cJSON *pt = nullptr;

            cJSON_ArrayForEach(pt, points)
            {
                cJSON *c = cJSON_GetObjectItemCaseSensitive(pt, "count");

                if (cJSON_IsNumber(c))
                {
                    total += static_cast<int64_t>(c->valuedouble);
                }
            }
        }

        return total;
    }

    // Return the sum of all data_points[].value values for a gauge.
    double GetGaugeValue(const std::string &metricName)
    {
        auto metric = GetMetricJson(metricName);

        if (!metric)
        {
            return 0.0;
        }

        double total = 0.0;
        cJSON *points = cJSON_GetObjectItemCaseSensitive(metric.get(), "dataPoints");

        if (cJSON_IsArray(points))
        {
            cJSON *pt = nullptr;

            cJSON_ArrayForEach(pt, points)
            {
                cJSON *v = cJSON_GetObjectItemCaseSensitive(pt, "value");

                if (cJSON_IsNumber(v))
                {
                    total += v->valuedouble;
                }
            }
        }

        return total;
    }

    // Return the total count of all data_points[].value increments for a counter.
    double GetCounterValue(const std::string &metricName)
    {
        auto metric = GetMetricJson(metricName);

        if (!metric)
        {
            return 0.0;
        }

        double total = 0.0;
        cJSON *points = cJSON_GetObjectItemCaseSensitive(metric.get(), "dataPoints");

        if (cJSON_IsArray(points))
        {
            cJSON *pt = nullptr;

            cJSON_ArrayForEach(pt, points)
            {
                cJSON *v = cJSON_GetObjectItemCaseSensitive(pt, "value");

                if (cJSON_IsNumber(v))
                {
                    total += v->valuedouble;
                }
            }
        }

        return total;
    }

    // Return the "value" of the first counter datapoint that matches ALL of
    // the supplied attribute key=value pairs.  Returns -1.0 if not found.
    double GetCounterDatapointValue(const std::string &metricName,
                                    std::initializer_list<std::pair<const char *, const char *>> attrs)
    {
        auto metric = GetMetricJson(metricName);

        if (!metric)
        {
            return -1.0;
        }

        cJSON *points = cJSON_GetObjectItemCaseSensitive(metric.get(), "dataPoints");

        if (!cJSON_IsArray(points))
        {
            return -1.0;
        }

        cJSON *pt = nullptr;

        cJSON_ArrayForEach(pt, points)
        {
            cJSON *dpAttrs = cJSON_GetObjectItemCaseSensitive(pt, "attributes");

            if (!cJSON_IsObject(dpAttrs))
            {
                continue;
            }

            bool allMatch = true;

            for (auto [k, v] : attrs)
            {
                cJSON *av = cJSON_GetObjectItemCaseSensitive(dpAttrs, k);

                if (!av || !cJSON_IsString(av) || std::string(av->valuestring) != v)
                {
                    allMatch = false;
                    break;
                }
            }

            if (allMatch)
            {
                cJSON *val = cJSON_GetObjectItemCaseSensitive(pt, "value");

                return cJSON_IsNumber(val) ? val->valuedouble : -1.0;
            }
        }

        return -1.0;
    }

    // A minimal valid SBMD driver script that produces a success terminal.
    static constexpr const char *kMinimalDriver = R"(
        function readTestResource(args) { return Sbmd.result().success(); }
        function writeTestResource(args) { return Sbmd.result().success(); }

        SbmdDriver({
            schemaVersion: "4.0",
            driverVersion: 1,
            name: "test-driver",
            barton: { deviceClass: "light", deviceClassVersion: 1 },
            matter: { deviceTypes: [0xFFF10001] },
            aliases: {},
            endpoints: {
                "1": {
                    profile: "light",
                    profileVersion: 1,
                    resources: {
                        "test.resource": {
                            type: "string",
                            modes: ["read", "write"],
                            read: readTestResource,
                            write: writeTestResource,
                        },
                    },
                },
            },
        });
    )";

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class SbmdObservabilityTest : public ::testing::Test
{
public:
    static void SetUpTestSuite()
    {
        // Initialize observability backend first
        observabilityInit();

        // Initialize metrics BEFORE the JS runtime (per task 1.2 ordering)
        MQuickJsRuntime::InitializeMetrics();

        ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));

        auto *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            ASSERT_TRUE(SbmdLoader::InjectCaptureFunction(ctx));
        }

        SbmdHandlerInvoker::InitializeMetrics();

        // Load the test driver
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto reg = SbmdLoader::LoadDriver(ctx, "test-driver.sbmd.js", kMinimalDriver, strlen(kMinimalDriver));
            ASSERT_NE(reg, nullptr);
            testDriver = std::make_unique<SbmdDriver>(std::move(reg), kMinimalDriver);
            ASSERT_TRUE(testDriver->Activate(ctx));
        }
    }

    static void TearDownTestSuite()
    {
        testDriver.reset();

        MQuickJsRuntime::Shutdown();
        SbmdHandlerInvoker::ShutdownMetrics();
        MQuickJsRuntime::ShutdownMetrics();
        observabilityShutdown();
    }

protected:
    static std::unique_ptr<SbmdDriver> testDriver;

    // Helper to invoke the read handler on "test.resource" and return the result.
    std::optional<ParsedResult> InvokeRead()
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        const auto &reg = testDriver->GetRegistration();
        const SbmdResource *res = nullptr;

        for (const auto &endpoint : reg.endpoints)
        {
            for (const auto &r : endpoint.resources)
            {
                if (r.id == "test.resource")
                {
                    res = &r;
                    break;
                }
            }

            if (res)
            {
                break;
            }
        }

        if (!res || !res->read)
        {
            return std::nullopt;
        }

        HandlerContext hctx;
        hctx.deviceUuid = "test-device";
        hctx.endpointId = "1";

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, "test.resource", std::nullopt);

        OperationContext opCtx;
        opCtx.driverName = "test-driver";
        opCtx.opType = "read";
        opCtx.resourceId = "test.resource";
        opCtx.startTime = std::chrono::steady_clock::now();

        return SbmdHandlerInvoker::InvokeHandler(ctx, res->read->Fn(), args, &opCtx);
    }
};

std::unique_ptr<SbmdDriver> SbmdObservabilityTest::testDriver;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(SbmdObservabilityTest, ArenaSizeGaugeRecordedAtInit)
{
    double arena = GetGaugeValue("sbmd.js.heap.arena_bytes");
    EXPECT_DOUBLE_EQ(arena, 512.0 * 1024.0);
}

TEST_F(SbmdObservabilityTest, ForceSnapshotPopulatesHeapHistogram)
{
    int64_t countBefore = GetHistogramCount("sbmd.js.heap.used_bytes");
    MQuickJsRuntime::ForceSnapshot();
    int64_t countAfter = GetHistogramCount("sbmd.js.heap.used_bytes");

    EXPECT_GT(countAfter, countBefore);
}

TEST_F(SbmdObservabilityTest, HandlerDurationHistogramPopulated)
{
    int64_t countBefore = GetHistogramCount("sbmd.handler.duration_ms");
    auto result = InvokeRead();
    ASSERT_TRUE(result.has_value());

    int64_t countAfter = GetHistogramCount("sbmd.handler.duration_ms");
    EXPECT_GT(countAfter, countBefore);
    EXPECT_GE(GetHistogramSum("sbmd.handler.duration_ms"), 0.0);
}

TEST_F(SbmdObservabilityTest, HeapDeltaHistogramPopulated)
{
    int64_t countBefore = GetHistogramCount("sbmd.handler.heap_delta_bytes");
    auto result = InvokeRead();
    ASSERT_TRUE(result.has_value());

    int64_t countAfter = GetHistogramCount("sbmd.handler.heap_delta_bytes");
    EXPECT_GT(countAfter, countBefore);
}

TEST_F(SbmdObservabilityTest, SuccessOutcomeCounterIncrements)
{
    double beforeTotal = GetCounterValue("sbmd.handler.outcome");
    double beforeAttrib = GetCounterDatapointValue(
        "sbmd.handler.outcome",
        {
            {     "driver",   "test-driver"},
            {    "op_type",          "read"},
            {"resource_id", "test.resource"},
            {    "outcome",       "success"}
    });

    auto result = InvokeRead();
    ASSERT_TRUE(result.has_value());

    double afterTotal = GetCounterValue("sbmd.handler.outcome");
    EXPECT_GT(afterTotal, beforeTotal);

    double afterAttrib = GetCounterDatapointValue(
        "sbmd.handler.outcome",
        {
            {     "driver",   "test-driver"},
            {    "op_type",          "read"},
            {"resource_id", "test.resource"},
            {    "outcome",       "success"}
    });
    EXPECT_GT(afterAttrib, std::max(beforeAttrib, 0.0));
}

TEST_F(SbmdObservabilityTest, ExceptionCounterIncrementsOnThrow)
{
    double beforeTotal = GetCounterValue("sbmd.handler.outcome");
    double beforeAttrib =
        GetCounterDatapointValue("sbmd.handler.outcome",
                                 {
                                     { "driver", "test-driver"},
                                     {"op_type",        "read"},
                                     {"outcome",   "exception"}
    });

    // Load a driver that throws and invoke it
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    // Evaluate a throwing JS function to simulate an exception
    const char *throwScript = "(function(args) { throw new Error('test throw'); })";
    SafeJSValue fn(ctx, JS_Eval(ctx, throwScript, strlen(throwScript), "<test>", JS_EVAL_RETVAL));
    ASSERT_FALSE(JS_IsException(fn.Get()));

    HandlerContext hctx;
    hctx.deviceUuid = "test-device";
    hctx.endpointId = "1";
    SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, "test.resource", std::nullopt);

    OperationContext opCtx;
    opCtx.driverName = "test-driver";
    opCtx.opType = "read";
    opCtx.startTime = std::chrono::steady_clock::now();

    auto result = SbmdHandlerInvoker::InvokeHandler(ctx, fn.Get(), args, &opCtx);
    // fn goes out of scope and SafeJSValue releases the reference

    // Should have returned nullopt (exception path)
    EXPECT_FALSE(result.has_value());

    double afterTotal = GetCounterValue("sbmd.handler.outcome");
    EXPECT_GT(afterTotal, beforeTotal);

    double afterAttrib =
        GetCounterDatapointValue("sbmd.handler.outcome",
                                 {
                                     { "driver", "test-driver"},
                                     {"op_type",        "read"},
                                     {"outcome",   "exception"}
    });
    EXPECT_GT(afterAttrib, std::max(beforeAttrib, 0.0));
}

// Error outcome counter increments when handler returns Sbmd.result().error(), attributed correctly
TEST_F(SbmdObservabilityTest, ErrorOutcomeCounterIncrements)
{
    double beforeTotal = GetCounterValue("sbmd.handler.outcome");
    double beforeAttrib = GetCounterDatapointValue(
        "sbmd.handler.outcome",
        {
            {     "driver",   "test-driver"},
            {    "op_type",          "read"},
            {"resource_id", "test.resource"},
            {    "outcome",         "error"}
    });

    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    // A handler that returns an error terminal via the Sbmd result API
    const char *errorScript = "(function(args) { return Sbmd.result().error('test error'); })";
    SafeJSValue fn(ctx, JS_Eval(ctx, errorScript, strlen(errorScript), "<test>", JS_EVAL_RETVAL));
    ASSERT_FALSE(JS_IsException(fn.Get()));

    HandlerContext hctx;
    hctx.deviceUuid = "test-device";
    hctx.endpointId = "1";
    SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, "test.resource", std::nullopt);

    OperationContext opCtx;
    opCtx.driverName = "test-driver";
    opCtx.opType = "read";
    opCtx.resourceId = "test.resource";
    opCtx.startTime = std::chrono::steady_clock::now();

    auto result = SbmdHandlerInvoker::InvokeHandler(ctx, fn.Get(), args, &opCtx);

    // Error terminal: InvokeHandler returns a parsed result (it IS a valid terminal)
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ResultTerminal::Error>(result->terminal.data));

    double afterTotal = GetCounterValue("sbmd.handler.outcome");
    EXPECT_GT(afterTotal, beforeTotal);

    double afterAttrib = GetCounterDatapointValue(
        "sbmd.handler.outcome",
        {
            {     "driver",   "test-driver"},
            {    "op_type",          "read"},
            {"resource_id", "test.resource"},
            {    "outcome",         "error"}
    });
    EXPECT_GT(afterAttrib, std::max(beforeAttrib, 0.0));
}

TEST_F(SbmdObservabilityTest, LoadingPhaseExceptionCounterIncrements)
{
    double countBefore = GetCounterValue("sbmd.js.exception");

    // Attempt to load invalid JS
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();
    const char *badScript = "this is not valid javascript }{][";
    auto reg = SbmdLoader::LoadDriver(ctx, "bad.sbmd.js", badScript, strlen(badScript));

    // Should fail
    EXPECT_EQ(reg, nullptr);

    double countAfter = GetCounterValue("sbmd.js.exception");
    EXPECT_GT(countAfter, countBefore);
}

// Hold the JS mutex on a background thread to create real contention, then
// measure elapsed wait on the main thread and call RecordMutexWait directly
// (same pattern used in production code).
TEST_F(SbmdObservabilityTest, MutexWaitHistogramPopulated)
{
    int64_t countBefore = GetHistogramCount("sbmd.js.mutex.wait_ms");

    // Hold the mutex on a background thread for a short but measurable time.
    std::promise<void> holdingMutex;
    std::promise<void> releaseSignal;

    std::thread holder([&]() {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        holdingMutex.set_value();          // signal that the lock is held
        releaseSignal.get_future().wait(); // wait for main thread to release us
    });

    // Wait until the background thread holds the lock, then measure contention.
    holdingMutex.get_future().wait();

    auto t0 = std::chrono::steady_clock::now();
    releaseSignal.set_value(); // release the background thread

    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        double waitMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        MQuickJsRuntime::RecordMutexWait(waitMs);
    }

    holder.join();

    int64_t countAfter = GetHistogramCount("sbmd.js.mutex.wait_ms");
    EXPECT_GT(countAfter, countBefore);
}

TEST_F(SbmdObservabilityTest, GcCountIncrements)
{
    double countBefore = GetCounterValue("sbmd.js.gc.count");

    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_NE(ctx, nullptr);
        JS_GC(ctx);
    }

    double countAfter = GetCounterValue("sbmd.js.gc.count");
    EXPECT_DOUBLE_EQ(countAfter, countBefore + 1.0);
}

TEST_F(SbmdObservabilityTest, GcRootsGaugeHasValue)
{
    MQuickJsRuntime::ForceSnapshot();
    double roots = GetGaugeValue("sbmd.js.gc_roots");
    EXPECT_GT(roots, 0.0);
}
