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
     *
     * Self-registers with MetricsRegistry at static-initialization time.
     * InitializeMetrics() / ShutdownMetrics() are called by MetricsRegistry
     * — do not call them directly.
     */
    class MQuickJsRuntimeMetrics
    {
    public:
        static void InitializeMetrics();
        static void ShutdownMetrics();

        /**
         * Start the background idle heap sampler.
         * No-op if BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS <= 0.
         * Called by MQuickJsRuntime::Initialize() after jsContextReady = true.
         */
        static void StartSampler();

        /**
         * Stop and join the background idle heap sampler.
         * Called by MQuickJsRuntime::Shutdown() before freeing the JS context.
         */
        static void StopSampler();

        /**
         * Synchronously sample heap stats and record pool health metrics.
         * Acquires the JS mutex internally. No-op if context not ready or
         * handles not initialized.
         */
        static void ForceSnapshot();

        /**
         * Reset the idle sampler timer without waiting for the full period.
         * Safe to call while holding the JS mutex.
         */
        static void TickleSampler();

        /**
         * Record pool health metrics from an already-captured JSMemoryUsage.
         * Must be called while holding MQuickJsRuntime::GetMutex().
         */
        static void RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount);

        /**
         * Record the arena size gauge (called once at MQuickJsRuntime::Initialize
         * time with the configured memory-pool size).
         */
        static void RecordArenaSize(int64_t bytes);

        /** Record a JS mutex wait duration in milliseconds. */
        static void RecordMutexWait(double ms);

        /**
         * Record a JS exception event.
         * @param phase  "init" or "loading"
         * @param driver Filename stem, or nullptr to omit the "driver" attribute
         */
        static void RecordJsException(const char *phase, const char *driver);

        /**
         * GC instrumentation callback — registered with JS_SetGCCallback.
         * Called by the mquickjs engine at GC start (isEnd == 0) and end
         * (isEnd == 1).
         */
        static void GCCallback(JSContext *ctx, int isEnd, void *opaque) noexcept;

    private:
        static ObservabilityHistogram *heapUsedHisto;
        static ObservabilityGauge *heapArenaGauge;
        static ObservabilityGauge *heapFreeGauge;
        static ObservabilityGauge *heapPeakGauge;
        static ObservabilityHistogram *mutexWaitHisto;
        static ObservabilityCounter *jsExceptionCounter;
        static ObservabilityCounter *gcCountCounter;
        static ObservabilityHistogram *gcDurationHisto;
        static ObservabilityGauge *gcRootsGauge;

        static std::thread periodicSamplerThread;
        static std::atomic<bool> samplerShouldStop;
        static std::atomic<uint64_t> tickleSeq;
        static std::condition_variable samplerCv;
        static std::mutex samplerCvMutex;
        static std::chrono::steady_clock::time_point gcStartTime;
        static int64_t peakHeapRecorded;
    };

} // namespace barton

#else

namespace barton
{
    class MQuickJsRuntimeMetrics
    {
    public:
        static void InitializeMetrics() {}
        static void ShutdownMetrics() {}
        static void StartSampler() {}
        static void StopSampler() {}
        static void ForceSnapshot() {}
        static void TickleSampler() {}
        static void RecordHeapSnapshot(const JSMemoryUsage &, size_t) {}
        static void RecordArenaSize(int64_t) {}
        static void RecordMutexWait(double) {}
        static void RecordJsException(const char *, const char *) {}
        static void GCCallback(JSContext *, int, void *) noexcept {}
    };
} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
