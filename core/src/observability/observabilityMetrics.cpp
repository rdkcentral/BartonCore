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

#include <glib.h>

#include <opentelemetry/metrics/provider.h>

namespace metrics_api = opentelemetry::metrics;

static const char *kMeterName = "barton-core";

static opentelemetry::nostd::shared_ptr<metrics_api::Meter> getMeter()
{
    return metrics_api::Provider::GetMeterProvider()->GetMeter(kMeterName);
}

struct ObservabilityCounter
{
    opentelemetry::nostd::unique_ptr<metrics_api::Counter<uint64_t>> counter;
    gint ref_count;
};

struct ObservabilityGauge
{
    opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument> gauge;
    int64_t currentValue;
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
    wrapper->currentValue = 0;
    wrapper->ref_count = 1;

    auto meter = getMeter();
    wrapper->gauge =
        meter->CreateInt64ObservableGauge(name, description != nullptr ? description : "", unit != nullptr ? unit : "");

    wrapper->gauge->AddCallback(
        [](opentelemetry::metrics::ObserverResult result, void *state) {
            auto *g = static_cast<ObservabilityGauge *>(state);
            auto val = g->currentValue;
            if (opentelemetry::nostd::holds_alternative<
                    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result))
            {
                opentelemetry::nostd::get<
                    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result)
                    ->Observe(val);
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
    gauge->currentValue = value;
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
