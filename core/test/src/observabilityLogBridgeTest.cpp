//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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

#include <gtest/gtest.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opentelemetry/exporters/memory/in_memory_span_exporter.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/read_write_log_record.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/scope.h>

#include "observability/observabilityLogBridge.h"
#include "observability/observabilityTracing.h"

namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace nostd = opentelemetry::nostd;

/* ------------------------------------------------------------------ */
/* Minimal in-memory log exporter for tests                           */
/* ------------------------------------------------------------------ */

struct CapturedLog
{
    opentelemetry::logs::Severity severity;
    std::string body;
    std::unordered_map<std::string, opentelemetry::common::AttributeValue> attributes;
    opentelemetry::trace::TraceId traceId;
    opentelemetry::trace::SpanId spanId;
};

class InMemoryLogExporter final : public logs_sdk::LogRecordExporter
{
public:
    InMemoryLogExporter(std::shared_ptr<std::vector<CapturedLog>> dest) : dest_(std::move(dest)) {}

    std::unique_ptr<logs_sdk::Recordable> MakeRecordable() noexcept override
    {
        return std::unique_ptr<logs_sdk::Recordable>(new logs_sdk::ReadWriteLogRecord());
    }

    opentelemetry::sdk::common::ExportResult
    Export(const nostd::span<std::unique_ptr<logs_sdk::Recordable>> &records) noexcept override
    {
        for (auto &rec : records)
        {
            auto *lr = static_cast<logs_sdk::ReadWriteLogRecord *>(rec.get());
            if (lr == nullptr)
            {
                continue;
            }

            CapturedLog entry;
            entry.severity = lr->GetSeverity();

            auto &bodyVal = lr->GetBody();
            if (nostd::holds_alternative<const char *>(bodyVal))
            {
                entry.body = nostd::get<const char *>(bodyVal);
            }
            else if (nostd::holds_alternative<nostd::string_view>(bodyVal))
            {
                auto sv = nostd::get<nostd::string_view>(bodyVal);
                entry.body = std::string(sv.data(), sv.size());
            }

            entry.attributes = lr->GetAttributes();
            entry.traceId = lr->GetTraceId();
            entry.spanId = lr->GetSpanId();

            std::lock_guard<std::mutex> lock(mutex_);
            dest_->push_back(std::move(entry));
        }
        return opentelemetry::sdk::common::ExportResult::kSuccess;
    }

    bool Shutdown(std::chrono::microseconds) noexcept override { return true; }

private:
    std::shared_ptr<std::vector<CapturedLog>> dest_;
    std::mutex mutex_;
};

/* ------------------------------------------------------------------ */
/* Test fixture                                                        */
/* ------------------------------------------------------------------ */

class LogBridgeTest : public ::testing::Test
{
protected:
    std::shared_ptr<std::vector<CapturedLog>> captured_;

    void SetUp() override
    {
        captured_ = std::make_shared<std::vector<CapturedLog>>();

        auto exporter = std::unique_ptr<logs_sdk::LogRecordExporter>(new InMemoryLogExporter(captured_));
        auto processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));

        auto provider = std::make_shared<logs_sdk::LoggerProvider>(std::move(processor));
        logs_api::Provider::SetLoggerProvider(nostd::shared_ptr<logs_api::LoggerProvider>(provider));

        observabilityLogBridgeInit();
    }

    void TearDown() override
    {
        observabilityLogBridgeShutdown();
        logs_api::Provider::SetLoggerProvider(
            nostd::shared_ptr<logs_api::LoggerProvider>(new logs_api::NoopLoggerProvider()));
    }
};

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

TEST_F(LogBridgeTest, EmitCreatesLogRecord)
{
    observabilityLogBridgeEmit("testCategory", 2 /* INFO */, "test.c", "myFunc", 42, "hello world");

    ASSERT_EQ(captured_->size(), 1u);
    EXPECT_EQ((*captured_)[0].severity, opentelemetry::logs::Severity::kInfo);
    EXPECT_EQ((*captured_)[0].body, "hello world");
}

TEST_F(LogBridgeTest, SeverityMapping)
{
    observabilityLogBridgeEmit("cat", 0, "f", "fn", 1, "trace"); // TRACE
    observabilityLogBridgeEmit("cat", 1, "f", "fn", 1, "debug"); // DEBUG
    observabilityLogBridgeEmit("cat", 2, "f", "fn", 1, "info");  // INFO
    observabilityLogBridgeEmit("cat", 3, "f", "fn", 1, "warn");  // WARN
    observabilityLogBridgeEmit("cat", 4, "f", "fn", 1, "error"); // ERROR

    ASSERT_EQ(captured_->size(), 5u);
    EXPECT_EQ((*captured_)[0].severity, opentelemetry::logs::Severity::kTrace);
    EXPECT_EQ((*captured_)[1].severity, opentelemetry::logs::Severity::kDebug);
    EXPECT_EQ((*captured_)[2].severity, opentelemetry::logs::Severity::kInfo);
    EXPECT_EQ((*captured_)[3].severity, opentelemetry::logs::Severity::kWarn);
    EXPECT_EQ((*captured_)[4].severity, opentelemetry::logs::Severity::kError);
}

TEST_F(LogBridgeTest, SourceLocationAttributes)
{
    observabilityLogBridgeEmit("cat", 2, "myfile.c", "myFunc", 99, "msg");

    ASSERT_EQ(captured_->size(), 1u);
    auto &attrs = (*captured_)[0].attributes;

    auto itFile = attrs.find("code.filepath");
    ASSERT_NE(itFile, attrs.end());
    EXPECT_STREQ(nostd::get<const char *>(itFile->second), "myfile.c");

    auto itFunc = attrs.find("code.function");
    ASSERT_NE(itFunc, attrs.end());
    EXPECT_STREQ(nostd::get<const char *>(itFunc->second), "myFunc");

    auto itLine = attrs.find("code.lineno");
    ASSERT_NE(itLine, attrs.end());
    EXPECT_EQ(nostd::get<int64_t>(itLine->second), 99);
}

TEST_F(LogBridgeTest, NullFieldsAreSafe)
{
    observabilityLogBridgeEmit(nullptr, 2, nullptr, nullptr, 0, nullptr);

    ASSERT_EQ(captured_->size(), 1u);
    EXPECT_EQ((*captured_)[0].severity, opentelemetry::logs::Severity::kInfo);
}

TEST_F(LogBridgeTest, EmitBeforeInitIsNoOp)
{
    observabilityLogBridgeShutdown(); // undo SetUp's init
    observabilityLogBridgeEmit("cat", 2, "f", "fn", 1, "should not appear");
    EXPECT_EQ(captured_->size(), 0u);
    observabilityLogBridgeInit(); // re-init so TearDown is balanced
}

/* ------------------------------------------------------------------ */
/* Task 5.4: Trace-log correlation                                     */
/* ------------------------------------------------------------------ */

class LogBridgeCorrelationTest : public LogBridgeTest
{
protected:
    std::shared_ptr<opentelemetry::exporter::memory::InMemorySpanExporter> spanExporter_;

    void SetUp() override
    {
        LogBridgeTest::SetUp();

        // Set up a tracer provider so we get real trace/span IDs
        spanExporter_ = std::make_shared<opentelemetry::exporter::memory::InMemorySpanExporter>();
        // InMemorySpanExporter must be kept alive; pass a non-owning unique_ptr to the processor
        auto exporterPtr =
            std::unique_ptr<trace_sdk::SpanExporter>(new opentelemetry::exporter::memory::InMemorySpanExporter());
        auto tracerProcessor =
            std::unique_ptr<trace_sdk::SpanProcessor>(new trace_sdk::SimpleSpanProcessor(std::move(exporterPtr)));
        auto tracerProvider =
            nostd::shared_ptr<trace_api::TracerProvider>(new trace_sdk::TracerProvider(std::move(tracerProcessor)));
        trace_api::Provider::SetTracerProvider(tracerProvider);
    }

    void TearDown() override
    {
        trace_api::Provider::SetTracerProvider(
            nostd::shared_ptr<trace_api::TracerProvider>(new trace_api::NoopTracerProvider()));
        LogBridgeTest::TearDown();
    }
};

TEST_F(LogBridgeCorrelationTest, LogWithinSpanHasTraceContext)
{
    // Create a span and make it current
    auto tracer = trace_api::Provider::GetTracerProvider()->GetTracer("test");
    auto span = tracer->StartSpan("test-span");
    auto scope = trace_api::Scope(span);

    auto expectedTraceId = span->GetContext().trace_id();
    auto expectedSpanId = span->GetContext().span_id();

    // Emit a log while the span is active
    observabilityLogBridgeEmit("cat", 2, "f", "fn", 1, "correlated log");

    span->End();

    ASSERT_EQ(captured_->size(), 1u);
    EXPECT_EQ((*captured_)[0].traceId, expectedTraceId);
    EXPECT_EQ((*captured_)[0].spanId, expectedSpanId);
    // Trace ID should be valid (non-zero)
    EXPECT_TRUE(expectedTraceId.IsValid());
    EXPECT_TRUE(expectedSpanId.IsValid());
}

TEST_F(LogBridgeCorrelationTest, LogWithoutSpanHasInvalidTraceContext)
{
    // No active span - emit a log
    observabilityLogBridgeEmit("cat", 2, "f", "fn", 1, "no span log");

    ASSERT_EQ(captured_->size(), 1u);
    EXPECT_FALSE((*captured_)[0].traceId.IsValid());
    EXPECT_FALSE((*captured_)[0].spanId.IsValid());
}
