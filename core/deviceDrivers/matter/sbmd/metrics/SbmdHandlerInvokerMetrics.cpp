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

#include "SbmdHandlerInvokerMetrics.h"

#ifdef BARTON_CONFIG_SBMD_METRICS

namespace barton
{
    SbmdHandlerInvokerMetrics::SbmdHandlerInvokerMetrics()
    {
        handlerDurationHisto = observabilityHistogramCreate(
            "sbmd.handler.duration_ms", "Time from JS_Call entry to return for each handler invocation", "ms");
        handlerHeapDeltaHisto = observabilityHistogramCreate(
            "sbmd.handler.heap_delta_bytes", "Change in heap_used across a single handler call", "By");
        handlerOutcomeCounter =
            observabilityCounterCreate("sbmd.handler.outcome", "Count of handler invocations by outcome", "1");
    }

    void SbmdHandlerInvokerMetrics::RecordInvocation(double durationMs,
                                                     std::optional<double> heapDelta,
                                                     const char *driver,
                                                     const char *opType,
                                                     const char *resourceId)
    {
        auto recordHisto = [&](ObservabilityHistogram *histo, double value) {
            if (!histo)
            {
                return;
            }

            if (!driver && !opType)
            {
                observabilityHistogramRecord(histo, value);

                return;
            }

            if (resourceId)
            {
                observabilityHistogramRecordWithAttrs(histo,
                                                      value,
                                                      "driver",
                                                      driver ? driver : "",
                                                      "op_type",
                                                      opType ? opType : "",
                                                      "resource_id",
                                                      resourceId,
                                                      nullptr);
            }
            else
            {
                observabilityHistogramRecordWithAttrs(
                    histo, value, "driver", driver ? driver : "", "op_type", opType ? opType : "", nullptr);
            }
        };

        recordHisto(handlerDurationHisto, durationMs);

        if (heapDelta.has_value())
        {
            recordHisto(handlerHeapDeltaHisto, *heapDelta);
        }
    }

    void SbmdHandlerInvokerMetrics::RecordOutcome(const char *driver,
                                                  const char *opType,
                                                  const char *resourceId,
                                                  const char *outcome)
    {
        if (driver || opType || resourceId)
        {
            if (resourceId)
            {
                observabilityCounterAddWithAttrs(handlerOutcomeCounter,
                                                 1,
                                                 "driver",
                                                 driver ? driver : "",
                                                 "op_type",
                                                 opType ? opType : "",
                                                 "resource_id",
                                                 resourceId,
                                                 "outcome",
                                                 outcome,
                                                 nullptr);
            }
            else
            {
                observabilityCounterAddWithAttrs(handlerOutcomeCounter,
                                                 1,
                                                 "driver",
                                                 driver ? driver : "",
                                                 "op_type",
                                                 opType ? opType : "",
                                                 "outcome",
                                                 outcome,
                                                 nullptr);
            }
        }
        else
        {
            observabilityCounterAddWithAttrs(handlerOutcomeCounter, 1, "outcome", outcome, nullptr);
        }
    }

} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
