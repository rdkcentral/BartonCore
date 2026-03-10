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

#ifdef BARTON_CONFIG_OBSERVABILITY

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
 * Release a counter reference. Frees when the last reference is dropped.
 * @param counter  Counter to release (NULL is safe no-op)
 */
void observabilityCounterRelease(ObservabilityCounter *counter);

/**
 * Release a gauge reference. Frees when the last reference is dropped.
 * @param gauge  Gauge to release (NULL is safe no-op)
 */
void observabilityGaugeRelease(ObservabilityGauge *gauge);

#else /* !BARTON_CONFIG_OBSERVABILITY */

static inline ObservabilityCounter *observabilityCounterCreate(const char *name, const char *description, const char *unit)
{
    (void) name;
    (void) description;
    (void) unit;
    return (ObservabilityCounter *) 0;
}
static inline void observabilityCounterAdd(ObservabilityCounter *counter, uint64_t value)
{
    (void) counter;
    (void) value;
}
static inline ObservabilityGauge *observabilityGaugeCreate(const char *name, const char *description, const char *unit)
{
    (void) name;
    (void) description;
    (void) unit;
    return (ObservabilityGauge *) 0;
}
static inline void observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value)
{
    (void) gauge;
    (void) value;
}
static inline void observabilityCounterRelease(ObservabilityCounter *counter)
{
    (void) counter;
}
static inline void observabilityGaugeRelease(ObservabilityGauge *gauge)
{
    (void) gauge;
}

#endif /* BARTON_CONFIG_OBSERVABILITY */

#ifdef __cplusplus
}
#endif

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObservabilityCounter, observabilityCounterRelease)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObservabilityGauge, observabilityGaugeRelease)

#endif /* OBSERVABILITY_METRICS_H */
