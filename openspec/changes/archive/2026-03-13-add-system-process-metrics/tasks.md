## 1. Collector Configuration

- [x] 1.1 Add `hostmetrics` receiver to `docker/otel-collector-config.yaml` with `cpu`, `memory`, and `process` scrapers (15s interval, process filter for `barton-core-reference`)
- [x] 1.2 Add `hostmetrics` to the `metrics` pipeline receivers alongside `otlp`
- [x] 1.3 Add `spanmetrics` connector to derive RED metrics from traces
- [x] 1.4 Add `prometheus` exporter on port 8889

## 2. Docker Compose PID Sharing

- [x] 2.1 Add `pid: "service:barton"` to the `otel-collector` service in `docker/compose.observability.yaml`

## 3. Prometheus & Jaeger SPM

- [x] 3.1 Create `docker/prometheus.yaml` scrape config targeting `otel-collector:8889` (15s interval)
- [x] 3.2 Add Prometheus container (`prom/prometheus:v2.53.0`) to `docker/compose.observability.yaml`
- [x] 3.3 Configure Jaeger SPM env vars (`METRICS_STORAGE_TYPE`, `PROMETHEUS_SERVER_URL`, `PROMETHEUS_QUERY_SUPPORT_SPANMETRICS_CONNECTOR`, normalize flags)
- [x] 3.4 Remove all hardcoded `ports:` from `compose.observability.yaml` for shared-machine safety

## 4. Devcontainer & CLI

- [x] 4.1 Update `devcontainer.json` with socat proxies and `forwardPorts` for Jaeger (16686) and Prometheus (9090)
- [x] 4.2 Update `dockerw` with ephemeral port override logic for Prometheus alongside Jaeger

## 5. Documentation

- [x] 5.1 Create `docs/OBSERVABILITY_METRICS.md` with all metric tables and Prometheus query reference
- [x] 5.2 Update `docs/OBSERVABILITY.md` with Prometheus/Jaeger SPM docs, trim inline metrics, add cross-references

## 6. Verification

- [x] 6.1 Verify `system.cpu.time` and `system.memory.usage` metrics appear in Prometheus
- [x] 6.2 Verify `process.cpu.time`, `process.memory.physical_usage`, `process.threads`, and `process.open_file_descriptors` metrics appear for `barton-core-reference`
- [x] 6.3 Verify Jaeger Monitor tab shows span-derived RED metrics via Prometheus
- [x] 6.4 Verify all 33 application metrics are queryable in Prometheus
