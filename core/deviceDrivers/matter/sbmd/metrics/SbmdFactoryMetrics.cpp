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

#include "SbmdFactoryMetrics.h"
#include "MetricsRegistry.h"

#ifdef BARTON_CONFIG_SBMD_METRICS

namespace barton
{
    ObservabilityCounter *SbmdFactoryMetrics::driverLoadFailureCounter = nullptr;
    ObservabilityHistogram *SbmdFactoryMetrics::driverLoadDurationHisto = nullptr;
    ObservabilityHistogram *SbmdFactoryMetrics::driverLoadHeapDeltaHisto = nullptr;
    ObservabilityGauge *SbmdFactoryMetrics::registeredDriversGauge = nullptr;

    // Self-register with MetricsRegistry before main().
    static bool s_registered = (MetricsRegistry::registerProvider(
                                    {SbmdFactoryMetrics::InitializeMetrics, SbmdFactoryMetrics::ShutdownMetrics}),
                                true);

    void SbmdFactoryMetrics::InitializeMetrics()
    {
        if (driverLoadFailureCounter != nullptr)
        {
            return;
        }

        driverLoadFailureCounter =
            observabilityCounterCreate("sbmd.driver.load.failure", "Number of SBMD driver loads that failed", "1");
        driverLoadDurationHisto = observabilityHistogramCreate(
            "sbmd.driver.load.duration_ms", "Time to load and activate an SBMD driver", "ms");
        driverLoadHeapDeltaHisto = observabilityHistogramCreate(
            "sbmd.driver.load.heap_bytes", "Change in mquickjs heap_used across a driver load", "By");
        registeredDriversGauge = observabilityGaugeCreate(
            "sbmd.driver.registered.count", "Number of SBMD drivers successfully registered", "1");
    }

    void SbmdFactoryMetrics::ShutdownMetrics()
    {
        observabilityHistogramRelease(driverLoadDurationHisto);
        driverLoadDurationHisto = nullptr;
        observabilityHistogramRelease(driverLoadHeapDeltaHisto);
        driverLoadHeapDeltaHisto = nullptr;
        observabilityGaugeRelease(registeredDriversGauge);
        registeredDriversGauge = nullptr;
        // driverLoadFailureCounter released last — serves as the idempotence sentinel
        observabilityCounterRelease(driverLoadFailureCounter);
        driverLoadFailureCounter = nullptr;
    }

    void SbmdFactoryMetrics::RecordDriverLoadFailure(const char *driver, const char *reason)
    {
        if (!driverLoadFailureCounter)
        {
            return;
        }

        observabilityCounterAddWithAttrs(driverLoadFailureCounter, 1, "driver", driver, "reason", reason, nullptr);
    }

    void SbmdFactoryMetrics::RecordDriverLoadSuccess(double durationMs, double heapDelta, const char *driver)
    {
        if (driverLoadDurationHisto)
        {
            observabilityHistogramRecordWithAttrs(driverLoadDurationHisto, durationMs, "driver", driver, nullptr);
        }

        if (driverLoadHeapDeltaHisto)
        {
            observabilityHistogramRecordWithAttrs(driverLoadHeapDeltaHisto, heapDelta, "driver", driver, nullptr);
        }
    }

    void SbmdFactoryMetrics::RecordRegisteredDriverCount(int64_t count)
    {
        if (!registeredDriversGauge)
        {
            return;
        }

        observabilityGaugeRecord(registeredDriversGauge, count);
    }

} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
