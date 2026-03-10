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

#include "observability/observability.h"

#ifdef BARTON_CONFIG_OBSERVABILITY

#include <cstdlib>
#include <string>

extern "C" {
#include <icLog/logging.h>
}
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace otlp = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;

#define LOG_TAG     "observability"
#define logFmt(fmt) "%s: " fmt, __func__

namespace
{
    const char *kDefaultEndpoint = "http://localhost:4318";
    const char *kDefaultServiceName = "barton-core";

    std::string getEnvOr(const char *name, const char *defaultValue)
    {
        const char *val = std::getenv(name);
        return (val != nullptr && val[0] != '\0') ? std::string(val) : std::string(defaultValue);
    }
} // namespace

extern "C" int observabilityInit(void)
{
    std::string endpoint = getEnvOr("OTEL_EXPORTER_OTLP_ENDPOINT", kDefaultEndpoint);
    std::string serviceName = getEnvOr("OTEL_SERVICE_NAME", kDefaultServiceName);

    icInfo("Initializing OpenTelemetry: endpoint=%s, service=%s", endpoint.c_str(), serviceName.c_str());

    // Build resource attributes
    auto resourceAttrs = resource::ResourceAttributes {
        {"service.name", serviceName}
    };
    auto otelResource = resource::Resource::Create(resourceAttrs);

    // --- Tracer Provider ---
    otlp::OtlpHttpExporterOptions traceOpts;
    traceOpts.url = endpoint + "/v1/traces";
    auto traceExporter = otlp::OtlpHttpExporterFactory::Create(traceOpts);
    auto traceProcessor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(traceExporter));
    auto tracerProvider = trace_sdk::TracerProviderFactory::Create(std::move(traceProcessor), otelResource);
    trace_api::Provider::SetTracerProvider(std::shared_ptr<trace_api::TracerProvider>(std::move(tracerProvider)));

    // --- Meter Provider ---
    otlp::OtlpHttpMetricExporterOptions metricOpts;
    metricOpts.url = endpoint + "/v1/metrics";
    auto metricExporter = otlp::OtlpHttpMetricExporterFactory::Create(metricOpts);
    metrics_sdk::PeriodicExportingMetricReaderOptions readerOpts;
    readerOpts.export_interval_millis = std::chrono::milliseconds(10000);
    readerOpts.export_timeout_millis = std::chrono::milliseconds(5000);
    auto metricReader =
        metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(metricExporter), readerOpts);
    auto meterProvider =
        metrics_sdk::MeterProviderFactory::Create(std::make_unique<metrics_sdk::ViewRegistry>(), otelResource);
    meterProvider->AddMetricReader(std::move(metricReader));
    metrics_api::Provider::SetMeterProvider(std::shared_ptr<metrics_api::MeterProvider>(std::move(meterProvider)));

    // --- Logger Provider ---
    otlp::OtlpHttpLogRecordExporterOptions logOpts;
    logOpts.url = endpoint + "/v1/logs";
    auto logExporter = otlp::OtlpHttpLogRecordExporterFactory::Create(logOpts);
    auto logProcessor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(logExporter));
    auto loggerProvider = logs_sdk::LoggerProviderFactory::Create(std::move(logProcessor), otelResource);
    logs_api::Provider::SetLoggerProvider(std::shared_ptr<logs_api::LoggerProvider>(std::move(loggerProvider)));

    icInfo("OpenTelemetry initialized successfully");

    return 0;
}

extern "C" void observabilityShutdown(void)
{
    icInfo("Shutting down OpenTelemetry");

    // Flush and reset providers. The SDK handles flushing on shutdown.
    auto tracerProvider = trace_api::Provider::GetTracerProvider();
    if (auto *sdkProvider = dynamic_cast<trace_sdk::TracerProvider *>(tracerProvider.get()))
    {
        sdkProvider->ForceFlush();
    }

    auto meterProvider = metrics_api::Provider::GetMeterProvider();
    // MeterProvider doesn't expose ForceFlush via base, but shutdown triggers flush

    // Reset to no-op providers
    trace_api::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(new trace_api::NoopTracerProvider()));
    metrics_api::Provider::SetMeterProvider(
        opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(new metrics_api::NoopMeterProvider()));
    logs_api::Provider::SetLoggerProvider(
        opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(new logs_api::NoopLoggerProvider()));
}

#endif /* BARTON_CONFIG_OBSERVABILITY */
