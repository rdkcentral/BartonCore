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
     * Observability metrics for SpecBasedMatterDeviceDriver: deferred
     * operation in-flight counts, depths, durations, timeouts, and
     * max-depth overflows.
     */
    class SpecBasedMatterDeviceDriverMetrics
    {
    public:
        SpecBasedMatterDeviceDriverMetrics();

        /** Record the in-flight gauge after a new deferred op is registered. */
        void RecordDeferredStart(int64_t inFlight);

        /** Record a deferred op overall-deadline timeout. */
        void RecordDeferredTimeout(const char *driver, const char *opType, const char *resourceId);

        /** Record a deferred op that was aborted because it exceeded the maximum deferral depth. */
        void RecordDeferredDepthExceeded(const char *driver, const char *opType, const char *resourceId);

        /**
         * Record a completed deferred operation.
         * @param resourceId    nullptr to omit the "resource_id" attribute
         * @param inFlightAfter pendingOperations size after removal
         */
        void RecordDeferredComplete(double durationMs,
                                    double depth,
                                    const char *driver,
                                    const char *opType,
                                    const char *resourceId,
                                    int64_t inFlightAfter);

    private:
        ObservabilityCounter *deferredTimeoutCounter = nullptr;
        ObservabilityCounter *deferralMaxDepthCounter = nullptr;
        ObservabilityGauge *deferredInFlightGauge = nullptr;
        ObservabilityHistogram *deferredDurationHisto = nullptr;
        ObservabilityHistogram *deferralDepthHisto = nullptr;
    };

} // namespace barton

#else

namespace barton
{
    class SpecBasedMatterDeviceDriverMetrics
    {
    public:
        void RecordDeferredStart(int64_t) {}

        void RecordDeferredTimeout(const char *, const char *, const char *) {}

        void RecordDeferredDepthExceeded(const char *, const char *, const char *) {}

        void RecordDeferredComplete(double, double, const char *, const char *, const char *, int64_t) {}
    };
} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
