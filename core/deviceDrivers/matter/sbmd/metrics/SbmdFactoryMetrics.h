//------------------------------ tabstop = 4 ----------------------------------
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
//------------------------------ tabstop = 4 ----------------------------------

#pragma once

#include <cstdint>

#ifdef BARTON_CONFIG_SBMD_METRICS

#include "observability/observabilityMetrics.h"

namespace barton
{
    /**
     * Observability metrics for SbmdFactory: driver load durations, heap
     * impact, failure counts, and registered-driver gauge.
     *
     * Self-registers with MetricsRegistry at static-initialization time.
     * InitializeMetrics() / ShutdownMetrics() are called by MetricsRegistry
     * — do not call them directly.
     */
    class SbmdFactoryMetrics
    {
    public:
        static void InitializeMetrics();
        static void ShutdownMetrics();

        /** Record a driver load failure. */
        static void RecordDriverLoadFailure(const char *driver, const char *reason);

        /** Record a successful driver load. */
        static void RecordDriverLoadSuccess(double durationMs, double heapDelta, const char *driver);

        /** Record the current registered-driver count. */
        static void RecordRegisteredDriverCount(int64_t count);

    private:
        static ObservabilityCounter *driverLoadFailureCounter;
        static ObservabilityHistogram *driverLoadDurationHisto;
        static ObservabilityHistogram *driverLoadHeapDeltaHisto;
        static ObservabilityGauge *registeredDriversGauge;
    };

} // namespace barton

#else

namespace barton
{
    class SbmdFactoryMetrics
    {
    public:
        static void InitializeMetrics() {}
        static void ShutdownMetrics() {}
        static void RecordDriverLoadFailure(const char *, const char *) {}
        static void RecordDriverLoadSuccess(double, double, const char *) {}
        static void RecordRegisteredDriverCount(int64_t) {}
    };
} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
