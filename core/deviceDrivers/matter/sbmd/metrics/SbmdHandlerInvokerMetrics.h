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

#ifdef BARTON_CONFIG_SBMD_METRICS

#include "observability/observabilityMetrics.h"

namespace barton
{
    /**
     * Observability metrics for SbmdHandlerInvoker: per-invocation timing,
     * heap impact, and outcome counts.
     *
     * Self-registers with MetricsRegistry at static-initialization time.
     * InitializeMetrics() / ShutdownMetrics() are called by MetricsRegistry
     * — do not call them directly.
     */
    class SbmdHandlerInvokerMetrics
    {
    public:
        static void InitializeMetrics();
        static void ShutdownMetrics();

        /**
         * Record handler invocation timing and heap impact.
         * @param resourceId  nullptr to omit the "resource_id" attribute
         */
        static void RecordInvocation(double durationMs,
                                     double heapDelta,
                                     const char *driver,
                                     const char *opType,
                                     const char *resourceId);

        /**
         * Record a handler outcome (success, exception, timeout, stack_overflow,
         * error).
         * @param resourceId  nullptr to omit the "resource_id" attribute
         */
        static void RecordOutcome(const char *driver, const char *opType, const char *resourceId, const char *outcome);

    private:
        static ObservabilityHistogram *handlerDurationHisto;
        static ObservabilityHistogram *handlerHeapDeltaHisto;
        static ObservabilityCounter *handlerOutcomeCounter;
    };

} // namespace barton

#else

namespace barton
{
    class SbmdHandlerInvokerMetrics
    {
    public:
        static void InitializeMetrics() {}
        static void ShutdownMetrics() {}
        static void RecordInvocation(double, double, const char *, const char *, const char *) {}
        static void RecordOutcome(const char *, const char *, const char *, const char *) {}
    };
} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
