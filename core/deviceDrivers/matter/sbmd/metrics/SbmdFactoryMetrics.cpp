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

#ifdef BARTON_CONFIG_SBMD_METRICS

namespace barton
{
    SbmdFactoryMetrics::SbmdFactoryMetrics()
    {
        driverLoadFailureCounter =
            observabilityCounterCreate("sbmd.driver.load.failure", "Number of SBMD driver loads that failed", "1");
        driverLoadDurationHisto = observabilityHistogramCreate(
            "sbmd.driver.load.duration_ms", "Time to load and activate an SBMD driver", "ms");
        registeredDriversGauge = observabilityGaugeCreate(
            "sbmd.driver.registered.count", "Number of SBMD drivers successfully registered", "1");
        registrationTotalHisto =
            observabilityHistogramCreate("sbmd.driver.registration.total_ms",
                                         "Total wall time to discover, load, activate, and register all SBMD drivers",
                                         "ms");
        bundleLoadHisto =
            observabilityHistogramCreate("sbmd.driver.bundle_load_ms",
                                         "Time to load the SBMD utilities bundle and inject the capture function",
                                         "ms");
    }

    void SbmdFactoryMetrics::RecordDriverLoadFailure(const char *driver, const char *reason)
    {
        observabilityCounterAddWithAttrs(driverLoadFailureCounter, 1, "driver", driver, "reason", reason, nullptr);
    }

    void SbmdFactoryMetrics::RecordDriverLoadSuccess(double durationMs, const char *driver)
    {
        observabilityHistogramRecordWithAttrs(driverLoadDurationHisto, durationMs, "driver", driver, nullptr);
    }

    void SbmdFactoryMetrics::RecordRegisteredDriverCount(int64_t count)
    {
        observabilityGaugeRecord(registeredDriversGauge, count);
    }

    void SbmdFactoryMetrics::RecordRegistrationTotal(double durationMs)
    {
        observabilityHistogramRecord(registrationTotalHisto, durationMs);
    }

    void SbmdFactoryMetrics::RecordBundleLoad(double durationMs)
    {
        observabilityHistogramRecord(bundleLoadHisto, durationMs);
    }

} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
