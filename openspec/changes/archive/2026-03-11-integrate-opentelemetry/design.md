## Context

BartonCore is a C99/C++17 IoT device management library with no structured observability infrastructure. The current logging facade (`icLog` from BartonCommon) outputs plain-text lines to stdout. A set of `TELEMETRY_COUNTER`/`TELEMETRY_VALUE_*` macros exist but compile as no-ops outside RDK platforms. There is no distributed tracing, no metrics collection, and no correlation of operations across subsystem boundaries (Zigbee, Matter, Thread).

The codebase already uses a pattern of optional subsystems gated by CMake flags (`BCORE_ZIGBEE`, `BCORE_MATTER`, `BCORE_THREAD`), each conditionally compiled and linked. The Matter subsystem demonstrates C++ code compiled alongside C99 core code via the object library pattern. The system uses a hybrid threading model: pthreads for synchronization, a GLib main loop for event dispatch, and the Matter SDK's own event loop.

OpenTelemetry C++ SDK (`opentelemetry-cpp`) provides tracing, metrics, and log signals. Since the core library is C99 but already links C++ (Matter subsystem), adding a C++ dependency is architecturally consistent.

## Goals / Non-Goals

**Goals:**
- Provide opt-in distributed tracing across key BartonCore operations (discovery, commissioning, resource reads/writes, firmware upgrades, subsystem init/shutdown)
- Provide operational metrics (device counts, comm-fail rates, discovery durations, event throughput)
- Bridge `icLog` output to OpenTelemetry's Logs SDK for structured, exportable log records
- Export all signals via OTLP/HTTP to any OpenTelemetry-compatible collector
- Maintain zero overhead when `BCORE_OBSERVABILITY=OFF` (default)

**Non-Goals:**
- Instrumenting third-party code (Matter SDK, GLib, ZHAL)
- Providing collection or visualization infrastructure
- Adding public GObject API surface for observability
- Refactoring existing `icLogDebug`/`icLogError` call sites
- Modifying the legacy `TELEMETRY_COUNTER`/`TELEMETRY_VALUE_*` macro system — it will coexist alongside OpenTelemetry unchanged
- Providing a production observability backend — the reference app's local visualizer is for development/demo only
- Supporting non-OTLP exporters (Jaeger, Zipkin direct) — OTLP is the standard path

## Decisions

### Decision 1: C adapter layer wrapping opentelemetry-cpp

**Choice:** Create a thin C99 header/C++ implementation adapter in `core/src/observability/` that exposes tracing, metrics, and log-bridge functions through C-linkage (`extern "C"`) APIs.

**Rationale:** The core codebase is C99 — device service, drivers, and most subsystem code cannot call C++ APIs directly. The Matter subsystem already establishes the pattern of C++ `.cpp` files compiled alongside C `.c` files in the same object library. A C adapter lets any C source file create spans or record metrics with a simple function call, while the C++ implementation stays encapsulated.

**Alternatives considered:**
- *Pure C SDK*: OpenTelemetry does not have an official C SDK. Community efforts exist but are immature and poorly maintained.
- *Preprocessor-only approach*: Using macros that expand to nothing or to otel calls — too limited for span lifecycle management and context propagation.

**Architecture:**

```
┌─────────────────────────────────────────────────────────┐
│                    C99 Source Files                      │
│  deviceService.c  │  driverManager.c  │  commWatchdog.c │
│                                                         │
│   #include "observability/observabilityTracing.h"               │
│   g_autoptr(ObservabilitySpan) span = observabilitySpanStart("discover"); │
│   ...                                                   │
│   /* span released automatically at scope exit */       │
└──────────────────────┬──────────────────────────────────┘
                       │ C-linkage calls
┌──────────────────────▼──────────────────────────────────┐
│           core/src/observability/  (C++ impl)           │
│                                                         │
│  observabilityTracing.h / .cpp   ← Span lifecycle, context     │
│  observabilityMetrics.h / .cpp   ← Counters, gauges            │
│  observabilityLogBridge.h / .cpp ← icLog → OTel Logs bridge    │
│  observability.h/.cpp ← Init/shutdown, config     │
│                                                         │
│  All headers: extern "C" { ... }                        │
│  All impls: #include <opentelemetry/...>                │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│            opentelemetry-cpp SDK                        │
│                                                         │
│  TracerProvider → OTLP Span Exporter                    │
│  MeterProvider  → OTLP Metric Exporter                  │
│  LoggerProvider → OTLP Log Exporter                     │
│                                                         │
│  Transport: libcurl (HTTP/protobuf) — already a dep     │
└─────────────────────────────────────────────────────────┘
```

### Decision 2: OTLP over HTTP with libcurl, not gRPC

**Choice:** Use the OTLP HTTP/protobuf exporter (`OtlpHttpExporter`) with libcurl as the transport.

**Rationale:** libcurl is already a required dependency of BartonCore. Adding gRPC would bring in a large new dependency tree (abseil, protobuf, gRPC core) — potentially 50+ MB of build artifacts. The HTTP exporter in opentelemetry-cpp uses libcurl directly and supports the same OTLP wire protocol. Any collector (Jaeger, Grafana Alloy, OTel Collector) that accepts OTLP/gRPC also accepts OTLP/HTTP.

**Alternatives considered:**
- *gRPC transport*: Better streaming performance, but unacceptable dependency footprint for an embedded-oriented library.
- *In-process collector*: Too complex; exporting to an external collector is the standard pattern.

### Decision 3: Compile-time gating via `BCORE_OBSERVABILITY` flag

**Choice:** Use a new CMake boolean option `BCORE_OBSERVABILITY` (default OFF) → `BARTON_CONFIG_OBSERVABILITY`. When OFF, all observability headers expose no-op inline functions or macros, identical to the existing `TELEMETRY_COUNTER` no-op pattern. When ON, the adapter `.cpp` files are compiled and linked.

**Rationale:** Follows the established `bcore_option()` pattern used by all other optional features. Default OFF means no impact on existing builds, no new dependencies, and no runtime overhead unless explicitly opted in.

```
BCORE_OBSERVABILITY=OFF (default)        BCORE_OBSERVABILITY=ON
┌────────────────────────┐               ┌────────────────────────┐
│ observabilityTracing.h:        │               │ observabilityTracing.h:        │
│   static inline void   │               │   extern ObservabilitySpan*    │
│   observabilitySpanStart(...)  │               │   observabilitySpanStart(...)  │
│   { /* no-op */ }      │               │   → calls otel SDK     │
└────────────────────────┘               └────────────────────────┘
  No .cpp files compiled                   .cpp files compiled
  No otel-cpp dependency                   Links opentelemetry-cpp
```

### Decision 4: Span context propagation via explicit parameter passing

**Choice:** Pass a `ObservabilitySpanContext*` opaque pointer through function call chains to propagate trace context across thread/subsystem boundaries. Do not use thread-local storage (TLS) for implicit propagation.

**Rationale:** BartonCore's threading model makes TLS-based context propagation unreliable:
- Operations start on one thread, dispatch callbacks on the GLib main loop thread, and may fork work into the thread pool (5 workers).
- Matter operations transit between pthreads and the Matter event loop.
- The `icThreadPool` dispatches work items to arbitrary threads.

Explicit passing is more verbose but deterministic. The initial instrumentation can pass `NULL` for context (creates root spans) and selectively add parent context for the most important cross-thread flows (e.g., discovery → subsystem → driver callback chain).

**Alternatives considered:**
- *Thread-local automatic propagation*: Standard in Java/Go OpenTelemetry, but breaks with BartonCore's thread pool and event loop dispatch model.
- *GLib `GSource` user-data propagation*: Too tightly coupled to GLib internals; not all operations flow through GLib sources.

### Decision 5: icLog integration via linker `--wrap` shim

**Choice:** Intercept `icLogMsg` calls at link time using the `--wrap=icLogMsg` linker flag. A shim file (`observabilityLogBridgeShim.cpp`) implements `__wrap_icLogMsg`, which forwards the log message to the OpenTelemetry Logs SDK via `observabilityLogBridgeEmit()` and then calls through to `__real_icLogMsg` to preserve the original log output.

**Rationale:** This approach avoids modifying BartonCommon entirely. The `--wrap` linker flag is a standard GNU ld feature that transparently redirects calls to `icLogMsg` through the shim at link time. No changes to any existing log call sites or to the BartonCommon library are needed.

The log bridge preserves:
- `categoryName` → OTel log `InstrumentationScope`
- `priority` → OTel `Severity`
- `file`, `func`, `line` → OTel log attributes
- Message body → OTel `Body`
- Active span context (if any) → automatic trace-log correlation

**Alternatives considered:**
- *New BartonCommon logging backend*: Would require modifying the BartonCommon library, adding cross-repo coupling.
- *Wrapper macros around icLog*: Would require changing every log call site.
- *Post-processing stdout*: Fragile, loses structured metadata.

### Decision 6: opentelemetry-cpp dependency via CMake FetchContent

**Choice:** Fetch `opentelemetry-cpp` at CMake configure time via `FetchContent_Declare`/`FetchContent_MakeAvailable`, pinned to a specific release tag. Build only the required components: SDK, OTLP HTTP exporter, and logging SDK.

**Rationale:** Follows the existing pattern for BartonCommon (`FetchContent`). Pinning to a tag ensures reproducible builds. Building a subset avoids pulling in gRPC, Prometheus, or other unneeded exporters.

**Alternatives considered:**
- *System-installed package*: OpenTelemetry C++ is rarely packaged by distros; would complicate Docker and CI setup.
- *Git submodule*: BartonCore uses `FetchContent` elsewhere; consistency is valuable.

### Decision 7: Configuration via BartonCore's property system

**Choice:** Expose OTLP exporter configuration through runtime environment variables following the OpenTelemetry specification's environment variable conventions:
- `OTEL_EXPORTER_OTLP_ENDPOINT` (default: `http://localhost:4318`)
- `OTEL_SERVICE_NAME` (default: `barton-core`)

The OTLP/HTTP protocol is hardcoded (not configurable via `OTEL_EXPORTER_OTLP_PROTOCOL`).

**Rationale:** OpenTelemetry defines standard environment variables for configuration. Respecting these means BartonCore works out-of-the-box with any OTel Collector deployment. No new BartonCore-specific property keys needed.

### Decision 8: Reference app as end-to-end observability demo

**Choice:** The reference application (`reference/`) will serve as a complete, runnable demonstration of BartonCore's observability. When built with `BCORE_OBSERVABILITY=ON`, the reference app exercises the full pipeline. A Docker Compose overlay (`docker/compose.observability.yaml`) adds an OpenTelemetry Collector and Jaeger, so a developer can start the stack and immediately see traces, metrics, and logs in a browser.

**Rationale:** BartonCore is a library — it handles instrumentation and OTLP export, but deliberately stops short of running its own collector or visualizer. However, without a working example, adoption friction is high. The reference app already demonstrates the API; extending it to demonstrate observability follows the same pattern. The Docker overlay keeps the observability infrastructure opt-in and separate from the core dev environment.

**Architecture:**

```
┌──────────────────────────────┐
│  Reference App               │
│  (barton-core-reference)     │
│                              │
│  OTEL_EXPORTER_OTLP_ENDPOINT │
│  = http://otel-collector:4318│
└──────────────┬───────────────┘
               │ OTLP/HTTP
┌──────────────▼───────────────┐
│  OpenTelemetry Collector     │
│  (docker service)            │
│                              │
│  receivers:  otlp            │
│  exporters:  jaeger, logging │
└──────────────┬───────────────┘
               │
┌──────────────▼───────────────┐
│  Jaeger                      │
│  (docker service)            │
│                              │
│  UI: http://localhost:16686  │
│  Traces, spans, service map  │
└──────────────────────────────┘
```

**Alternatives considered:**
- *Grafana + Prometheus + Tempo stack*: More comprehensive but significantly heavier for a dev demo. Jaeger is a single binary that handles traces, and the OTel Collector can log metrics to stdout for demo purposes.
- *No demo infrastructure*: Leaves it to developers to figure out the full pipeline, increasing adoption friction.

### Decision 9: Incremental commits per logical task group

**Choice:** Each task group (build system, headers, tracing impl, metrics impl, etc.) SHALL be committed independently as it is completed, rather than accumulating all changes into a single feature commit.

**Rationale:** Independent commits provide:
- Reviewable, bisectable history — each commit compiles and passes tests.
- Easier rollback if a specific layer causes issues.
- Clearer attribution of changes to their purpose.
- The build system and no-op headers can land first, establishing the scaffolding before any implementation code arrives.

The task groups in `tasks.md` are already structured to produce sane commit boundaries.

## Risks / Trade-offs

**[Build time increase when enabled]** → opentelemetry-cpp adds compile time. Mitigated by: default OFF, FetchContent caching, and building only required components (no gRPC, no Prometheus exporter).

**[Binary size increase ~2–5 MB]** → Acceptable for development/debugging builds; production deployments that don't need observability leave the flag OFF.

**[Thread safety of otel-cpp SDK]** → The OpenTelemetry C++ SDK's `TracerProvider`, `MeterProvider`, and `LoggerProvider` are thread-safe by design. The C adapter layer needs no additional synchronization beyond what the SDK provides. Risk is low.

**[Context propagation gaps]** → Initial instrumentation will have incomplete trace propagation across thread pool boundaries. This is acceptable — spans without parents become root spans, and the tracing is still useful for per-operation latency. Full cross-thread propagation can be added incrementally.

**[BartonCommon log backend coupling]** → Resolved by using the `--wrap=icLogMsg` linker flag to intercept log calls at link time, avoiding any modification to BartonCommon.

**[protobuf version conflicts]** → The Matter SDK bundles its own protobuf. opentelemetry-cpp's OTLP exporter also needs protobuf. Linking both may cause ODR violations. Mitigated by: using opentelemetry-cpp's "with abseil" build option that shares protobuf, or using the HTTP/JSON exporter variant that avoids protobuf entirely.

## Resolved Questions

1. **BartonCommon modification scope**: Resolved — the icLog OTel bridge lives in BartonCore using the `--wrap=icLogMsg` linker flag. No BartonCommon modifications are needed.

2. **Protobuf coexistence with Matter SDK**: Resolved — the OTLP HTTP exporter uses libcurl and HTTP/protobuf, which does not conflict with the Matter SDK's bundled protobuf.

3. **Initial instrumentation scope**: Resolved — implemented spans for: device discovery, device found, resource read/write, and subsystem init/shutdown. Commissioning spans deferred to a future iteration.

4. **Metric cardinality**: Not yet resolved — current metrics do not include per-device dimensions. The `device.active.count` gauge tracks the total count.
