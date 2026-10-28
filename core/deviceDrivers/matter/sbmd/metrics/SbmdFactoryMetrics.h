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
     * Observability metrics for SbmdFactory: driver load durations, failure
     * counts, and registered-driver gauge.
     */
    class SbmdFactoryMetrics
    {
    public:
        SbmdFactoryMetrics();

        /** Record a driver load failure. */
        void RecordDriverLoadFailure(const char *driver, const char *reason);

        /** Record a successful driver load. */
        void RecordDriverLoadSuccess(double durationMs, const char *driver);

        /** Record the current registered-driver count. */
        void RecordRegisteredDriverCount(int64_t count);

        /** Record total wall time to discover, load, activate, and register all drivers. */
        void RecordRegistrationTotal(double durationMs);

        /** Record the one-time SBMD bundle load + capture-injection time. */
        void RecordBundleLoad(double durationMs);

    private:
        ObservabilityCounter *driverLoadFailureCounter = nullptr;
        ObservabilityHistogram *driverLoadDurationHisto = nullptr;
        ObservabilityGauge *registeredDriversGauge = nullptr;
        ObservabilityHistogram *registrationTotalHisto = nullptr;
        ObservabilityHistogram *bundleLoadHisto = nullptr;
    };

} // namespace barton

#else

namespace barton
{
    class SbmdFactoryMetrics
    {
    public:
        void RecordDriverLoadFailure(const char *, const char *) {}

        void RecordDriverLoadSuccess(double, const char *) {}

        void RecordRegisteredDriverCount(int64_t) {}

        void RecordRegistrationTotal(double) {}

        void RecordBundleLoad(double) {}
    };
} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
