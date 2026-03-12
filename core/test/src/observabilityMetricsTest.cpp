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

#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>

#include "observability/observabilityMetrics.h"

namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;

class MetricsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto provider = opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(new metrics_sdk::MeterProvider());
        metrics_api::Provider::SetMeterProvider(provider);
    }

    void TearDown() override
    {
        metrics_api::Provider::SetMeterProvider(
            opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(new metrics_api::NoopMeterProvider()));
    }
};

TEST_F(MetricsTest, CreateCounter)
{
    g_autoptr(ObservabilityCounter) counter = observabilityCounterCreate("test.counter", "A test counter", "{count}");
    ASSERT_NE(counter, nullptr);
    // Should not crash
    observabilityCounterAdd(counter, 1);
    observabilityCounterAdd(counter, 5);
}

TEST_F(MetricsTest, CreateCounterNullName)
{
    ObservabilityCounter *counter = observabilityCounterCreate(nullptr, "desc", "unit");
    EXPECT_EQ(counter, nullptr);
}

TEST_F(MetricsTest, CreateGauge)
{
    g_autoptr(ObservabilityGauge) gauge = observabilityGaugeCreate("test.gauge", "A test gauge", "{device}");
    ASSERT_NE(gauge, nullptr);
    observabilityGaugeRecord(gauge, 10);
    observabilityGaugeRecord(gauge, 20);
    observabilityGaugeRecord(gauge, 5);
}

TEST_F(MetricsTest, CreateGaugeNullName)
{
    ObservabilityGauge *gauge = observabilityGaugeCreate(nullptr, "desc", "unit");
    EXPECT_EQ(gauge, nullptr);
}

TEST_F(MetricsTest, AddToNullCounterIsSafe)
{
    observabilityCounterAdd(nullptr, 1);
    // Should not crash
}

TEST_F(MetricsTest, RecordNullGaugeIsSafe)
{
    observabilityGaugeRecord(nullptr, 1);
    // Should not crash
}

TEST_F(MetricsTest, CounterWithNullOptionalArgs)
{
    g_autoptr(ObservabilityCounter) counter = observabilityCounterCreate("test.counter2", nullptr, nullptr);
    ASSERT_NE(counter, nullptr);
    observabilityCounterAdd(counter, 1);
}

TEST_F(MetricsTest, GaugeWithNullOptionalArgs)
{
    g_autoptr(ObservabilityGauge) gauge = observabilityGaugeCreate("test.gauge2", nullptr, nullptr);
    ASSERT_NE(gauge, nullptr);
    observabilityGaugeRecord(gauge, 42);
}

TEST_F(MetricsTest, CounterAddWithAttrs)
{
    g_autoptr(ObservabilityCounter) counter = observabilityCounterCreate("test.counter.attrs", "desc", "{count}");
    ASSERT_NE(counter, nullptr);
    observabilityCounterAddWithAttrs(counter, 1, "subsystem", "zigbee", NULL);
    observabilityCounterAddWithAttrs(counter, 5, "subsystem", "matter", "device", "bulb", NULL);
}

TEST_F(MetricsTest, CounterAddWithAttrsEmpty)
{
    g_autoptr(ObservabilityCounter) counter = observabilityCounterCreate("test.counter.noattrs", "desc", "{count}");
    ASSERT_NE(counter, nullptr);
    // NULL terminates immediately — no attributes
    observabilityCounterAddWithAttrs(counter, 3, NULL);
}

TEST_F(MetricsTest, CounterAddWithAttrsNullCounter)
{
    // Should not crash
    observabilityCounterAddWithAttrs(nullptr, 1, "key", "val", NULL);
}

TEST_F(MetricsTest, GaugeRecordWithAttrs)
{
    g_autoptr(ObservabilityGauge) gauge = observabilityGaugeCreate("test.gauge.attrs", "desc", "{device}");
    ASSERT_NE(gauge, nullptr);
    observabilityGaugeRecordWithAttrs(gauge, 10, "subsystem", "zigbee", NULL);
    observabilityGaugeRecordWithAttrs(gauge, 20, "subsystem", "matter", "status", "online", NULL);
}

TEST_F(MetricsTest, GaugeRecordWithAttrsEmpty)
{
    g_autoptr(ObservabilityGauge) gauge = observabilityGaugeCreate("test.gauge.noattrs", "desc", "{device}");
    ASSERT_NE(gauge, nullptr);
    observabilityGaugeRecordWithAttrs(gauge, 7, NULL);
}

TEST_F(MetricsTest, GaugeRecordWithAttrsNullGauge)
{
    // Should not crash
    observabilityGaugeRecordWithAttrs(nullptr, 1, "key", "val", NULL);
}

TEST_F(MetricsTest, CreateHistogram)
{
    g_autoptr(ObservabilityHistogram) histogram =
        observabilityHistogramCreate("test.histogram", "A test histogram", "ms");
    ASSERT_NE(histogram, nullptr);
    observabilityHistogramRecord(histogram, 1.5);
    observabilityHistogramRecord(histogram, 100.0);
}

TEST_F(MetricsTest, CreateHistogramNullName)
{
    ObservabilityHistogram *histogram = observabilityHistogramCreate(nullptr, "desc", "unit");
    EXPECT_EQ(histogram, nullptr);
}

TEST_F(MetricsTest, HistogramWithNullOptionalArgs)
{
    g_autoptr(ObservabilityHistogram) histogram =
        observabilityHistogramCreate("test.histogram2", nullptr, nullptr);
    ASSERT_NE(histogram, nullptr);
    observabilityHistogramRecord(histogram, 42.0);
}

TEST_F(MetricsTest, HistogramRecordWithAttrs)
{
    g_autoptr(ObservabilityHistogram) histogram =
        observabilityHistogramCreate("test.histogram.attrs", "desc", "ms");
    ASSERT_NE(histogram, nullptr);
    observabilityHistogramRecordWithAttrs(histogram, 25.5, "operation", "read", NULL);
    observabilityHistogramRecordWithAttrs(histogram, 50.0, "operation", "write", "subsystem", "matter", NULL);
}

TEST_F(MetricsTest, HistogramRecordWithAttrsEmpty)
{
    g_autoptr(ObservabilityHistogram) histogram =
        observabilityHistogramCreate("test.histogram.noattrs", "desc", "ms");
    ASSERT_NE(histogram, nullptr);
    observabilityHistogramRecordWithAttrs(histogram, 10.0, NULL);
}

TEST_F(MetricsTest, RecordNullHistogramIsSafe)
{
    observabilityHistogramRecord(nullptr, 1.0);
    // Should not crash
}

TEST_F(MetricsTest, HistogramRecordWithAttrsNullHistogram)
{
    // Should not crash
    observabilityHistogramRecordWithAttrs(nullptr, 1.0, "key", "val", NULL);
}
