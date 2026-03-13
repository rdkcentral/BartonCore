## Requirements

### Requirement: System CPU metrics via collector
The OTel Collector `hostmetrics` receiver SHALL scrape system-wide CPU utilization metrics from the shared PID namespace.

#### Scenario: System CPU utilization is reported
- **WHEN** the observability stack is running with the `hostmetrics` receiver enabled
- **THEN** the collector SHALL emit `system.cpu.time` metrics broken down by CPU state (user, system, idle, etc.) following OTel semantic conventions

### Requirement: System memory metrics via collector
The OTel Collector `hostmetrics` receiver SHALL scrape system-wide memory usage metrics.

#### Scenario: System memory usage is reported
- **WHEN** the observability stack is running with the `hostmetrics` receiver enabled
- **THEN** the collector SHALL emit `system.memory.usage` metrics broken down by memory state (used, free, buffered, cached, etc.) following OTel semantic conventions

### Requirement: Process CPU metrics for barton-core
The OTel Collector `hostmetrics` process scraper SHALL report CPU usage for the barton-core process.

#### Scenario: Process CPU time is reported
- **WHEN** the barton-core reference app is running and the `hostmetrics` process scraper is enabled
- **THEN** the collector SHALL emit `process.cpu.time` for the barton-core process with attributes `process.executable.name` and `process.pid`

#### Scenario: Only barton-core process is scraped
- **WHEN** the process scraper is configured with `include.names` matching `barton-core-reference`
- **THEN** metrics SHALL only be emitted for processes matching that name, not for the collector or other container processes

### Requirement: Process memory metrics for barton-core
The OTel Collector `hostmetrics` process scraper SHALL report memory usage for the barton-core process.

#### Scenario: Process memory usage is reported
- **WHEN** the barton-core reference app is running and the `hostmetrics` process scraper is enabled
- **THEN** the collector SHALL emit `process.memory.physical_usage` (RSS) for the barton-core process

### Requirement: Process thread and file descriptor metrics for barton-core
The OTel Collector `hostmetrics` process scraper SHALL report thread count and open file descriptor count for the barton-core process.

#### Scenario: Thread count is reported
- **WHEN** the barton-core reference app is running and the `hostmetrics` process scraper is enabled
- **THEN** the collector SHALL emit `process.threads` for the barton-core process

#### Scenario: Open file descriptor count is reported
- **WHEN** the barton-core reference app is running and the `hostmetrics` process scraper is enabled
- **THEN** the collector SHALL emit `process.open_file_descriptors` for the barton-core process

### Requirement: Hostmetrics collection interval
The `hostmetrics` receiver SHALL use a configurable collection interval appropriate for development and demonstration.

#### Scenario: Collection interval is 15 seconds
- **WHEN** the default collector configuration is used
- **THEN** the `hostmetrics` receiver SHALL scrape metrics every 15 seconds

### Requirement: PID namespace sharing for process visibility
The OTel Collector container SHALL share the barton container's PID namespace so it can read barton-core's `/proc` entries.

#### Scenario: Collector shares barton PID namespace
- **WHEN** the observability Docker Compose overlay is used
- **THEN** the `otel-collector` service SHALL be configured with `pid: "service:barton"` to share barton's PID namespace

#### Scenario: Base compose is unaffected
- **WHEN** the base `docker/compose.yaml` is used without the observability overlay
- **THEN** no PID namespace sharing SHALL be configured and no system/process metrics SHALL be collected

### Requirement: Hostmetrics pipeline integration
System and process metrics from the `hostmetrics` receiver SHALL flow through the existing metrics pipeline alongside application metrics.

#### Scenario: Hostmetrics in metrics pipeline
- **WHEN** the collector configuration includes the `hostmetrics` receiver
- **THEN** the `metrics` pipeline SHALL include both `otlp` and `hostmetrics` receivers, using the same `batch` processor and exporters

### Requirement: Observability documentation for system/process metrics
The observability documentation SHALL describe the system and process metrics available when running the observability stack.

#### Scenario: Documentation covers system/process metrics
- **WHEN** a developer reads `docs/OBSERVABILITY_METRICS.md`
- **THEN** they SHALL find a section describing the system-level metrics (CPU, memory) and process-level metrics (CPU, memory, threads, FDs) collected by the `hostmetrics` receiver

#### Scenario: Documentation explains PID sharing
- **WHEN** a developer reads `docs/OBSERVABILITY.md`
- **THEN** they SHALL find an explanation of why PID namespace sharing is needed and how it is configured
