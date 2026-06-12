//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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

#include "observability/observabilityMetrics.h"

#ifdef BARTON_CONFIG_OBSERVABILITY

#include <algorithm>
#include <cstdarg>
#include <map>
#include <mutex>
#include <string>

#include <glib.h>

#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/metrics/provider.h>

namespace metrics_api = opentelemetry::metrics;

namespace
{
    const char *kMeterName = "barton-core";

    opentelemetry::nostd::shared_ptr<metrics_api::Meter> getMeter()
    {
        return metrics_api::Provider::GetMeterProvider()->GetMeter(kMeterName);
    }

    /**
     * Parse a NULL-terminated variadic list of (const char *key, const char *value) pairs
     * into owned string pairs.
     */
    std::vector<std::pair<std::string, std::string>> parseVarAttrs(va_list ap)
    {
        std::vector<std::pair<std::string, std::string>> attrs;
        while (true)
        {
            const char *key = va_arg(ap, const char *);
            if (key == nullptr)
            {
                break;
            }
            const char *val = va_arg(ap, const char *);
            if (val != nullptr)
            {
                attrs.emplace_back(key, val);
            }
        }
        return attrs;
    }

    using AttrPair = std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>;

    /**
     * Convert owned string pairs to SDK-compatible attribute pairs.
     * The returned vector contains string_views that reference the input strings,
     * so the input MUST outlive the returned vector.
     */
    std::vector<AttrPair> buildAttrs(const std::vector<std::pair<std::string, std::string>> &attrs)
    {
        std::vector<AttrPair> result;
        result.reserve(attrs.size());
        for (const auto &kv : attrs)
        {
            result.emplace_back(
                opentelemetry::nostd::string_view {
                    kv.first.data(), kv.first.size()
            },
                opentelemetry::common::AttributeValue {
                    opentelemetry::nostd::string_view {kv.second.data(), kv.second.size()}});
        }
        return result;
    }
} // namespace

struct ObservabilityCounter
{
    opentelemetry::nostd::unique_ptr<metrics_api::Counter<uint64_t>> counter;
    gint ref_count;
};

struct ObservabilityGauge
{
    opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument> gauge;
    // Multi-dimensional: each unique attribute combination has its own value.
    // Key: sorted vector of attribute pairs (canonical form for dedup).
    std::map<std::vector<std::pair<std::string, std::string>>, int64_t> observations;
    std::mutex mtx;
    gint ref_count;
};

extern "C" ObservabilityCounter *observabilityCounterCreate(const char *name, const char *description, const char *unit)
{
    if (name == nullptr)
    {
        return nullptr;
    }

    auto meter = getMeter();
    auto counter =
        meter->CreateUInt64Counter(name, description != nullptr ? description : "", unit != nullptr ? unit : "");

    auto *wrapper = new (std::nothrow) ObservabilityCounter();
    if (wrapper == nullptr)
    {
        return nullptr;
    }
    wrapper->counter = std::move(counter);
    wrapper->ref_count = 1;
    return wrapper;
}

extern "C" void observabilityCounterAdd(ObservabilityCounter *counter, uint64_t value)
{
    if (counter == nullptr)
    {
        return;
    }
    counter->counter->Add(value);
}

extern "C" void observabilityCounterAddWithAttrs(ObservabilityCounter *counter, uint64_t value, ...)
{
    if (counter == nullptr)
    {
        return;
    }

    va_list ap;
    va_start(ap, value);
    auto attrs = parseVarAttrs(ap);
    va_end(ap);

    if (attrs.empty())
    {
        counter->counter->Add(value);
    }
    else
    {
        auto sdkAttrs = buildAttrs(attrs);
        opentelemetry::common::KeyValueIterableView<decltype(sdkAttrs)> view {sdkAttrs};
        counter->counter->Add(value, static_cast<const opentelemetry::common::KeyValueIterable &>(view));
    }
}

extern "C" ObservabilityGauge *observabilityGaugeCreate(const char *name, const char *description, const char *unit)
{
    if (name == nullptr)
    {
        return nullptr;
    }

    auto *wrapper = new (std::nothrow) ObservabilityGauge();
    if (wrapper == nullptr)
    {
        return nullptr;
    }
    wrapper->ref_count = 1;

    auto meter = getMeter();
    wrapper->gauge =
        meter->CreateInt64ObservableGauge(name, description != nullptr ? description : "", unit != nullptr ? unit : "");

    wrapper->gauge->AddCallback(
        [](opentelemetry::metrics::ObserverResult result, void *state) {
            auto *g = static_cast<ObservabilityGauge *>(state);
            std::map<std::vector<std::pair<std::string, std::string>>, int64_t> snapshot;
            {
                std::lock_guard<std::mutex> lock(g->mtx);
                snapshot = g->observations;
            }
            if (opentelemetry::nostd::holds_alternative<
                    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result))
            {
                auto observer = opentelemetry::nostd::get<
                    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result);
                for (const auto &entry : snapshot)
                {
                    if (entry.first.empty())
                    {
                        observer->Observe(entry.second);
                    }
                    else
                    {
                        auto sdkAttrs = buildAttrs(entry.first);
                        opentelemetry::common::KeyValueIterableView<decltype(sdkAttrs)> view {sdkAttrs};
                        observer->Observe(entry.second,
                                          static_cast<const opentelemetry::common::KeyValueIterable &>(view));
                    }
                }
            }
        },
        wrapper);

    return wrapper;
}

extern "C" void observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value)
{
    if (gauge == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(gauge->mtx);
    // Empty key = no attributes
    gauge->observations[{}] = value;
}

extern "C" void observabilityGaugeRecordWithAttrs(ObservabilityGauge *gauge, int64_t value, ...)
{
    if (gauge == nullptr)
    {
        return;
    }

    va_list ap;
    va_start(ap, value);
    auto attrs = parseVarAttrs(ap);
    va_end(ap);

    // Sort attributes to canonicalize the key
    std::sort(attrs.begin(), attrs.end());

    std::lock_guard<std::mutex> lock(gauge->mtx);
    gauge->observations[attrs] = value;
}

struct ObservabilityHistogram
{
    opentelemetry::nostd::unique_ptr<metrics_api::Histogram<double>> histogram;
    gint ref_count;
};

extern "C" ObservabilityHistogram *
observabilityHistogramCreate(const char *name, const char *description, const char *unit)
{
    if (name == nullptr)
    {
        return nullptr;
    }

    auto meter = getMeter();
    auto histogram =
        meter->CreateDoubleHistogram(name, description != nullptr ? description : "", unit != nullptr ? unit : "");

    auto *wrapper = new (std::nothrow) ObservabilityHistogram();
    if (wrapper == nullptr)
    {
        return nullptr;
    }
    wrapper->histogram = std::move(histogram);
    wrapper->ref_count = 1;
    return wrapper;
}

extern "C" void observabilityHistogramRecord(ObservabilityHistogram *histogram, double value)
{
    if (histogram == nullptr)
    {
        return;
    }
    histogram->histogram->Record(value, opentelemetry::context::Context {});
}

extern "C" void observabilityHistogramRecordWithAttrs(ObservabilityHistogram *histogram, double value, ...)
{
    if (histogram == nullptr)
    {
        return;
    }

    va_list ap;
    va_start(ap, value);
    auto attrs = parseVarAttrs(ap);
    va_end(ap);

    if (attrs.empty())
    {
        histogram->histogram->Record(value, opentelemetry::context::Context {});
    }
    else
    {
        auto sdkAttrs = buildAttrs(attrs);
        opentelemetry::common::KeyValueIterableView<decltype(sdkAttrs)> view {sdkAttrs};
        histogram->histogram->Record(value,
                                     static_cast<const opentelemetry::common::KeyValueIterable &>(view),
                                     opentelemetry::context::Context {});
    }
}

extern "C" void observabilityHistogramRelease(ObservabilityHistogram *histogram)
{
    if (histogram == nullptr)
    {
        return;
    }
    if (g_atomic_int_dec_and_test(&histogram->ref_count))
    {
        delete histogram;
    }
}

extern "C" void observabilityCounterRelease(ObservabilityCounter *counter)
{
    if (counter == nullptr)
    {
        return;
    }
    if (g_atomic_int_dec_and_test(&counter->ref_count))
    {
        delete counter;
    }
}

extern "C" void observabilityGaugeRelease(ObservabilityGauge *gauge)
{
    if (gauge == nullptr)
    {
        return;
    }
    if (g_atomic_int_dec_and_test(&gauge->ref_count))
    {
        delete gauge;
    }
}

#endif /* BARTON_CONFIG_OBSERVABILITY */
