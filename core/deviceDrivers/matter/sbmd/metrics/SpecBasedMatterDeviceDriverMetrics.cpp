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

#include "SpecBasedMatterDeviceDriverMetrics.h"

#ifdef BARTON_CONFIG_SBMD_METRICS

namespace barton
{
    SpecBasedMatterDeviceDriverMetrics::SpecBasedMatterDeviceDriverMetrics()
    {
        deferredTimeoutCounter = observabilityCounterCreate(
            "sbmd.deferred.timeout", "Number of deferred operations that exceeded the overall deadline", "1");
        deferralMaxDepthCounter = observabilityCounterCreate(
            "sbmd.deferred.max_depth", "Number of deferred operations that hit MAX_DEFERRAL_DEPTH", "1");
        deferredInFlightGauge = observabilityGaugeCreate(
            "sbmd.deferred.in_flight", "Number of deferred operations currently in flight", "1");
        deferredDurationHisto =
            observabilityHistogramCreate("sbmd.deferred.duration_ms",
                                         "Total wall-clock duration of a deferred operation from start to completion",
                                         "ms");
        deferralDepthHisto = observabilityHistogramCreate(
            "sbmd.deferred.depth", "Deferral depth at which a deferred operation completed", "1");
    }

    void SpecBasedMatterDeviceDriverMetrics::RecordDeferredStart(int64_t inFlight)
    {
        observabilityGaugeRecord(deferredInFlightGauge, inFlight);
    }

    void SpecBasedMatterDeviceDriverMetrics::RecordDeferredTimeout(const char *driver,
                                                                   const char *opType,
                                                                   const char *resourceId)
    {
        if (resourceId)
        {
            observabilityCounterAddWithAttrs(
                deferredTimeoutCounter, 1, "driver", driver, "op_type", opType, "resource_id", resourceId, nullptr);
        }
        else
        {
            observabilityCounterAddWithAttrs(deferredTimeoutCounter, 1, "driver", driver, "op_type", opType, nullptr);
        }
    }

    void SpecBasedMatterDeviceDriverMetrics::RecordDeferredDepthExceeded(const char *driver,
                                                                         const char *opType,
                                                                         const char *resourceId)
    {
        if (resourceId)
        {
            observabilityCounterAddWithAttrs(
                deferralMaxDepthCounter, 1, "driver", driver, "op_type", opType, "resource_id", resourceId, nullptr);
        }
        else
        {
            observabilityCounterAddWithAttrs(deferralMaxDepthCounter, 1, "driver", driver, "op_type", opType, nullptr);
        }
    }

    void SpecBasedMatterDeviceDriverMetrics::RecordDeferredComplete(double durationMs,
                                                                    double depth,
                                                                    const char *driver,
                                                                    const char *opType,
                                                                    const char *resourceId,
                                                                    int64_t inFlightAfter)
    {
        auto recordHisto = [&](ObservabilityHistogram *histo, double value) {
            if (!histo)
            {
                return;
            }

            if (resourceId)
            {
                observabilityHistogramRecordWithAttrs(
                    histo, value, "driver", driver, "op_type", opType, "resource_id", resourceId, nullptr);
            }
            else
            {
                observabilityHistogramRecordWithAttrs(histo, value, "driver", driver, "op_type", opType, nullptr);
            }
        };

        recordHisto(deferredDurationHisto, durationMs);
        recordHisto(deferralDepthHisto, depth);

        observabilityGaugeRecord(deferredInFlightGauge, inFlightAfter);
    }

} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
