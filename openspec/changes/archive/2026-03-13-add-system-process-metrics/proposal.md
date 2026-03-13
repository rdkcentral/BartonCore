## Why

BartonCore's observability stack currently exports 33 application-level OTel meters (device lifecycle, commissioning, Zigbee, subsystem events) but provides zero visibility into the process or host system health. Without system/process metrics, operators cannot correlate application behavior with resource consumption (memory leaks, CPU spikes, file descriptor exhaustion) — information critical for diagnosing production issues on resource-constrained IoT hub devices.

## What Changes

- **Enable the OTel Collector `hostmetrics` receiver** to scrape system-level (CPU, memory) and process-level (barton-core CPU, memory, threads, FDs) metrics — following OTel semantic conventions out of the box.
- **Configure Docker Compose service PID sharing** (`pid: "service:barton"`) so the collector container can see barton-core's `/proc` entries without mounting the host's `/proc` filesystem.
- **Add a Prometheus backend** (`docker/prometheus.yaml`) scraping the collector's metrics endpoint, enabling persistent metric storage and querying.
- **Add a `spanmetrics` connector** in the collector to derive RED (Rate, Errors, Duration) metrics from traces automatically.
- **Configure Jaeger Monitor (SPM) tab** with Prometheus as the metrics backend, enabling service performance monitoring directly in the Jaeger UI.
- **Add a Prometheus exporter** on the collector (port 8889) so Prometheus can scrape all metrics (application, system/process, and span-derived).
- **Remove all hardcoded ports** from `compose.observability.yaml` for shared-machine safety; ports are assigned ephemerally by `dockerw` or forwarded via socat in the devcontainer.
- **Update `dockerw`** with ephemeral port override logic for Prometheus alongside Jaeger.
- **Update `devcontainer.json`** with socat proxies and port forwarding for both Jaeger (16686) and Prometheus (9090).
- **Create `OBSERVABILITY_METRICS.md`** as the authoritative metrics reference with all metric tables and Prometheus query examples.
- **Update `OBSERVABILITY.md`** with Prometheus/Jaeger SPM documentation, trimming inline metric tables in favor of cross-references to the new metrics doc.

## Non-goals

- Production-hardened alerting rules or dashboards — this is a proof-of-concept.
- Monitoring other containers or host-level aggregation beyond the barton service.
- Application-side process metrics (GLib internals) — deferred; `/proc`-based collection covers standard metrics.
- Adding tracing or log pipeline changes (log export remains `debug`-only).

## Capabilities

### New Capabilities
- `otel-system-process-metrics`: Collector-side `hostmetrics` receiver configuration for system and process metric scraping, Docker Compose PID sharing setup.

### Modified Capabilities
- `otel-reference-demo`: The observability Docker Compose overlay and collector config are extended with the `hostmetrics` receiver, PID namespace sharing, Prometheus backend, `spanmetrics` connector, Jaeger SPM configuration, zero-port compose, devcontainer socat proxies, and `OBSERVABILITY_METRICS.md`.

## Impact

- **Docker layer** (`docker/`): `compose.observability.yaml` gains `pid: "service:barton"`, Prometheus container, Jaeger SPM env vars, zero hardcoded ports. `otel-collector-config.yaml` gains `hostmetrics` receiver, `spanmetrics` connector, `prometheus` exporter, updated pipelines. New `prometheus.yaml` scrape config.
- **Devcontainer** (`.devcontainer/devcontainer.json`): Object-form `postStartCommand` with foreground socat proxies for Jaeger and Prometheus, `forwardPorts` for 16686 and 9090.
- **CLI wrapper** (`dockerw`): Ephemeral port override logic for Jaeger and Prometheus, cleanup trap, URL logging.
- **Documentation** (`docs/`): New `OBSERVABILITY_METRICS.md` with all metric tables and Prometheus query reference. `OBSERVABILITY.md` trimmed with cross-references.
- **CMake flags**: `BCORE_OBSERVABILITY` remains the relevant flag — no new flags needed.
- **Dependencies**: No new library dependencies; `hostmetrics` and `spanmetrics` are built into the `otel/opentelemetry-collector-contrib` image already in use. Prometheus (`prom/prometheus:v2.53.0`) is added as a new container.
