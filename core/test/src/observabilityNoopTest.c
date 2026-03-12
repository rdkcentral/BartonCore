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

/*
 * Test that all observability headers compile and all API calls
 * are safe no-ops when BARTON_CONFIG_OBSERVABILITY is not defined.
 */

/* Ensure the flag is NOT defined for this test */
#ifdef BARTON_CONFIG_OBSERVABILITY
#undef BARTON_CONFIG_OBSERVABILITY
#endif

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include "observability/observabilityLogBridge.h"
#include "observability/observabilityMetrics.h"
#include "observability/observability.h"
#include "observability/observabilityTracing.h"

static void test_observability_noop_init_shutdown(void **state)
{
    (void) state;
    assert_int_equal(observabilityInit(), 0);
    observabilityShutdown();
}

static void test_tracing_noop_span_lifecycle(void **state)
{
    (void) state;

    ObservabilitySpan *span = observabilitySpanStart("test.span");
    assert_null(span);

    observabilitySpanSetAttribute(span, "key", "value");
    observabilitySpanSetAttributeInt(span, "count", 42);
    observabilitySpanSetError(span, "error message");

    ObservabilitySpanContext *ctx = observabilitySpanGetContext(span);
    assert_null(ctx);

    ObservabilitySpan *child = observabilitySpanStartWithParent("child.span", ctx);
    assert_null(child);

    observabilitySpanRelease(child);
    observabilitySpanContextRelease(ctx);
    observabilitySpanRelease(span);
}

static void test_metrics_noop_counter_gauge(void **state)
{
    (void) state;

    ObservabilityCounter *counter = observabilityCounterCreate("test.counter", "desc", "1");
    assert_null(counter);
    observabilityCounterAdd(counter, 1);
    observabilityCounterAddWithAttrs(counter, 1, "key", "val", NULL);

    ObservabilityGauge *gauge = observabilityGaugeCreate("test.gauge", "desc", "1");
    assert_null(gauge);
    observabilityGaugeRecord(gauge, 42);
    observabilityGaugeRecordWithAttrs(gauge, 42, "key", "val", NULL);

    ObservabilityHistogram *histogram = observabilityHistogramCreate("test.histogram", "desc", "ms");
    assert_null(histogram);
    observabilityHistogramRecord(histogram, 1.5);
    observabilityHistogramRecordWithAttrs(histogram, 2.5, "key", "val", NULL);
    observabilityHistogramRelease(histogram);
}

static void test_logbridge_noop(void **state)
{
    (void) state;

    assert_int_equal(observabilityLogBridgeInit(), 0);
    observabilityLogBridgeEmit("category", 0, "file.c", "func", 1, "message");
    observabilityLogBridgeShutdown();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_observability_noop_init_shutdown),
        cmocka_unit_test(test_tracing_noop_span_lifecycle),
        cmocka_unit_test(test_metrics_noop_counter_gauge),
        cmocka_unit_test(test_logbridge_noop),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
