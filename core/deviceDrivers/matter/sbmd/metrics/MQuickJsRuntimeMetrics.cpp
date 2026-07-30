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

#define LOG_TAG     "MQuickJsRuntimeMetrics"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "MQuickJsRuntimeMetrics.h"
#include "matter/sbmd/mquickjs/MQuickJsRuntime.h"

#include <system_error>

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{

#ifdef BARTON_CONFIG_SBMD_METRICS

    // ── Handle initialization ──────────────────────────────────────────────────

    MQuickJsRuntimeMetrics::MQuickJsRuntimeMetrics()
    {
        heapUsedHisto = observabilityHistogramCreate(
            "sbmd.js.heap.used_bytes", "Distribution of mquickjs heap bytes in use at each handler invocation", "By");
        heapArenaGauge =
            observabilityGaugeCreate("sbmd.js.heap.arena_bytes", "Total mquickjs arena size in bytes", "By");
        heapFreeGauge = observabilityGaugeCreate(
            "sbmd.js.heap.free_bytes", "Bytes between top of heap and bottom of JS call stack", "By");
        heapPeakGauge = observabilityGaugeCreate(
            "sbmd.js.heap.peak_bytes", "All-time peak heap bytes observed since last init", "By");
        mutexWaitHisto = observabilityHistogramCreate(
            "sbmd.js.mutex.wait_ms", "Time spent waiting to acquire the JS mutex before a handler call", "ms");
        jsExceptionCounter =
            observabilityCounterCreate("sbmd.js.exception", "Number of JavaScript exceptions encountered", "1");
#ifdef BARTON_CONFIG_SBMD_GC_INSTRUMENTATION
        gcCountCounter = observabilityCounterCreate("sbmd.js.gc.count", "Number of GC cycles completed", "1");
        gcDurationHisto = observabilityHistogramCreate("sbmd.js.gc.duration_ms", "Duration of each GC cycle", "ms");
        gcRootsGauge = observabilityGaugeCreate(
            "sbmd.js.gc_roots", "Live GC roots registered on the JS context (push/pop + add/delete lists)", "1");
#endif
    }

    // ── Sampler ───────────────────────────────────────────────────────────────

    void MQuickJsRuntimeMetrics::StartSampler()
    {
        if (BARTON_CONFIG_SBMD_METRICS_HEAP_SAMPLE_PERIOD_MS <= 0)
        {
            return;
        }

        samplerShouldStop.store(false, std::memory_order_relaxed);

        try
        {
            periodicSamplerThread = std::thread(&MQuickJsRuntimeMetrics::RunSampler, this);
        }
        catch (const std::system_error &e)
        {
            icWarn("Failed to start background heap sampler: %s — periodic sampling disabled", e.what());
        }
    }

    void MQuickJsRuntimeMetrics::RunSampler()
    {
        using clock = std::chrono::steady_clock;

        std::unique_lock<std::mutex> lock(samplerCvMutex);

        // Each outer iteration is one idle-wait cycle. On activity (tickle) we
        // restart so the idle timer begins fresh from the moment of last activity.
        // The loop exits only when stop is requested.
        while (!samplerShouldStop.load(std::memory_order_relaxed))
        {
            auto deadline = clock::now() + std::chrono::milliseconds(BARTON_CONFIG_SBMD_METRICS_HEAP_SAMPLE_PERIOD_MS);
            auto lastTickle = tickleSeq.load(std::memory_order_relaxed);

            // Predicate-based wait handles spurious wakeups internally.
            // Returns false (timeout) when the deadline elapses without the
            // predicate firing; returns true on stop or tickle activity.
            bool timeout = !samplerCv.wait_until(lock, deadline, [&]() {
                return samplerShouldStop.load(std::memory_order_relaxed) ||
                       tickleSeq.load(std::memory_order_relaxed) != lastTickle;
            });

            if (timeout)
            {
                // Timeout: no handler activity for the full idle period — take a
                // snapshot, then restart the outer loop to arm the next deadline.
                lock.unlock();
                ForceSnapshot();
                lock.lock();
            }

            // Stop or tickle: fall through to restart the outer loop. The while
            // condition handles stop; a tickle resets the idle deadline from now.
        }
    }

    void MQuickJsRuntimeMetrics::StopSampler()
    {
        samplerShouldStop.store(true, std::memory_order_relaxed);
        TickleSampler();

        if (periodicSamplerThread.joinable())
        {
            periodicSamplerThread.join();
        }
    }

    void MQuickJsRuntimeMetrics::ForceSnapshot()
    {
        if (!MQuickJsRuntime::IsContextReady())
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            JSContext *ctx = MQuickJsRuntime::GetSharedContext();

            if (!ctx)
            {
                return;
            }

            auto usage = MQuickJsRuntime::GetMemoryUsage(ctx, 0);

            if (!usage)
            {
                return;
            }

            MQuickJsRuntime::RecordHeapSnapshot(*usage);
        }

        TickleSampler();
    }

    void MQuickJsRuntimeMetrics::TickleSampler()
    {
        tickleSeq.fetch_add(1, std::memory_order_relaxed);
        samplerCv.notify_one();
    }

    // ── Recording ─────────────────────────────────────────────────────────────

    void MQuickJsRuntimeMetrics::RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount)
    {
        observabilityHistogramRecord(heapUsedHisto, static_cast<double>(usage.heap_used));
        observabilityGaugeRecord(heapFreeGauge, static_cast<int64_t>(usage.free_size));

        if (static_cast<int64_t>(usage.heap_used) > peakHeapRecorded)
        {
            peakHeapRecorded = static_cast<int64_t>(usage.heap_used);
        }

        observabilityGaugeRecord(heapPeakGauge, peakHeapRecorded);
        observabilityGaugeRecord(gcRootsGauge, static_cast<int64_t>(gcRootCount));
    }

    void MQuickJsRuntimeMetrics::RecordArenaSize(int64_t bytes)
    {
        observabilityGaugeRecord(heapArenaGauge, bytes);
    }

    void MQuickJsRuntimeMetrics::RecordMutexWait(double ms)
    {
        observabilityHistogramRecord(mutexWaitHisto, ms);
    }

    void MQuickJsRuntimeMetrics::RecordJsException(const char *phase, const char *driver)
    {
        if (driver)
        {
            observabilityCounterAddWithAttrs(jsExceptionCounter, 1, "phase", phase, "driver", driver, nullptr);
        }
        else
        {
            observabilityCounterAddWithAttrs(jsExceptionCounter, 1, "phase", phase, nullptr);
        }
    }

    void MQuickJsRuntimeMetrics::GCCallback(JSContext * /*ctx*/, int isEnd, void *opaque) noexcept
    {
        auto *metrics = static_cast<MQuickJsRuntimeMetrics *>(opaque);

        if (!metrics)
        {
            return;
        }

        if (isEnd == 0)
        {
            metrics->gcStartTime = std::chrono::steady_clock::now();

            return;
        }

        observabilityCounterAdd(metrics->gcCountCounter, 1);

        auto elapsed = std::chrono::steady_clock::now() - metrics->gcStartTime;
        double ms = std::chrono::duration<double, std::milli>(elapsed).count();

        observabilityHistogramRecord(metrics->gcDurationHisto, ms);
    }

#endif // BARTON_CONFIG_SBMD_METRICS

} // namespace barton
