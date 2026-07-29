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

#include <cstddef>
#include <cstdint>

extern "C" {
#include <mquickjs/mquickjs.h>
}

#ifdef BARTON_CONFIG_SBMD_METRICS

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "observability/observabilityMetrics.h"

namespace barton
{
    /**
     * Observability metrics for the mquickjs JS runtime: heap pool health,
     * GC activity, mutex contention, and JS exceptions.
     */
    class MQuickJsRuntimeMetrics
    {
    public:
        MQuickJsRuntimeMetrics();

        /**
         * Start the background idle heap sampler.
         * No-op if BARTON_CONFIG_SBMD_METRICS_HEAP_SAMPLE_PERIOD_MS <= 0.
         */
        void StartSampler();

        /**
         * Stop and join the background idle heap sampler.
         */
        void StopSampler();

        /**
         * Synchronously sample heap stats and record pool health metrics.
         * Acquires the JS mutex internally. No-op if context not ready or
         * handles not initialized.
         */
        void ForceSnapshot();

        /**
         * Reset the idle sampler timer without waiting for the full period.
         * Safe to call while holding the JS mutex.
         */
        void TickleSampler();

        /**
         * Record pool health metrics from an already-captured JSMemoryUsage.
         * Must be called while holding MQuickJsRuntime::GetMutex().
         */
        void RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount);

        /**
         * Record the arena size gauge (called once at initialization time
         * with the configured memory-pool size).
         */
        void RecordArenaSize(int64_t bytes);

        /** Record a JS mutex wait duration in milliseconds. */
        void RecordMutexWait(double ms);

        /**
         * Record a JS exception event.
         * @param phase  "init" (bundle/polyfill load), "loading" (per-driver eval),
         *               or "invocation" (runtime handler exception)
         * @param driver Filename stem of the affected driver, or nullptr to omit
         *               the "driver" attribute from the event
         */
        void RecordJsException(const char *phase, const char *driver);

        /**
         * GC instrumentation callback — registered with JS_SetGCCallback with
         * this instance as opaque. Called by the mquickjs engine at GC start
         * (isEnd == 0) and end (isEnd == 1).
         */
        static void GCCallback(JSContext *ctx, int isEnd, void *opaque) noexcept;

    private:
        ObservabilityHistogram *heapUsedHisto = nullptr;
        ObservabilityGauge *heapArenaGauge = nullptr;
        ObservabilityGauge *heapFreeGauge = nullptr;
        ObservabilityGauge *heapPeakGauge = nullptr;
        ObservabilityHistogram *mutexWaitHisto = nullptr;
        ObservabilityCounter *jsExceptionCounter = nullptr;
        ObservabilityCounter *gcCountCounter = nullptr;
        ObservabilityHistogram *gcDurationHisto = nullptr;
        ObservabilityGauge *gcRootsGauge = nullptr;

        void RunSampler();

        std::thread periodicSamplerThread;
        std::atomic<bool> samplerShouldStop {false};
        std::atomic<uint64_t> tickleSeq {0};
        std::condition_variable samplerCv;
        std::mutex samplerCvMutex;
        std::chrono::steady_clock::time_point gcStartTime;
        int64_t peakHeapRecorded = 0;
    };

} // namespace barton

#else

namespace barton
{
    class MQuickJsRuntimeMetrics
    {
    public:
        void StartSampler() {}

        void StopSampler() {}

        void ForceSnapshot() {}

        void TickleSampler() {}

        void RecordHeapSnapshot(const JSMemoryUsage &, size_t) {}

        void RecordArenaSize(int64_t) {}

        void RecordMutexWait(double) {}

        void RecordJsException(const char *, const char *) {}
        static void GCCallback(JSContext *, int, void *) noexcept {}
    };
} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
