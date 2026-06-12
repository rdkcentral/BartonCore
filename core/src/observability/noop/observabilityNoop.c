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
 * No-op observability backend.
 *
 * All functions are safe stubs that do nothing. This is compiled when
 * BCORE_OBSERVABILITY_BACKEND is "none" (the default).
 */

#include "observability/observability.h"
#include "observability/observabilityLogBridge.h"
#include "observability/observabilityMetrics.h"
#include "observability/observabilityTracing.h"
#include "observability/deviceTelemetry.h"

#include <stdbool.h>
#include <stdarg.h>

/* --- init / shutdown --- */

int observabilityInit(void)
{
    return 0;
}

void observabilityShutdown(void)
{
}

/* --- log bridge --- */

int observabilityLogBridgeInit(void)
{
    return 0;
}

void observabilityLogBridgeEmit(const char *category,
                                int priority,
                                const char *file,
                                const char *func,
                                int line,
                                const char *message)
{
    (void) category;
    (void) priority;
    (void) file;
    (void) func;
    (void) line;
    (void) message;
}

void observabilityLogBridgeShutdown(void)
{
}

/* --- metrics --- */

ObservabilityCounter *observabilityCounterCreate(const char *name, const char *description, const char *unit)
{
    (void) name;
    (void) description;
    (void) unit;

    return NULL;
}

void observabilityCounterAdd(ObservabilityCounter *counter, uint64_t value)
{
    (void) counter;
    (void) value;
}

void observabilityCounterAddWithAttrs(ObservabilityCounter *counter, uint64_t value, ...)
{
    (void) counter;
    (void) value;
}

ObservabilityGauge *observabilityGaugeCreate(const char *name, const char *description, const char *unit)
{
    (void) name;
    (void) description;
    (void) unit;

    return NULL;
}

void observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value)
{
    (void) gauge;
    (void) value;
}

void observabilityGaugeRecordWithAttrs(ObservabilityGauge *gauge, int64_t value, ...)
{
    (void) gauge;
    (void) value;
}

ObservabilityHistogram *observabilityHistogramCreate(const char *name, const char *description, const char *unit)
{
    (void) name;
    (void) description;
    (void) unit;

    return NULL;
}

void observabilityHistogramRecord(ObservabilityHistogram *histogram, double value)
{
    (void) histogram;
    (void) value;
}

void observabilityHistogramRecordWithAttrs(ObservabilityHistogram *histogram, double value, ...)
{
    (void) histogram;
    (void) value;
}

void observabilityCounterRelease(ObservabilityCounter *counter)
{
    (void) counter;
}

void observabilityGaugeRelease(ObservabilityGauge *gauge)
{
    (void) gauge;
}

void observabilityHistogramRelease(ObservabilityHistogram *histogram)
{
    (void) histogram;
}

/* --- tracing --- */

ObservabilitySpan *observabilitySpanStart(const char *name)
{
    (void) name;

    return NULL;
}

ObservabilitySpan *observabilitySpanStartWithParent(const char *name, const ObservabilitySpanContext *parent)
{
    (void) name;
    (void) parent;

    return NULL;
}

ObservabilitySpan *observabilitySpanStartWithLink(const char *name, const ObservabilitySpanContext *linked)
{
    (void) name;
    (void) linked;

    return NULL;
}

void observabilitySpanRelease(ObservabilitySpan *span)
{
    (void) span;
}

void observabilitySpanSetAttribute(ObservabilitySpan *span, const char *key, const char *value)
{
    (void) span;
    (void) key;
    (void) value;
}

void observabilitySpanSetAttributeInt(ObservabilitySpan *span, const char *key, int64_t value)
{
    (void) span;
    (void) key;
    (void) value;
}

void observabilitySpanSetError(ObservabilitySpan *span, const char *message)
{
    (void) span;
    (void) message;
}

ObservabilitySpanContext *observabilitySpanGetContext(ObservabilitySpan *span)
{
    (void) span;

    return NULL;
}

void observabilitySpanContextRef(ObservabilitySpanContext *ctx)
{
    (void) ctx;
}

void observabilitySpanContextRelease(ObservabilitySpanContext *ctx)
{
    (void) ctx;
}

void observabilitySpanContextSetCurrent(ObservabilitySpanContext *ctx)
{
    (void) ctx;
}

ObservabilitySpanContext *observabilitySpanContextGetCurrent(void)
{
    return NULL;
}

/* --- device telemetry --- */

void deviceTelemetryRecordResourceUpdate(const char *deviceUuid,
                                         const char *endpointId,
                                         const char *resourceId,
                                         const char *resourceType,
                                         const char *newValue,
                                         bool didChange)
{
    (void) deviceUuid;
    (void) endpointId;
    (void) resourceId;
    (void) resourceType;
    (void) newValue;
    (void) didChange;
}

void deviceTelemetryInvalidateCache(const char *deviceUuid)
{
    (void) deviceUuid;
}
