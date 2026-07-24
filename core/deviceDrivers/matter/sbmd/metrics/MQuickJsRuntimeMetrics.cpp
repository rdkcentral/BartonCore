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
#include "MetricsRegistry.h"
#include "matter/sbmd/mquickjs/MQuickJsRuntime.h"

#include <system_error>

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{

#ifdef BARTON_CONFIG_SBMD_METRICS

    // ── Static member initialization ─────────────────────────────────────────

    ObservabilityHistogram *MQuickJsRuntimeMetrics::heapUsedHisto = nullptr;
    ObservabilityGauge *MQuickJsRuntimeMetrics::heapArenaGauge = nullptr;
    ObservabilityGauge *MQuickJsRuntimeMetrics::heapFreeGauge = nullptr;
    ObservabilityGauge *MQuickJsRuntimeMetrics::heapPeakGauge = nullptr;
    ObservabilityHistogram *MQuickJsRuntimeMetrics::mutexWaitHisto = nullptr;
    ObservabilityCounter *MQuickJsRuntimeMetrics::jsExceptionCounter = nullptr;
    ObservabilityCounter *MQuickJsRuntimeMetrics::gcCountCounter = nullptr;
    ObservabilityHistogram *MQuickJsRuntimeMetrics::gcDurationHisto = nullptr;
    ObservabilityGauge *MQuickJsRuntimeMetrics::gcRootsGauge = nullptr;
    std::chrono::steady_clock::time_point MQuickJsRuntimeMetrics::gcStartTime {};
    std::thread MQuickJsRuntimeMetrics::periodicSamplerThread;
    std::atomic<bool> MQuickJsRuntimeMetrics::samplerShouldStop {false};
    std::atomic<uint64_t> MQuickJsRuntimeMetrics::tickleSeq {0};
    std::condition_variable MQuickJsRuntimeMetrics::samplerCv;
    std::mutex MQuickJsRuntimeMetrics::samplerCvMutex;
    int64_t MQuickJsRuntimeMetrics::peakHeapRecorded = 0;

    // Self-register with MetricsRegistry before main().
    static bool s_registered = (MetricsRegistry::registerProvider({MQuickJsRuntimeMetrics::InitializeMetrics,
                                                                   MQuickJsRuntimeMetrics::ShutdownMetrics}),
                                true);

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    void MQuickJsRuntimeMetrics::InitializeMetrics()
    {
        if (heapUsedHisto != nullptr)
        {
            return; // Already initialized — guard against double-init on retry paths
        }

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
        gcCountCounter = observabilityCounterCreate("sbmd.js.gc.count", "Number of GC cycles completed", "1");
        gcDurationHisto = observabilityHistogramCreate("sbmd.js.gc.duration_ms", "Duration of each GC cycle", "ms");
        gcRootsGauge = observabilityGaugeCreate(
            "sbmd.js.gc_roots", "Live GC roots registered on the JS context (push/pop + add/delete lists)", "1");
        peakHeapRecorded = 0;
    }

    void MQuickJsRuntimeMetrics::ShutdownMetrics()
    {
        observabilityGaugeRelease(heapArenaGauge);
        heapArenaGauge = nullptr;
        observabilityGaugeRelease(heapFreeGauge);
        heapFreeGauge = nullptr;
        observabilityGaugeRelease(heapPeakGauge);
        heapPeakGauge = nullptr;
        observabilityHistogramRelease(mutexWaitHisto);
        mutexWaitHisto = nullptr;
        observabilityCounterRelease(jsExceptionCounter);
        jsExceptionCounter = nullptr;
        observabilityCounterRelease(gcCountCounter);
        gcCountCounter = nullptr;
        observabilityHistogramRelease(gcDurationHisto);
        gcDurationHisto = nullptr;
        observabilityGaugeRelease(gcRootsGauge);
        gcRootsGauge = nullptr;
        // heapUsedHisto released last — serves as the idempotence sentinel
        observabilityHistogramRelease(heapUsedHisto);
        heapUsedHisto = nullptr;
    }

    // ── Sampler ───────────────────────────────────────────────────────────────

    void MQuickJsRuntimeMetrics::StartSampler()
    {
        if (BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS <= 0)
        {
            return;
        }

        samplerShouldStop.store(false, std::memory_order_relaxed);

        try
        {
            periodicSamplerThread = std::thread([]() {
                using clock = std::chrono::steady_clock;

                std::unique_lock<std::mutex> lock(samplerCvMutex);

                while (!samplerShouldStop.load(std::memory_order_relaxed))
                {
                    auto deadline =
                        clock::now() + std::chrono::milliseconds(BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS);
                    auto lastTickle = tickleSeq.load(std::memory_order_relaxed);

                    while (true)
                    {
                        auto status = samplerCv.wait_until(lock, deadline);

                        if (samplerShouldStop.load(std::memory_order_relaxed))
                        {
                            return;
                        }

                        if (status == std::cv_status::timeout)
                        {
                            lock.unlock();
                            ForceSnapshot();
                            lock.lock();
                            break;
                        }

                        auto curTickle = tickleSeq.load(std::memory_order_relaxed);

                        if (curTickle != lastTickle)
                        {
                            break;
                        }
                    }
                }
            });
        }
        catch (const std::system_error &e)
        {
            icWarn("Failed to start background heap sampler: %s — periodic sampling disabled", e.what());
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

        if (!heapUsedHisto || !heapFreeGauge || !heapPeakGauge || !gcRootsGauge)
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

            JSMemoryUsage usage = {};
            size_t gcRootCount = JS_GetGCRootCount(ctx);
            JS_GetMemoryUsage(ctx, &usage, 0);
            RecordHeapSnapshot(usage, gcRootCount);
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
        if (!heapUsedHisto)
        {
            return;
        }

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
        if (!heapArenaGauge)
        {
            return;
        }

        observabilityGaugeRecord(heapArenaGauge, bytes);
    }

    void MQuickJsRuntimeMetrics::RecordMutexWait(double ms)
    {
        if (!mutexWaitHisto)
        {
            return;
        }

        observabilityHistogramRecord(mutexWaitHisto, ms);
    }

    void MQuickJsRuntimeMetrics::RecordJsException(const char *phase, const char *driver)
    {
        if (!jsExceptionCounter)
        {
            return;
        }

        if (driver)
        {
            observabilityCounterAddWithAttrs(jsExceptionCounter, 1, "phase", phase, "driver", driver, nullptr);
        }
        else
        {
            observabilityCounterAddWithAttrs(jsExceptionCounter, 1, "phase", phase, nullptr);
        }
    }

    void MQuickJsRuntimeMetrics::GCCallback(JSContext * /*ctx*/, int isEnd, void * /*opaque*/) noexcept
    {
        if (isEnd == 0)
        {
            gcStartTime = std::chrono::steady_clock::now();

            return;
        }

        if (gcCountCounter)
        {
            observabilityCounterAdd(gcCountCounter, 1);
        }

        if (gcDurationHisto)
        {
            auto elapsed = std::chrono::steady_clock::now() - gcStartTime;
            double ms = std::chrono::duration<double, std::milli>(elapsed).count();

            observabilityHistogramRecord(gcDurationHisto, ms);
        }
    }

} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
