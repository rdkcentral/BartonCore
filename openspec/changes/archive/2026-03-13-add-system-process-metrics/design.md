## Context

BartonCore runs as a long-lived C/C++ service inside a Docker container, managed by a GLib main loop. The existing observability stack (33 application-level OTel meters, traces, logs) exports via OTLP/HTTP to an OTel Collector (`otel/opentelemetry-collector-contrib:0.104.0`), which forwards to Jaeger.

The collector contrib image ships with a mature `hostmetrics` receiver that can scrape CPU, memory, disk, network, filesystem, and per-process metrics directly from `/proc` — following the OTel semantic conventions. However, the collector runs in its own container and cannot see barton-core's `/proc` entries by default.

```
Current deployment (compose.observability.yaml):

┌─────────────────────┐    ┌─────────────────────┐    ┌──────────┐
│ barton (container)   │    │ otel-collector       │    │ jaeger   │
│ PID namespace: own   │───▶│ PID namespace: own   │───▶│          │
│                      │4318│                      │4317│          │
│ barton-core process  │    │ /proc = collector's   │    │          │
│ /proc = barton's     │    │ own processes only    │    │          │
└─────────────────────┘    └─────────────────────┘    └──────────┘
         ⚠ Collector cannot see barton-core in /proc
```

## Goals / Non-Goals

**Goals:**
- Enable the collector to scrape system-level and barton-core process-level metrics
- Use Docker service PID sharing so the collector sees barton's `/proc` entries
- Keep changes minimal and infrastructure-only where possible
- Document the new metrics for operators

**Non-Goals:**
- Production alerting rules, dashboards, or Grafana configuration
- Monitoring containers other than barton-core
- Application-side process metric instrumentation (deferred — the collector covers all standard metrics via `/proc`, and no GLib-internal stats are accessible via public API without significant effort)
- Changing the existing 33 application-level meters
- Log export to a persistent backend (remains `debug`-only)

## Decisions

### Decision 1: PID sharing via `pid: "service:barton"` (over `/proc` host mount)

The collector container will share barton's PID namespace using Docker Compose's `pid: "service:barton"` directive.

```
With PID sharing:

┌───────────────────────────────────────────────────┐
│              Shared PID namespace                  │
│                                                   │
│  ┌─────────────────┐    ┌─────────────────────┐  │
│  │ barton           │    │ otel-collector       │  │
│  │ PID 1: entryp..  │    │ PID moves into       │  │
│  │ PID X: barton-c. │    │ barton's namespace   │  │
│  │                  │    │                      │  │
│  │  /proc/X/stat  ──────▶│ hostmetrics receiver │  │
│  │  /proc/X/statm ──────▶│ can read barton's    │  │
│  │  /proc/X/fd/   ──────▶│ /proc entries        │  │
│  └─────────────────┘    └─────────────────────┘  │
│                                                   │
└───────────────────────────────────────────────────┘
```

**Why over `/proc` host mount:**
- Simpler configuration — one line in compose vs. volume mount + `root_path` config
- More secure — collector only sees barton's processes, not every host process
- Standard Docker Compose feature, well-documented
- No need to account for host vs. container `/proc` path mapping

**Trade-off:** The collector's own process metrics will appear in barton's PID namespace. This is acceptable for a PoC — a `filter` processor can exclude collector PIDs if needed later.

### Decision 2: Collector `hostmetrics` receiver with process filtering

The `hostmetrics` receiver will be configured with the following scrapers:
- **`cpu`**: System-wide CPU utilization
- **`memory`**: System-wide memory usage
- **`process`**: Per-process metrics filtered to `barton-core-reference` (the reference app binary name)

The `process` scraper will use `include.names` with `regexp` matching to target the barton-core process specifically.

**Why not disk/filesystem/network scrapers:** These add volume without clear PoC value. They can be enabled trivially later by adding lines to the config.

### Decision 3: No application-side process metrics in this change

During exploration, we considered adding observable gauges for GLib main loop internals (event loop depth, pending sources, etc.). However:
- GLib's `GMainContext` does not expose pending source counts or loop iteration stats via public API
- Reading `/proc/self/*` from C would duplicate what the collector already provides
- The PoC goal is better served by demonstrating collector-side collection

If app-specific runtime metrics are identified later (e.g., internal queue depths, watchdog timers), they can be added as a follow-up change using the existing metrics API.

### Decision 4: Pipeline wiring in collector config

The `hostmetrics` receiver will be added to the existing `metrics` pipeline alongside the `otlp` receiver and the new `spanmetrics` connector. All metrics flow through the same batch processor and out to both the Prometheus exporter and the debug exporter.

```yaml
connectors:
  spanmetrics:

service:
  pipelines:
    traces:
      receivers: [otlp]
      processors: [batch]
      exporters: [otlp/jaeger, spanmetrics, debug]
    metrics:
      receivers: [otlp, hostmetrics, spanmetrics]
      processors: [batch]
      exporters: [prometheus, debug]
```

**Why not a separate pipeline:** Single pipeline is simpler, and the metrics are naturally co-located.

### Decision 5: Prometheus backend with Jaeger SPM

A Prometheus instance (`prom/prometheus:v2.53.0`) scrapes the collector's Prometheus exporter endpoint (`otel-collector:8889`) every 15 seconds. Jaeger's Monitor (SPM) tab queries Prometheus for span-derived RED metrics.

Jaeger environment configuration:
- `METRICS_STORAGE_TYPE=prometheus`
- `PROMETHEUS_SERVER_URL=http://prometheus:9090`
- `PROMETHEUS_QUERY_SUPPORT_SPANMETRICS_CONNECTOR=true` (required for connector metric names)
- `PROMETHEUS_QUERY_NORMALIZE_CALLS=true`
- `PROMETHEUS_QUERY_NORMALIZE_DURATION=true`

**Why Prometheus:** It provides a standard query interface for metrics, enables the Jaeger Monitor tab, and allows ad-hoc PromQL queries for debugging. No additional visualization layer (Grafana) needed for the PoC.

### Decision 6: `spanmetrics` connector for RED metrics

The OTel Collector's `spanmetrics` connector derives `calls_total` and `duration_milliseconds` metrics from traces automatically. These flow into the metrics pipeline and are scraped by Prometheus.

**Why a connector over manual instrumentation:** RED metrics are universal and don't require per-operation counter/histogram code. The connector produces them from every span automatically.

### Decision 7: Zero hardcoded ports for shared-machine safety

All `ports:` directives are removed from `compose.observability.yaml`. Ports are assigned:
- **In devcontainer:** via socat proxies (foreground, object-form `postStartCommand`) and `forwardPorts`
- **Via dockerw:** ephemeral port overrides with cleanup trap and URL logging

**Why:** Shared development machines have port conflicts. Ephemeral assignment eliminates collisions.

### Decision 8: Dedicated OBSERVABILITY_METRICS.md

All metric tables and Prometheus query reference material is moved to a new `docs/OBSERVABILITY_METRICS.md`. `OBSERVABILITY.md` is trimmed to cross-reference the metrics doc.

**Why a separate file:** `OBSERVABILITY.md` was growing large with architecture, build, config, traces, logs, and metrics all in one file. Separating metrics keeps both docs focused.

## Risks / Trade-offs

**[PID namespace sharing affects container isolation]** → Acceptable for dev/demo environment; document that this is not a production security posture. The shared namespace is opt-in via the observability overlay and not present in the base `compose.yaml`.

**[Process name matching is brittle]** → If the binary name changes, the filter breaks silently (produces no metrics, no errors). Mitigation: document the expected binary name; the `regexp` matcher provides some flexibility.

**[Collector sees its own process in shared namespace]** → The `process` scraper filter narrows to `barton-core-reference` only, so collector processes are excluded by name. No additional filtering needed.

**[Collection interval tuning]** → Default `hostmetrics` collection interval (60s) may be too coarse for a demo. We'll set it to 15s to match the PoC cadence. This is a single config value and trivially adjustable.

## Open Questions

1. ~~**Should we add a Prometheus exporter to the collector** for easier metric browsing?~~ **Resolved: YES.** A Prometheus exporter (port 8889) and Prometheus backend were added. Jaeger SPM queries Prometheus for span-derived metrics.
2. ~~**Should the `network` scraper be included** for TCP connection counts per process?~~ **Deferred.** Low effort to add later by adding a line to the config.
