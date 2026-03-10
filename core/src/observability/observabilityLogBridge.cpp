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

#include "observability/observabilityLogBridge.h"

#ifdef BARTON_CONFIG_OBSERVABILITY

#include <atomic>

#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>

namespace logs_api = opentelemetry::logs;
namespace trace_api = opentelemetry::trace;

namespace
{
    const char *kLoggerName = "barton-core";

    /**
     * Maps icLog priority values to OpenTelemetry severity.
     * icLog priorities (from logging.h):
     *   IC_LOG_TRACE = 0, IC_LOG_DEBUG = 1, IC_LOG_INFO = 2,
     *   IC_LOG_WARN = 3, IC_LOG_ERROR = 4, IC_LOG_NONE = 5
     */
    logs_api::Severity mapPriority(int priority)
    {
        switch (priority)
        {
            case 0:
                return logs_api::Severity::kTrace;
            case 1:
                return logs_api::Severity::kDebug;
            case 2:
                return logs_api::Severity::kInfo;
            case 3:
                return logs_api::Severity::kWarn;
            case 4:
                return logs_api::Severity::kError;
            default:
                return logs_api::Severity::kInfo;
        }
    }

    std::atomic<bool> g_bridgeInitialized {false};
} // namespace

extern "C" int observabilityLogBridgeInit(void)
{
    g_bridgeInitialized.store(true, std::memory_order_release);
    return 0;
}

extern "C" void observabilityLogBridgeEmit(const char *category,
                                   int priority,
                                   const char *file,
                                   const char *func,
                                   int line,
                                   const char *message)
{
    if (!g_bridgeInitialized.load(std::memory_order_acquire))
    {
        return;
    }

    auto provider = logs_api::Provider::GetLoggerProvider();
    auto logger = provider->GetLogger(kLoggerName, "", category != nullptr ? category : "");

    auto severity = mapPriority(priority);

    auto record = logger->CreateLogRecord();
    if (record == nullptr)
    {
        return;
    }

    record->SetSeverity(severity);
    if (message != nullptr)
    {
        record->SetBody(message);
    }

    // Add source location as attributes
    if (file != nullptr)
    {
        record->SetAttribute("code.filepath", file);
    }
    if (func != nullptr)
    {
        record->SetAttribute("code.function", func);
    }
    if (line > 0)
    {
        record->SetAttribute("code.lineno", static_cast<int64_t>(line));
    }

    // Trace-log correlation: attach current span context if available
    auto currentSpan = trace_api::GetSpan(opentelemetry::context::RuntimeContext::GetCurrent());
    if (currentSpan != nullptr && currentSpan->GetContext().IsValid())
    {
        record->SetTraceId(currentSpan->GetContext().trace_id());
        record->SetSpanId(currentSpan->GetContext().span_id());
        record->SetTraceFlags(currentSpan->GetContext().trace_flags());
    }

    logger->EmitLogRecord(std::move(record));
}

extern "C" void observabilityLogBridgeShutdown(void)
{
    g_bridgeInitialized.store(false, std::memory_order_release);
}

#endif /* BARTON_CONFIG_OBSERVABILITY */
