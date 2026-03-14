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

#include <thread>

#include <opentelemetry/exporters/memory/in_memory_span_exporter.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>

#include "observability/observabilityTracing.h"

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace memory = opentelemetry::exporter::memory;

class TracingTest : public ::testing::Test
{
protected:
    std::shared_ptr<memory::InMemorySpanData> spanData;

    void SetUp() override
    {
        auto exporter = std::make_unique<memory::InMemorySpanExporter>();
        spanData = exporter->GetData();
        auto processor = std::make_unique<trace_sdk::SimpleSpanProcessor>(std::move(exporter));
        auto provider = opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
            new trace_sdk::TracerProvider(std::move(processor)));
        trace_api::Provider::SetTracerProvider(provider);
    }

    void TearDown() override
    {
        trace_api::Provider::SetTracerProvider(
            opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(new trace_api::NoopTracerProvider()));
    }
};

TEST_F(TracingTest, CreateAndEndSpan)
{
    ObservabilitySpan *span = observabilitySpanStart("test.span");
    ASSERT_NE(span, nullptr);
    observabilitySpanRelease(span);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0]->GetName(), "test.span");
}

TEST_F(TracingTest, NullNameReturnsNull)
{
    ObservabilitySpan *span = observabilitySpanStart(nullptr);
    EXPECT_EQ(span, nullptr);
}

TEST_F(TracingTest, SpanAttributes)
{
    ObservabilitySpan *span = observabilitySpanStart("test.attrs");
    observabilitySpanSetAttribute(span, "key.str", "value");
    observabilitySpanSetAttributeInt(span, "key.int", 42);
    observabilitySpanRelease(span);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 1u);

    auto &attrs = spans[0]->GetAttributes();
    auto it = attrs.find("key.str");
    ASSERT_NE(it, attrs.end());
    EXPECT_EQ(opentelemetry::nostd::get<std::string>(it->second), "value");

    auto itInt = attrs.find("key.int");
    ASSERT_NE(itInt, attrs.end());
    EXPECT_EQ(opentelemetry::nostd::get<int64_t>(itInt->second), 42);
}

TEST_F(TracingTest, SpanError)
{
    ObservabilitySpan *span = observabilitySpanStart("test.error");
    observabilitySpanSetError(span, "something failed");
    observabilitySpanRelease(span);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0]->GetStatus(), trace_api::StatusCode::kError);
}

TEST_F(TracingTest, ChildSpanWithParentContext)
{
    ObservabilitySpan *parent = observabilitySpanStart("parent.span");
    ObservabilitySpanContext *ctx = observabilitySpanGetContext(parent);
    ASSERT_NE(ctx, nullptr);

    ObservabilitySpan *child = observabilitySpanStartWithParent("child.span", ctx);
    ASSERT_NE(child, nullptr);

    observabilitySpanRelease(child);
    observabilitySpanRelease(parent);
    observabilitySpanContextRelease(ctx);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 2u);

    // Both spans should share the same trace ID
    EXPECT_EQ(spans[0]->GetTraceId(), spans[1]->GetTraceId());
}

TEST_F(TracingTest, ChildSpanWithNullParent)
{
    ObservabilitySpan *span = observabilitySpanStartWithParent("orphan.span", nullptr);
    ASSERT_NE(span, nullptr);
    observabilitySpanRelease(span);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0]->GetName(), "orphan.span");
}

TEST_F(TracingTest, ReleaseNullSpanIsSafe)
{
    observabilitySpanRelease(nullptr);
    // Should not crash
}

TEST_F(TracingTest, SetAttributeOnNullSpanIsSafe)
{
    observabilitySpanSetAttribute(nullptr, "key", "val");
    observabilitySpanSetAttributeInt(nullptr, "key", 1);
    observabilitySpanSetError(nullptr, "err");
    // Should not crash
}

TEST_F(TracingTest, SpanContextRefRelease)
{
    ObservabilitySpan *span = observabilitySpanStart("ref.test");
    ObservabilitySpanContext *ctx = observabilitySpanGetContext(span);
    ASSERT_NE(ctx, nullptr);

    // Initial ref count is 1. Ref bumps to 2, then 3.
    observabilitySpanContextRef(ctx);
    observabilitySpanContextRef(ctx);

    // Release 3 → 2 → 1 → 0 (freed)
    observabilitySpanContextRelease(ctx);
    observabilitySpanContextRelease(ctx);
    observabilitySpanContextRelease(ctx);

    // Ref/Release on null must not crash
    observabilitySpanContextRef(nullptr);
    observabilitySpanContextRelease(nullptr);

    observabilitySpanRelease(span);
}

TEST_F(TracingTest, TlsGetCurrentReturnsNullByDefault)
{
    // Fresh TLS should be null (or cleared from prior tests)
    observabilitySpanContextSetCurrent(nullptr);
    EXPECT_EQ(observabilitySpanContextGetCurrent(), nullptr);
}

TEST_F(TracingTest, TlsSetCurrentAndClear)
{
    ObservabilitySpan *span = observabilitySpanStart("tls.test");
    ObservabilitySpanContext *ctx = observabilitySpanGetContext(span);
    ASSERT_NE(ctx, nullptr);

    observabilitySpanContextSetCurrent(ctx);
    EXPECT_EQ(observabilitySpanContextGetCurrent(), ctx);

    // Setting same value again should be a no-op (no double-release)
    observabilitySpanContextSetCurrent(ctx);
    EXPECT_EQ(observabilitySpanContextGetCurrent(), ctx);

    // Clear
    observabilitySpanContextSetCurrent(nullptr);
    EXPECT_EQ(observabilitySpanContextGetCurrent(), nullptr);

    observabilitySpanContextRelease(ctx);
    observabilitySpanRelease(span);
}

TEST_F(TracingTest, TlsIsolationAcrossThreads)
{
    ObservabilitySpan *span = observabilitySpanStart("tls.isolation");
    ObservabilitySpanContext *ctx = observabilitySpanGetContext(span);
    ASSERT_NE(ctx, nullptr);

    observabilitySpanContextSetCurrent(ctx);
    EXPECT_EQ(observabilitySpanContextGetCurrent(), ctx);

    ObservabilitySpanContext *otherThreadResult = nullptr;
    bool otherThreadRan = false;

    std::thread t([&otherThreadResult, &otherThreadRan]() {
        // Fresh thread should see null
        otherThreadResult = observabilitySpanContextGetCurrent();
        otherThreadRan = true;
    });
    t.join();

    EXPECT_TRUE(otherThreadRan);
    EXPECT_EQ(otherThreadResult, nullptr);

    // Main thread still has its context
    EXPECT_EQ(observabilitySpanContextGetCurrent(), ctx);

    observabilitySpanContextSetCurrent(nullptr);
    observabilitySpanContextRelease(ctx);
    observabilitySpanRelease(span);
}

TEST_F(TracingTest, ChildSpanInheritsTlsParent)
{
    // Simulate the device lifecycle pattern: root span → set TLS → child inherits
    ObservabilitySpan *root = observabilitySpanStart("root.op");
    ObservabilitySpanContext *rootCtx = observabilitySpanGetContext(root);
    observabilitySpanContextSetCurrent(rootCtx);

    // Child created with TLS parent — same pattern as device.found
    ObservabilitySpan *child = observabilitySpanStartWithParent("child.op", observabilitySpanContextGetCurrent());
    ASSERT_NE(child, nullptr);

    observabilitySpanRelease(child);
    observabilitySpanRelease(root);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 2u);
    // Both should share the same trace ID
    EXPECT_EQ(spans[0]->GetTraceId(), spans[1]->GetTraceId());

    observabilitySpanContextSetCurrent(nullptr);
    observabilitySpanContextRelease(rootCtx);
}

TEST_F(TracingTest, TlsContextPropagatesAcrossThreads)
{
    // Simulate the commission pattern: main thread creates span + context,
    // thread receives context via args, sets TLS, creates child span
    ObservabilitySpan *root = observabilitySpanStart("device.commission");
    ObservabilitySpanContext *rootCtx = observabilitySpanGetContext(root);
    ASSERT_NE(rootCtx, nullptr);
    observabilitySpanContextRef(rootCtx);

    bool childCreated = false;
    std::thread t([&spanData = this->spanData, rootCtx, &childCreated]() {
        observabilitySpanContextSetCurrent(rootCtx);
        observabilitySpanContextRelease(rootCtx);

        ObservabilitySpan *child =
            observabilitySpanStartWithParent("device.found", observabilitySpanContextGetCurrent());
        ASSERT_NE(child, nullptr);
        observabilitySpanRelease(child);
        childCreated = true;

        observabilitySpanContextSetCurrent(nullptr);
    });
    t.join();

    observabilitySpanRelease(root);
    observabilitySpanContextRelease(rootCtx);

    EXPECT_TRUE(childCreated);
    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0]->GetTraceId(), spans[1]->GetTraceId());
}

TEST_F(TracingTest, SpanStartWithLinkCreatesRootSpan)
{
    // Create an "origin" span to link from
    ObservabilitySpan *origin = observabilitySpanStart("subsystem.init");
    ObservabilitySpanContext *originCtx = observabilitySpanGetContext(origin);
    ASSERT_NE(originCtx, nullptr);

    // Create linked root span — should NOT be a child of origin
    ObservabilitySpan *linked = observabilitySpanStartWithLink("matter.init", originCtx);
    ASSERT_NE(linked, nullptr);

    observabilitySpanRelease(linked);
    observabilitySpanRelease(origin);
    observabilitySpanContextRelease(originCtx);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 2u);

    // The linked span must have a DIFFERENT trace ID (it's a root, not a child)
    EXPECT_NE(spans[0]->GetTraceId(), spans[1]->GetTraceId());

    // The linked span should carry a link referencing the origin span's context
    auto &linkedSpan = spans[0]; // InMemorySpanExporter records in end-order; linked ended first
    ASSERT_EQ(linkedSpan->GetLinks().size(), 1u);
}

TEST_F(TracingTest, SpanStartWithLinkNullLinkedCreatesPlainRoot)
{
    ObservabilitySpan *span = observabilitySpanStartWithLink("matter.init", nullptr);
    ASSERT_NE(span, nullptr);
    observabilitySpanRelease(span);

    auto spans = spanData->GetSpans();
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0]->GetName(), "matter.init");
    EXPECT_TRUE(spans[0]->GetLinks().empty());
}
