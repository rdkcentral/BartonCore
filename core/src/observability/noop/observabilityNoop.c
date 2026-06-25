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
 * No-op observability backend.
 *
 * All functions are safe stubs that do nothing. This is compiled when
 * BCORE_OBSERVABILITY_BACKEND is "none".
 */

#include "observability/observability.h"
#include "observability/observabilityMetrics.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --- init / shutdown / dump --- */

int observabilityInit(void)
{
    return 0;
}

void observabilityShutdown(void)
{
}

char *observabilityDumpJson(void)
{
    return strdup("{\"metrics\":{}}");
}

/* --- counters --- */

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

void observabilityCounterRelease(ObservabilityCounter *counter)
{
    (void) counter;
}

/* --- gauges --- */

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

void observabilityGaugeRelease(ObservabilityGauge *gauge)
{
    (void) gauge;
}

/* --- histograms --- */

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

void observabilityHistogramRelease(ObservabilityHistogram *histogram)
{
    (void) histogram;
}
