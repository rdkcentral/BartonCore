## MODIFIED Requirements

### Requirement: Docker Compose observability overlay
A Docker Compose overlay file SHALL provide a complete local observability stack (OTel Collector + Jaeger + Prometheus) that works alongside the existing development Compose files.

#### Scenario: Start observability stack
- **WHEN** a developer runs `docker compose -f docker/compose.yaml -f docker/compose.observability.yaml up`
- **THEN** an OpenTelemetry Collector, a Jaeger instance, and a Prometheus instance SHALL start alongside the existing services

#### Scenario: Collector receives observability data
- **WHEN** the reference app is running with `OTEL_EXPORTER_OTLP_ENDPOINT` pointing to the collector
- **THEN** the collector SHALL receive and forward traces, metrics, and logs

#### Scenario: Collector shares barton PID namespace
- **WHEN** the observability overlay is active
- **THEN** the `otel-collector` service SHALL be configured with `pid: "service:barton"` so the `hostmetrics` receiver can see barton-core's `/proc` entries

#### Scenario: Zero hardcoded ports
- **WHEN** the observability overlay compose file is used
- **THEN** no service SHALL have hardcoded `ports:` directives; port assignment is handled by `dockerw` (ephemeral) or devcontainer socat proxies

#### Scenario: Visualize traces in browser
- **WHEN** the observability stack is running and the reference app has performed device operations
- **THEN** a developer SHALL be able to open the Jaeger UI and see traces for BartonCore operations

#### Scenario: Overlay is optional
- **WHEN** a developer runs `docker compose -f docker/compose.yaml up` without the observability overlay
- **THEN** no collector, visualizer, or Prometheus services SHALL be started and no PID namespace sharing SHALL be configured

### Requirement: OTel Collector configuration
The observability overlay SHALL include an OTel Collector configuration file that receives OTLP data and system/process metrics, derives span RED metrics, and exports them to Prometheus and Jaeger.

#### Scenario: Collector config file
- **WHEN** the observability overlay is used
- **THEN** an OTel Collector config (`docker/otel-collector-config.yaml`) SHALL define an OTLP receiver on port 4318 (HTTP), a `hostmetrics` receiver with `cpu`, `memory`, and `process` scrapers, a `spanmetrics` connector, a `prometheus` exporter on port 8889, and an OTLP exporter to Jaeger

#### Scenario: Collector receives all signals
- **WHEN** the reference app emits traces, metrics, and logs
- **THEN** the collector SHALL accept all three signal types via its OTLP receiver and system/process metrics via its `hostmetrics` receiver

#### Scenario: Spanmetrics connector derives RED metrics
- **WHEN** traces flow through the collector
- **THEN** the `spanmetrics` connector SHALL produce `calls_total` and `duration_milliseconds` metrics automatically

### Requirement: Prometheus backend
The observability overlay SHALL include a Prometheus instance that scrapes the collector's metrics endpoint.

#### Scenario: Prometheus scrapes collector
- **WHEN** the observability stack is running
- **THEN** Prometheus SHALL scrape `otel-collector:8889` every 15 seconds

#### Scenario: Prometheus stores all metric types
- **WHEN** the collector emits application, system/process, and span-derived metrics
- **THEN** all metric types SHALL be queryable via PromQL in Prometheus

### Requirement: Jaeger Monitor (SPM) tab
The Jaeger instance SHALL be configured to use Prometheus as a metrics backend for the Monitor tab.

#### Scenario: Jaeger SPM queries Prometheus
- **WHEN** a developer opens the Jaeger Monitor tab
- **THEN** Jaeger SHALL query Prometheus for span-derived RED metrics using the spanmetrics connector metric names

#### Scenario: Jaeger SPM environment configuration
- **WHEN** the observability overlay is active
- **THEN** Jaeger SHALL be configured with `METRICS_STORAGE_TYPE=prometheus`, `PROMETHEUS_SERVER_URL`, and `PROMETHEUS_QUERY_SUPPORT_SPANMETRICS_CONNECTOR=true`

### Requirement: Observability documentation
The observability documentation SHALL describe the full metrics stack including Prometheus, Jaeger SPM, and system/process metrics.

#### Scenario: Dedicated metrics reference
- **WHEN** a developer seeks metrics information
- **THEN** they SHALL find `docs/OBSERVABILITY_METRICS.md` containing all metric tables and Prometheus query reference examples

#### Scenario: Architecture documentation
- **WHEN** a developer reads `docs/OBSERVABILITY.md`
- **THEN** they SHALL find Prometheus and Jaeger Monitor tab documentation with cross-references to the metrics doc

#### Scenario: Documentation explains PID sharing
- **WHEN** a developer reads `docs/OBSERVABILITY.md`
- **THEN** they SHALL find an explanation of why PID namespace sharing is needed and how it is configured
