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

#ifndef OBSERVABILITY_METRICS_H
#define OBSERVABILITY_METRICS_H

#include <stdint.h>

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque counter handle */
typedef struct ObservabilityCounter ObservabilityCounter;

/** Opaque gauge handle */
typedef struct ObservabilityGauge ObservabilityGauge;

/** Opaque histogram handle */
typedef struct ObservabilityHistogram ObservabilityHistogram;

/**
 * Create a named counter instrument.
 * @param name         Metric name (e.g., "device.commfail.count")
 * @param description  Human-readable description
 * @param unit         Unit of measurement (e.g., "1", "ms")
 * @return Counter handle, or NULL on failure
 */
ObservabilityCounter *observabilityCounterCreate(const char *name, const char *description, const char *unit);

/**
 * Add a value to a counter.
 * @param counter  Counter handle (NULL is safe no-op)
 * @param value    Value to add (must be non-negative)
 */
void observabilityCounterAdd(ObservabilityCounter *counter, uint64_t value);

/**
 * Add a value to a counter with string key-value attributes.
 * The attribute list is NULL-terminated: pass key, value pairs followed by NULL.
 * @param counter  Counter handle (NULL is safe no-op)
 * @param value    Value to add (must be non-negative)
 * @param ...      NULL-terminated pairs of (const char *key, const char *value)
 */
void observabilityCounterAddWithAttrs(ObservabilityCounter *counter, uint64_t value, ...);

/**
 * Create a named gauge instrument.
 * @param name         Metric name (e.g., "device.active.count")
 * @param description  Human-readable description
 * @param unit         Unit of measurement
 * @return Gauge handle, or NULL on failure
 */
ObservabilityGauge *observabilityGaugeCreate(const char *name, const char *description, const char *unit);

/**
 * Record a gauge value.
 * @param gauge  Gauge handle (NULL is safe no-op)
 * @param value  Current value to record
 */
void observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value);

/**
 * Record a gauge value with string key-value attributes.
 * The attribute list is NULL-terminated: pass key, value pairs followed by NULL.
 * @param gauge  Gauge handle (NULL is safe no-op)
 * @param value  Current value to record
 * @param ...    NULL-terminated pairs of (const char *key, const char *value)
 */
void observabilityGaugeRecordWithAttrs(ObservabilityGauge *gauge, int64_t value, ...);

/**
 * Create a named histogram instrument.
 * @param name         Metric name (e.g., "device.discovery.duration")
 * @param description  Human-readable description
 * @param unit         Unit of measurement (e.g., "s", "ms")
 * @return Histogram handle, or NULL on failure
 */
ObservabilityHistogram *observabilityHistogramCreate(const char *name, const char *description, const char *unit);

/**
 * Record a value into a histogram.
 * @param histogram  Histogram handle (NULL is safe no-op)
 * @param value      Value to record
 */
void observabilityHistogramRecord(ObservabilityHistogram *histogram, double value);

/**
 * Record a value into a histogram with string key-value attributes.
 * The attribute list is NULL-terminated: pass key, value pairs followed by NULL.
 * @param histogram  Histogram handle (NULL is safe no-op)
 * @param value      Value to record
 * @param ...        NULL-terminated pairs of (const char *key, const char *value)
 */
void observabilityHistogramRecordWithAttrs(ObservabilityHistogram *histogram, double value, ...);

/**
 * Acquire an additional reference to a counter. The returned handle refers to
 * the same instrument and must be balanced by a matching observabilityCounterRelease().
 * @param counter  Counter to acquire (NULL is safe and returns NULL)
 * @return The same counter handle, or NULL if counter was NULL
 */
ObservabilityCounter *observabilityCounterAcquire(ObservabilityCounter *counter);

/**
 * Release a counter reference. Frees when the last reference is dropped.
 * @param counter  Counter to release (NULL is safe no-op)
 */
void observabilityCounterRelease(ObservabilityCounter *counter);

/**
 * Acquire an additional reference to a gauge. The returned handle refers to
 * the same instrument and must be balanced by a matching observabilityGaugeRelease().
 * @param gauge  Gauge to acquire (NULL is safe and returns NULL)
 * @return The same gauge handle, or NULL if gauge was NULL
 */
ObservabilityGauge *observabilityGaugeAcquire(ObservabilityGauge *gauge);

/**
 * Release a gauge reference. Frees when the last reference is dropped.
 * @param gauge  Gauge to release (NULL is safe no-op)
 */
void observabilityGaugeRelease(ObservabilityGauge *gauge);

/**
 * Acquire an additional reference to a histogram. The returned handle refers to
 * the same instrument and must be balanced by a matching observabilityHistogramRelease().
 * @param histogram  Histogram to acquire (NULL is safe and returns NULL)
 * @return The same histogram handle, or NULL if histogram was NULL
 */
ObservabilityHistogram *observabilityHistogramAcquire(ObservabilityHistogram *histogram);

/**
 * Release a histogram reference. Frees when the last reference is dropped.
 * @param histogram  Histogram to release (NULL is safe no-op)
 */
void observabilityHistogramRelease(ObservabilityHistogram *histogram);

#ifdef __cplusplus
}
#endif

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObservabilityCounter, observabilityCounterRelease)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObservabilityGauge, observabilityGaugeRelease)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObservabilityHistogram, observabilityHistogramRelease)

#endif /* OBSERVABILITY_METRICS_H */
