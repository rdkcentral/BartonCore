// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

/*
 * Unit tests for in-memory observability instruments: counter, gauge, histogram.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "observability/observability.h"
#include "observability/observabilityMetrics.h"

#include <cjson/cJSON.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Setup / teardown                                                   */
/* ------------------------------------------------------------------ */

static int setup(void **state)
{
    (void) state;
    observabilityInit();

    return 0;
}

static int teardown(void **state)
{
    (void) state;
    observabilityShutdown();

    return 0;
}

/* ------------------------------------------------------------------ */
/* Counter tests                                                      */
/* ------------------------------------------------------------------ */

static void test_counter_add(void **state)
{
    (void) state;

    ObservabilityCounter *c = observabilityCounterCreate("test.counter", "A test counter", "1");
    assert_non_null(c);

    observabilityCounterAdd(c, 5);
    observabilityCounterAdd(c, 3);

    /* Verify via JSON dump */
    char *json = observabilityDumpJson();
    assert_non_null(json);

    cJSON *root = cJSON_Parse(json);
    assert_non_null(root);

    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    cJSON *counter = cJSON_GetObjectItem(metrics, "test.counter");
    assert_non_null(counter);

    cJSON *type = cJSON_GetObjectItem(counter, "type");
    assert_string_equal(cJSON_GetStringValue(type), "counter");

    cJSON *dataPoints = cJSON_GetObjectItem(counter, "dataPoints");
    assert_int_equal(cJSON_GetArraySize(dataPoints), 1);

    cJSON *dp = cJSON_GetArrayItem(dataPoints, 0);
    assert_int_equal((int) cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "value")), 8);

    cJSON_Delete(root);
    free(json);
    observabilityCounterRelease(c);
}

static void test_counter_with_attrs(void **state)
{
    (void) state;

    ObservabilityCounter *c = observabilityCounterCreate("test.counter.attrs", "Counter with attrs", "1");

    observabilityCounterAddWithAttrs(c, 1, "driver", "light", NULL);
    observabilityCounterAddWithAttrs(c, 2, "driver", "light", NULL);
    observabilityCounterAddWithAttrs(c, 10, "driver", "lock", NULL);

    char *json = observabilityDumpJson();
    cJSON *root = cJSON_Parse(json);
    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    cJSON *counter = cJSON_GetObjectItem(metrics, "test.counter.attrs");
    cJSON *dataPoints = cJSON_GetObjectItem(counter, "dataPoints");

    /* Should have two data points: one for driver=light, one for driver=lock */
    assert_int_equal(cJSON_GetArraySize(dataPoints), 2);

    /* Find the light data point */
    bool foundLight = false;
    bool foundLock = false;

    for (int i = 0; i < cJSON_GetArraySize(dataPoints); i++)
    {
        cJSON *dp = cJSON_GetArrayItem(dataPoints, i);
        cJSON *attrs = cJSON_GetObjectItem(dp, "attributes");

        if (attrs)
        {
            cJSON *driver = cJSON_GetObjectItem(attrs, "driver");

            if (driver && strcmp(cJSON_GetStringValue(driver), "light") == 0)
            {
                assert_int_equal((int) cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "value")), 3);
                foundLight = true;
            }
            else if (driver && strcmp(cJSON_GetStringValue(driver), "lock") == 0)
            {
                assert_int_equal((int) cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "value")), 10);
                foundLock = true;
            }
        }
    }

    assert_true(foundLight);
    assert_true(foundLock);

    cJSON_Delete(root);
    free(json);
    observabilityCounterRelease(c);
}

static void test_counter_null_safe(void **state)
{
    (void) state;

    /* These should not crash */
    observabilityCounterAdd(NULL, 5);
    observabilityCounterAddWithAttrs(NULL, 5, "key", "val", NULL);
    observabilityCounterRelease(NULL);
}

/* ------------------------------------------------------------------ */
/* Gauge tests                                                        */
/* ------------------------------------------------------------------ */

static void test_gauge_record(void **state)
{
    (void) state;

    ObservabilityGauge *g = observabilityGaugeCreate("test.gauge", "A test gauge", "bytes");
    assert_non_null(g);

    observabilityGaugeRecord(g, 100);
    observabilityGaugeRecord(g, 50);

    char *json = observabilityDumpJson();
    cJSON *root = cJSON_Parse(json);
    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    cJSON *gauge = cJSON_GetObjectItem(metrics, "test.gauge");

    cJSON *type = cJSON_GetObjectItem(gauge, "type");
    assert_string_equal(cJSON_GetStringValue(type), "gauge");

    cJSON *unit = cJSON_GetObjectItem(gauge, "unit");
    assert_string_equal(cJSON_GetStringValue(unit), "bytes");

    cJSON *dataPoints = cJSON_GetObjectItem(gauge, "dataPoints");
    assert_int_equal(cJSON_GetArraySize(dataPoints), 1);

    cJSON *dp = cJSON_GetArrayItem(dataPoints, 0);
    /* Gauge should record latest value, not sum */
    assert_int_equal((int) cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "value")), 50);

    cJSON_Delete(root);
    free(json);
    observabilityGaugeRelease(g);
}

static void test_gauge_with_attrs(void **state)
{
    (void) state;

    ObservabilityGauge *g = observabilityGaugeCreate("test.gauge.attrs", "Gauge with attrs", "1");

    observabilityGaugeRecordWithAttrs(g, 42, "device", "abc123", NULL);
    observabilityGaugeRecordWithAttrs(g, 99, "device", "def456", NULL);

    char *json = observabilityDumpJson();
    cJSON *root = cJSON_Parse(json);
    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    cJSON *gauge = cJSON_GetObjectItem(metrics, "test.gauge.attrs");
    cJSON *dataPoints = cJSON_GetObjectItem(gauge, "dataPoints");

    assert_int_equal(cJSON_GetArraySize(dataPoints), 2);

    cJSON_Delete(root);
    free(json);
    observabilityGaugeRelease(g);
}

static void test_gauge_null_safe(void **state)
{
    (void) state;

    observabilityGaugeRecord(NULL, 5);
    observabilityGaugeRecordWithAttrs(NULL, 5, "key", "val", NULL);
    observabilityGaugeRelease(NULL);
}

/* ------------------------------------------------------------------ */
/* Histogram tests                                                    */
/* ------------------------------------------------------------------ */

static void test_histogram_record(void **state)
{
    (void) state;

    ObservabilityHistogram *h = observabilityHistogramCreate("test.histogram", "A test histogram", "ms");
    assert_non_null(h);

    observabilityHistogramRecord(h, 1.0);
    observabilityHistogramRecord(h, 2.0);
    observabilityHistogramRecord(h, 3.0);

    char *json = observabilityDumpJson();
    cJSON *root = cJSON_Parse(json);
    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    cJSON *histogram = cJSON_GetObjectItem(metrics, "test.histogram");

    cJSON *type = cJSON_GetObjectItem(histogram, "type");
    assert_string_equal(cJSON_GetStringValue(type), "histogram");

    cJSON *dataPoints = cJSON_GetObjectItem(histogram, "dataPoints");
    assert_int_equal(cJSON_GetArraySize(dataPoints), 1);

    cJSON *dp = cJSON_GetArrayItem(dataPoints, 0);
    assert_int_equal((int) cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "count")), 3);
    assert_true(fabs(cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "sum")) - 6.0) < 1e-9);
    assert_true(fabs(cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "min")) - 1.0) < 1e-9);
    assert_true(fabs(cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "max")) - 3.0) < 1e-9);

    /* Verify buckets exist */
    cJSON *buckets = cJSON_GetObjectItem(dp, "buckets");
    assert_true(cJSON_GetArraySize(buckets) > 0);

    cJSON_Delete(root);
    free(json);
    observabilityHistogramRelease(h);
}

static void test_histogram_bucket_distribution(void **state)
{
    (void) state;

    ObservabilityHistogram *h = observabilityHistogramCreate("test.histogram.buckets", "Bucket test", "ms");

    /* Record values that span multiple buckets:
     * Bounds: 0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000
     * Value 0 -> bucket[0] (le=0)
     * Value 3 -> bucket[1] (le=5)
     * Value 7 -> bucket[2] (le=10)
     * Value 200 -> bucket[7] (le=250)
     * Value 99999 -> bucket[15] (overflow, le=+Inf)
     */
    observabilityHistogramRecord(h, 0.0);
    observabilityHistogramRecord(h, 3.0);
    observabilityHistogramRecord(h, 7.0);
    observabilityHistogramRecord(h, 200.0);
    observabilityHistogramRecord(h, 99999.0);

    char *json = observabilityDumpJson();
    cJSON *root = cJSON_Parse(json);
    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    cJSON *histogram = cJSON_GetObjectItem(metrics, "test.histogram.buckets");
    cJSON *dataPoints = cJSON_GetObjectItem(histogram, "dataPoints");
    cJSON *dp = cJSON_GetArrayItem(dataPoints, 0);

    assert_int_equal((int) cJSON_GetNumberValue(cJSON_GetObjectItem(dp, "count")), 5);

    /* Check that the overflow bucket has the 99999 value */
    cJSON *buckets = cJSON_GetObjectItem(dp, "buckets");
    int numBuckets = cJSON_GetArraySize(buckets);
    cJSON *lastBucket = cJSON_GetArrayItem(buckets, numBuckets - 1);
    assert_string_equal(cJSON_GetStringValue(cJSON_GetObjectItem(lastBucket, "le")), "+Inf");
    assert_int_equal((int) cJSON_GetNumberValue(cJSON_GetObjectItem(lastBucket, "count")), 1);

    cJSON_Delete(root);
    free(json);
    observabilityHistogramRelease(h);
}

static void test_histogram_with_attrs(void **state)
{
    (void) state;

    ObservabilityHistogram *h = observabilityHistogramCreate("test.histogram.attrs", "Histogram with attrs", "ms");

    observabilityHistogramRecordWithAttrs(h, 5.0, "op", "read", NULL);
    observabilityHistogramRecordWithAttrs(h, 10.0, "op", "write", NULL);
    observabilityHistogramRecordWithAttrs(h, 15.0, "op", "read", NULL);

    char *json = observabilityDumpJson();
    cJSON *root = cJSON_Parse(json);
    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    cJSON *histogram = cJSON_GetObjectItem(metrics, "test.histogram.attrs");
    cJSON *dataPoints = cJSON_GetObjectItem(histogram, "dataPoints");

    /* Two distinct attribute sets: op=read and op=write */
    assert_int_equal(cJSON_GetArraySize(dataPoints), 2);

    cJSON_Delete(root);
    free(json);
    observabilityHistogramRelease(h);
}

static void test_histogram_null_safe(void **state)
{
    (void) state;

    observabilityHistogramRecord(NULL, 5.0);
    observabilityHistogramRecordWithAttrs(NULL, 5.0, "key", "val", NULL);
    observabilityHistogramRelease(NULL);
}

/* ------------------------------------------------------------------ */
/* Release tests                                                      */
/* ------------------------------------------------------------------ */

static void test_counter_release_removes_from_dump(void **state)
{
    (void) state;

    ObservabilityCounter *c = observabilityCounterCreate("release.counter", "Release test", "1");
    assert_non_null(c);

    observabilityCounterAdd(c, 1);
    observabilityCounterRelease(c);

    char *json = observabilityDumpJson();
    assert_non_null(json);

    cJSON *root = cJSON_Parse(json);
    assert_non_null(root);

    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    assert_null(cJSON_GetObjectItem(metrics, "release.counter"));

    cJSON_Delete(root);
    free(json);
}

/* ------------------------------------------------------------------ */
/* JSON dump tests                                                    */
/* ------------------------------------------------------------------ */

static void test_dump_empty(void **state)
{
    (void) state;

    char *json = observabilityDumpJson();
    assert_non_null(json);

    cJSON *root = cJSON_Parse(json);
    assert_non_null(root);

    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");
    assert_non_null(metrics);

    /* No instruments registered in this test, so metrics should be empty */
    assert_int_equal(cJSON_GetArraySize(metrics), 0);

    cJSON_Delete(root);
    free(json);
}

static void test_dump_multiple_instruments(void **state)
{
    (void) state;

    ObservabilityCounter *c = observabilityCounterCreate("multi.counter", "counter", "1");
    ObservabilityGauge *g = observabilityGaugeCreate("multi.gauge", "gauge", "bytes");
    ObservabilityHistogram *h = observabilityHistogramCreate("multi.histogram", "histogram", "ms");

    observabilityCounterAdd(c, 1);
    observabilityGaugeRecord(g, 42);
    observabilityHistogramRecord(h, 5.0);

    char *json = observabilityDumpJson();
    cJSON *root = cJSON_Parse(json);
    cJSON *metrics = cJSON_GetObjectItem(root, "metrics");

    assert_non_null(cJSON_GetObjectItem(metrics, "multi.counter"));
    assert_non_null(cJSON_GetObjectItem(metrics, "multi.gauge"));
    assert_non_null(cJSON_GetObjectItem(metrics, "multi.histogram"));

    /* Verify type fields */
    assert_string_equal(
        cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(metrics, "multi.counter"), "type")), "counter");
    assert_string_equal(
        cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(metrics, "multi.gauge"), "type")), "gauge");
    assert_string_equal(
        cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(metrics, "multi.histogram"), "type")), "histogram");

    /* Verify description and unit are included */
    assert_string_equal(
        cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(metrics, "multi.gauge"), "unit")), "bytes");

    cJSON_Delete(root);
    free(json);
    observabilityCounterRelease(c);
    observabilityGaugeRelease(g);
    observabilityHistogramRelease(h);
}

/* ------------------------------------------------------------------ */
/* Test runner                                                        */
/* ------------------------------------------------------------------ */

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* Counter */
        cmocka_unit_test_setup_teardown(test_counter_add, setup, teardown),
        cmocka_unit_test_setup_teardown(test_counter_with_attrs, setup, teardown),
        cmocka_unit_test_setup_teardown(test_counter_null_safe, setup, teardown),
        /* Gauge */
        cmocka_unit_test_setup_teardown(test_gauge_record, setup, teardown),
        cmocka_unit_test_setup_teardown(test_gauge_with_attrs, setup, teardown),
        cmocka_unit_test_setup_teardown(test_gauge_null_safe, setup, teardown),
        /* Histogram */
        cmocka_unit_test_setup_teardown(test_histogram_record, setup, teardown),
        cmocka_unit_test_setup_teardown(test_histogram_bucket_distribution, setup, teardown),
        cmocka_unit_test_setup_teardown(test_histogram_with_attrs, setup, teardown),
        cmocka_unit_test_setup_teardown(test_histogram_null_safe, setup, teardown),
        /* Release */
        cmocka_unit_test_setup_teardown(test_counter_release_removes_from_dump, setup, teardown),
        /* JSON dump */
        cmocka_unit_test_setup_teardown(test_dump_empty, setup, teardown),
        cmocka_unit_test_setup_teardown(test_dump_multiple_instruments, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
