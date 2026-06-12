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

#include "observability/observabilityTracing.h"

#ifdef BARTON_CONFIG_OBSERVABILITY

#include <glib.h>

#include <opentelemetry/context/context.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>

namespace trace_api = opentelemetry::trace;

namespace
{
    const char *kTracerName = "barton-core";
} // namespace

/**
 * Internal span wrapper — holds the OTel span and the scope that keeps
 * the span active on the current thread during its lifetime.
 */
struct ObservabilitySpan
{
    opentelemetry::nostd::shared_ptr<trace_api::Span> span;
    gint ref_count;
};

/**
 * Internal span context wrapper — carries enough information to
 * create child spans from a different call chain or thread.
 */
struct ObservabilitySpanContext
{
    opentelemetry::context::Context context;
    gint ref_count;
};

namespace
{
    opentelemetry::nostd::shared_ptr<trace_api::Tracer> getTracer()
    {
        return trace_api::Provider::GetTracerProvider()->GetTracer(kTracerName);
    }
} // namespace

extern "C" ObservabilitySpan *observabilitySpanStart(const char *name)
{
    if (name == nullptr)
    {
        return nullptr;
    }

    auto tracer = getTracer();
    auto span = tracer->StartSpan(name);

    auto *wrapper = new (std::nothrow) ObservabilitySpan();
    if (wrapper == nullptr)
    {
        span->End();
        return nullptr;
    }
    wrapper->span = span;
    wrapper->ref_count = 1;
    return wrapper;
}

extern "C" ObservabilitySpan *observabilitySpanStartWithParent(const char *name, const ObservabilitySpanContext *parent)
{
    if (name == nullptr)
    {
        return nullptr;
    }

    auto tracer = getTracer();
    trace_api::StartSpanOptions opts;

    opentelemetry::nostd::shared_ptr<trace_api::Span> span;
    if (parent != nullptr)
    {
        opts.parent = trace_api::GetSpan(parent->context)->GetContext();
        span = tracer->StartSpan(name, {}, opts);
    }
    else
    {
        span = tracer->StartSpan(name);
    }

    auto *wrapper = new (std::nothrow) ObservabilitySpan();
    if (wrapper == nullptr)
    {
        span->End();
        return nullptr;
    }
    wrapper->span = span;
    wrapper->ref_count = 1;
    return wrapper;
}

extern "C" ObservabilitySpan *observabilitySpanStartWithLink(const char *name, const ObservabilitySpanContext *linked)
{
    if (name == nullptr)
    {
        return nullptr;
    }

    auto tracer = getTracer();

    opentelemetry::nostd::shared_ptr<trace_api::Span> span;
    if (linked != nullptr)
    {
        auto linkedSpanCtx = trace_api::GetSpan(linked->context)->GetContext();
        std::pair<
            trace_api::SpanContext,
            std::initializer_list<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>>
            link {linkedSpanCtx, {}};
        span = tracer->StartSpan(name, {}, {link});
    }
    else
    {
        span = tracer->StartSpan(name);
    }

    auto *wrapper = new (std::nothrow) ObservabilitySpan();
    if (wrapper == nullptr)
    {
        span->End();
        return nullptr;
    }
    wrapper->span = span;
    wrapper->ref_count = 1;
    return wrapper;
}

extern "C" void observabilitySpanRelease(ObservabilitySpan *span)
{
    if (span == nullptr)
    {
        return;
    }
    if (g_atomic_int_dec_and_test(&span->ref_count))
    {
        span->span->End();
        delete span;
    }
}

extern "C" void observabilitySpanSetAttribute(ObservabilitySpan *span, const char *key, const char *value)
{
    if (span == nullptr || key == nullptr || value == nullptr)
    {
        return;
    }
    span->span->SetAttribute(key, value);
}

extern "C" void observabilitySpanSetAttributeInt(ObservabilitySpan *span, const char *key, int64_t value)
{
    if (span == nullptr || key == nullptr)
    {
        return;
    }
    span->span->SetAttribute(key, value);
}

extern "C" void observabilitySpanSetError(ObservabilitySpan *span, const char *message)
{
    if (span == nullptr)
    {
        return;
    }
    span->span->SetStatus(trace_api::StatusCode::kError, message != nullptr ? message : "");
    if (message != nullptr)
    {
        span->span->AddEvent("exception",
                             {
                                 {"exception.message", message}
        });
    }
}

extern "C" ObservabilitySpanContext *observabilitySpanGetContext(ObservabilitySpan *span)
{
    if (span == nullptr)
    {
        return nullptr;
    }

    auto *ctx = new (std::nothrow) ObservabilitySpanContext();
    if (ctx == nullptr)
    {
        return nullptr;
    }
    // Store the span in a context object for propagation
    ctx->context = opentelemetry::context::RuntimeContext::GetCurrent().SetValue(
        trace_api::kSpanKey, opentelemetry::nostd::shared_ptr<trace_api::Span>(span->span));
    ctx->ref_count = 1;
    return ctx;
}

extern "C" void observabilitySpanContextRef(ObservabilitySpanContext *ctx)
{
    if (ctx == nullptr)
    {
        return;
    }
    g_atomic_int_inc(&ctx->ref_count);
}

extern "C" void observabilitySpanContextRelease(ObservabilitySpanContext *ctx)
{
    if (ctx == nullptr)
    {
        return;
    }
    if (g_atomic_int_dec_and_test(&ctx->ref_count))
    {
        delete ctx;
    }
}

static thread_local ObservabilitySpanContext *tlsCurrentContext = nullptr;

extern "C" void observabilitySpanContextSetCurrent(ObservabilitySpanContext *ctx)
{
    if (tlsCurrentContext != ctx)
    {
        observabilitySpanContextRelease(tlsCurrentContext);
        tlsCurrentContext = ctx;
        observabilitySpanContextRef(tlsCurrentContext);
    }
}

extern "C" ObservabilitySpanContext *observabilitySpanContextGetCurrent(void)
{
    return tlsCurrentContext;
}

#endif /* BARTON_CONFIG_OBSERVABILITY */
