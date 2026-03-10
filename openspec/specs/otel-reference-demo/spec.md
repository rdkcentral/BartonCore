## Requirements

### Requirement: Reference app observability enablement
When built with `BCORE_OBSERVABILITY=ON`, the reference application SHALL demonstrate BartonCore's observability capabilities as a working example for client developers.

#### Scenario: Build with observability
- **WHEN** the reference app is built with `BCORE_OBSERVABILITY=ON`
- **THEN** the resulting `barton-core-reference` binary SHALL initialize the observability layer and export traces, metrics, and logs via OTLP

#### Scenario: Build without observability
- **WHEN** the reference app is built with `BCORE_OBSERVABILITY=OFF`
- **THEN** the reference app SHALL behave identically to its current behavior with no observability overhead

### Requirement: OTLP endpoint configuration
The reference app SHALL respect the standard OpenTelemetry environment variables for configuring the export destination, requiring no code changes from the developer.

#### Scenario: Default endpoint
- **WHEN** the reference app starts with `BCORE_OBSERVABILITY=ON` and no `OTEL_EXPORTER_OTLP_ENDPOINT` is set
- **THEN** observability data SHALL be exported to `http://localhost:4318`

#### Scenario: Custom endpoint via environment
- **WHEN** `OTEL_EXPORTER_OTLP_ENDPOINT=http://otel-collector:4318` is set
- **THEN** the reference app SHALL export all observability data to that endpoint

### Requirement: Docker Compose observability overlay
A Docker Compose overlay file SHALL provide a complete local observability stack (OTel Collector + visualizer) that works alongside the existing development Compose files.

#### Scenario: Start observability stack
- **WHEN** a developer runs `docker compose -f docker/compose.yaml -f docker/compose.observability.yaml up`
- **THEN** an OpenTelemetry Collector and a Jaeger instance SHALL start alongside the existing services

#### Scenario: Collector receives observability data
- **WHEN** the reference app is running with `OTEL_EXPORTER_OTLP_ENDPOINT` pointing to the collector
- **THEN** the collector SHALL receive and forward traces, metrics, and logs to the visualizer

#### Scenario: Visualize traces in browser
- **WHEN** the observability stack is running and the reference app has performed device operations
- **THEN** a developer SHALL be able to open the Jaeger UI (e.g., `http://localhost:16686`) and see traces for BartonCore operations (discovery, commissioning, resource reads/writes)

#### Scenario: Overlay is optional
- **WHEN** a developer runs `docker compose -f docker/compose.yaml up` without the observability overlay
- **THEN** no collector or visualizer services SHALL be started

### Requirement: OTel Collector configuration
The observability overlay SHALL include an OTel Collector configuration file that receives OTLP data and exports it to the local visualizer.

#### Scenario: Collector config file
- **WHEN** the observability overlay is used
- **THEN** an OTel Collector config (`docker/otel-collector-config.yaml`) SHALL define an OTLP receiver on port 4318 (HTTP) and export to Jaeger

#### Scenario: Collector receives all signals
- **WHEN** the reference app emits traces, metrics, and logs
- **THEN** the collector SHALL accept all three signal types via its OTLP receiver

### Requirement: Observability demo documentation
Documentation SHALL guide a developer through running the reference app with observability end-to-end.

#### Scenario: Getting started guide
- **WHEN** a developer reads the observability documentation
- **THEN** they SHALL find step-by-step instructions for: building with `BCORE_OBSERVABILITY=ON`, starting the observability Docker stack, running the reference app, and viewing traces in the Jaeger UI
