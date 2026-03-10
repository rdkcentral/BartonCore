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
