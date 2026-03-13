# Observability Metrics Reference

This document is the authoritative reference for all metrics emitted by
BartonCore. It covers application metrics produced by the OTel SDK,
span-derived RED metrics from the collector's `spanmetrics` connector,
and system/process metrics collected by the `hostmetrics` receiver.

For build instructions, collector setup, and general observability
architecture see [OBSERVABILITY.md](OBSERVABILITY.md).

---

## Application Metrics

All application metrics are created via the C adapter layer in
`observabilityMetrics.h`. When `BARTON_CONFIG_OBSERVABILITY` is undefined
every call compiles to a zero-cost no-op.

### Device Lifecycle

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `device.active.count` | Gauge | {device} | — | Number of currently paired devices |
| `device.discovery.started` | Counter | {request} | — | Discovery requests started |
| `device.discovery.completed` | Counter | {request} | — | Discovery requests completed |
| `device.discovered.count` | Counter | {device} | `device.class` | Devices discovered |
| `device.add.success` | Counter | {device} | `device.class` | Devices successfully added |
| `device.add.failed` | Counter | {device} | `device.class` | Device add failures |
| `device.remove.success` | Counter | {device} | `device.class` | Devices successfully removed |
| `device.rejected.count` | Counter | {device} | `device.class` | Devices rejected by discovery filters |
| `device.discovery.duration` | Histogram | s | `device.class` | Discovery duration |

### Communication Watchdog

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `device.commfail.count` | Counter | {event} | — | Communication failure events |
| `device.commrestore.count` | Counter | {event} | — | Communication restored events |
| `device.commfail.current` | Gauge | {device} | — | Devices currently in comm-fail |
| `device.communication.check.performed` | Counter | {cycle} | — | Communication check cycles performed |

### Storage & Restore

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `storage.device.persist` | Counter | {operation} | — | Device persistence operations |
| `storage.device.count` | Gauge | {device} | — | Number of devices in storage |
| `storage.restore.attempt` | Counter | {attempt} | — | Storage restore attempts |
| `storage.restore.success` | Counter | {attempt} | — | Storage restore successes |
| `storage.restore.failed` | Counter | {attempt} | — | Storage restore failures |

### Driver Manager

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `driver.registered.count` | Counter | {driver} | `driver.name` | Device drivers registered |
| `driver.init.success` | Counter | {driver} | `driver.name` | Device driver startups |

### Events

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `event.produced` | Counter | {event} | `event.type` | Events produced by device service |

### Subsystem Manager

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `subsystem.init.started` | Counter | {event} | `subsystem.name` | Subsystem initialization started |
| `subsystem.init.completed` | Counter | {event} | `subsystem.name` | Subsystem initialization completed |
| `subsystem.init.failed` | Counter | {event} | — | Subsystem initialization failed |
| `subsystem.initialized.count` | Gauge | {subsystem} | — | Number of initialized subsystems |
| `subsystem.ready_for_devices` | Gauge | {state} | — | All subsystems ready for devices |
| `subsystem.init.duration` | Histogram | s | `subsystem.name` | Subsystem initialization duration |

### Matter Subsystem

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `matter.initializing` | Gauge | 1 | — | Whether Matter is initializing |
| `matter.commissioning.attempt` | Counter | {attempt} | — | Commissioning attempts |
| `matter.commissioning.success` | Counter | {attempt} | — | Commissioning successes |
| `matter.commissioning.failed` | Counter | {attempt} | — | Commissioning failures |
| `matter.commissioning.duration` | Histogram | s | — | Commissioning duration |
| `matter.pairing.attempt` | Counter | {attempt} | — | Pairing attempts |
| `matter.pairing.success` | Counter | {attempt} | — | Pairing successes |
| `matter.pairing.failed` | Counter | {attempt} | — | Pairing failures |
| `matter.ota.query_image.received` | Counter | {request} | — | OTA query image requests received |
| `matter.ota.query_image.available` | Counter | {request} | — | OTA images available |
| `matter.ota.query_image.not_available` | Counter | {request} | — | OTA images not available |
| `matter.ota.apply_update.received` | Counter | {request} | — | OTA apply update requests received |

### Zigbee Subsystem

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `zigbee.network.startup.completed` | Counter | {event} | — | Zigbee network startup completions |
| `zigbee.device.join.count` | Counter | {event} | — | Zigbee device join events |
| `zigbee.device.announce.count` | Counter | {event} | — | Zigbee device announce events |
| `zigbee.device.processed.count` | Counter | {device} | — | Zigbee devices processed |
| `zigbee.interference.detected` | Counter | {event} | — | Zigbee interference detected events |
| `zigbee.interference.resolved` | Counter | {event} | — | Zigbee interference resolved events |
| `zigbee.interference.active` | Gauge | {state} | — | Zigbee interference currently active |
| `zigbee.health_check.enabled` | Gauge | {state} | — | Zigbee health check enabled |
| `zigbee.pan_id_attack.detected` | Counter | {event} | — | Zigbee PAN ID attack detected |
| `zigbee.pan_id_attack.active` | Gauge | {state} | — | Zigbee PAN ID attack currently active |

### Device Telemetry

Per-device resource metrics with rich metadata attributes. The metadata
cache populates on the first resource update after device-add completes
(device class and manufacturer must both be non-empty). Metrics are emitted
from the central `updateResource()` convergence point in `deviceService.c`
so all subsystems (Zigbee, Matter, etc.) are covered automatically.

#### Resource Metrics

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `device.resource.value` | Gauge | {value} | `device.id`, `device.class`, `endpoint.profile`, `resource.name`, `device.manufacturer`, `device.model`, `device.hardware_version`, `device.firmware_version`, `driver.type` | Current numeric value of a device resource (integer, percentage, lightLevel, temperature, seconds, minutes, milliWatts, watts, battery.voltage, humidity.relative, signalStrength, motionSensitivity, illuminance, magneticFieldStrength) |
| `device.resource.state_change` | Counter | {transition} | `device.id`, `device.class`, `endpoint.profile`, `resource.name`, `device.manufacturer`, `device.model`, `device.hardware_version`, `device.firmware_version`, `driver.type`, `state` | Boolean state transitions on a device resource |
| `device.resource.update` | Counter | {update} | `device.id`, `device.class`, `endpoint.profile`, `resource.name`, `device.manufacturer`, `device.model`, `device.hardware_version`, `device.firmware_version`, `driver.type` | Resource value changes reported by devices (all resource types) |

#### Zigbee Link Quality

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `zigbee.device.rssi` | Gauge | dBm | `device.id` | Zigbee device received signal strength indicator |
| `zigbee.device.lqi` | Gauge | {lqi} | `device.id` | Zigbee device link quality indicator |

#### Matter Subscription Lifecycle

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `matter.subscription.established` | Counter | {event} | `device.id` | Matter subscription (re)establishment events |
| `matter.subscription.report` | Counter | {report} | `device.id` | Matter subscription report cycles received |
| `matter.subscription.error` | Counter | {error} | `device.id`, `error` | Matter subscription errors |

#### CASE Session

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `matter.case.session.duration` | Histogram | s | `device.id` | Time from `GetConnectedDevice` to `OnDeviceConnected` |
| `matter.case.session.error` | Counter | {error} | `device.id`, `error` | Matter CASE session connection failures |

---

## Span-Derived RED Metrics

The OTel Collector's `spanmetrics` connector automatically derives Rate,
Errors, and Duration (RED) metrics from every traced span. No application
code is required — any traced operation produces these metrics.

The following spans are instrumented and therefore produce RED metrics
(see [OBSERVABILITY.md](OBSERVABILITY.md) for the full span hierarchy and
attribute details):

| Span Name | Subsystem | Description |
|---|---|---|
| `subsystem.init` | Core | Subsystem initialization |
| `subsystem.shutdown` | Core | Subsystem shutdown |
| `device.discovery` | Core | Device discovery session |
| `device.found` | Core | Device found during discovery |
| `device.configure` | Core | Driver configure callback |
| `device.persist` | Core | Database persistence |
| `device.commission` | Core | Device commissioning operation |
| `device.pair` | Core | Device pairing operation |
| `device.remove` | Core | Device removal operation |
| `device.driver.remove` | Core | Driver removal callback |
| `device.commission_window.open` | Core | Commissioning window open |
| `resource.read` | Core | Resource read operation |
| `resource.write` | Core | Resource write operation |
| `resource.update` | Matter / Zigbee | Resource update from driver |
| `matter.case.connect` | Matter | CASE session establishment |
| `matter.subscribe` | Matter | Subscription request |
| `matter.report` | Matter | Subscription report cycle |
| `matter.fabric.remove` | Matter | Fabric removal on device remove |
| `zigbee.device.discovered` | Zigbee | Zigbee device discovered callback |
| `zigbee.cluster.read` | Zigbee | Zigbee cluster read operation |
| `zigbee.cluster.write` | Zigbee | Zigbee cluster write operation |

| Metric Name | Type | Labels | Description |
|---|---|---|---|
| `calls` | Counter | `service_name`, `span_name`, `span_kind`, `status_code` | Total span invocations |
| `duration` | Histogram (ms) | `service_name`, `span_name`, `span_kind`, `status_code` | Span duration |

> **Note:** The Prometheus exporter appends `_total` to counters and
> `_milliseconds_bucket` / `_milliseconds_sum` / `_milliseconds_count` to
> histograms, so the actual Prometheus metric names are `calls_total`,
> `duration_milliseconds_bucket`, etc.

---

## System & Process Metrics

When running the observability stack via Docker Compose, the OTel
Collector's `hostmetrics` receiver scrapes system-level and barton-core
process-level metrics automatically. The `otel-collector` service shares
barton's PID namespace (`pid: "service:barton"` in
`compose.observability.yaml`) so the collector can read barton-core's
`/proc` entries directly.

The receiver is configured with a 15-second collection interval and three
scrapers: `cpu`, `memory`, and `process` (filtered to
`barton-core-reference`).

### System Metrics

| Metric Name | Description |
|---|---|
| `system.cpu.time` | CPU time by state (user, system, idle, etc.) |
| `system.memory.usage` | Memory usage by state (used, free, buffered, cached, etc.) |

### Process Metrics (barton-core-reference)

| Metric Name | Description |
|---|---|
| `process.cpu.time` | CPU time consumed by barton-core |
| `process.memory.physical_usage` | Resident set size (RSS) |
| `process.threads` | Thread count |
| `process.open_file_descriptors` | Open file descriptor count |

Process metrics include `process.executable.name` and `process.pid`
attributes. Only the `barton-core-reference` process is scraped — the
collector and other container processes are excluded by the name filter.

> **Binary name dependency:** The process scraper matches on the executable
> name `barton-core-reference`. If the binary is renamed, the filter must be
> updated in `docker/otel-collector-config.yaml`.

---

## Prometheus Query Reference

> **Development environment only.** The queries below are written for the
> Prometheus instance included in the Docker Compose observability overlay
> (`compose.observability.yaml`). They are intended for local development
> and debugging. Production deployments may export metrics to a different
> backend (e.g., Grafana Cloud, Datadog, AWS CloudWatch) that uses its own
> query language and conventions.

### Device Fleet

| Panel | PromQL |
|---|---|
| Active devices | `device_active_count` |
| Devices in storage | `storage_device_count` |
| Devices in comm-fail | `device_commfail_current` |
| Comm-fail ratio | `device_commfail_current / (device_active_count > 0)` |
| Comm checks / min | `rate(device_communication_check_performed_total[5m]) * 60` |

### Events & Drivers

| Panel | PromQL |
|---|---|
| Total events produced | `event_produced_total` |
| Events by type (table) | `sum by (event_type) (event_produced_total)` |
| Registered driver count | `count(driver_registered_count_total)` |
| Registered drivers (table) | `driver_registered_count_total` |
| Driver init successes | `driver_init_success_total` |

> **One-shot counters:** Most events (`device.added`, `status.changed`, etc.)
> fire once at startup, so `rate()` drops to zero after the lookback window.
> The queries above use the raw cumulative counter, which stays valid for the
> lifetime of the process. If the service produces a sustained stream of
> events, `rate(event_produced_total[5m])` becomes useful.

### Subsystem Health

| Panel | PromQL |
|---|---|
| Subsystems initialized | `subsystem_initialized_count` |
| Init in-flight | `subsystem_init_started_total - subsystem_init_completed_total` |
| Ready for devices (boolean) | `subsystem_ready_for_devices` |
| Matter initializing (boolean) | `matter_initializing_ratio` |
| Mean init duration | `subsystem_init_duration_seconds_sum / subsystem_init_duration_seconds_count` |
| Init duration p50 | `histogram_quantile(0.5, subsystem_init_duration_seconds_bucket)` |
| Init duration p95 | `histogram_quantile(0.95, subsystem_init_duration_seconds_bucket)` |

> **One-shot histograms:** Subsystem initialization happens once at startup,
> so `rate()` drops to zero after the lookback window elapses and
> `histogram_quantile` returns NaN. The queries above use cumulative bucket
> totals instead, which remain valid for the lifetime of the process. The
> mean (`sum / count`) gives the exact average; the quantile gives the
> nearest bucket boundary.

### Device Telemetry

| Panel | PromQL |
|---|---|
| Resource updates / min by device | `sum(rate(device_resource_update_total[5m])) by (device_id) * 60` |
| Resource updates by device class | `sum(rate(device_resource_update_total[5m])) by (device_class)` |
| Current temperature by device | `device_resource_value{resource_name=~".*temperature.*"}` |
| Boolean state transitions / min | `sum(rate(device_resource_state_change_total[5m])) by (device_id, resource_name) * 60` |
| Firmware version breakdown | `count by (device_firmware_version) (device_resource_update_total)` |
| Zigbee RSSI by device | `zigbee_device_rssi_dBm` |
| Zigbee LQI by device | `zigbee_device_lqi` |
| Mean RSSI across fleet | `avg(zigbee_device_rssi_dBm)` |
| Matter subscriptions established | `matter_subscription_established_total` |
| Matter subscription errors | `matter_subscription_error_total` |
| Subscription report rate / min | `sum(rate(matter_subscription_report_total[5m])) * 60` |
| CASE session mean duration | `matter_case_session_duration_seconds_sum / matter_case_session_duration_seconds_count` |
| CASE session duration p50 | `histogram_quantile(0.5, matter_case_session_duration_seconds_bucket)` |
| CASE session duration p95 | `histogram_quantile(0.95, matter_case_session_duration_seconds_bucket)` |
| CASE session error rate | `rate(matter_case_session_error_total[5m])` |

> **High-cardinality attributes:** Device telemetry metrics carry a rich
> attribute set (`device.id`, `resource.name`, `device.manufacturer`, etc.).
> In a large fleet, Prometheus series count can grow significantly.
> Production deployments should consider applying metric views or attribute
> filters in the OTel Collector to drop unused attributes before export.

### Span-Derived RED Metrics

| Panel | PromQL |
|---|---|
| Request rate by operation | `sum(rate(calls_total[5m])) by (span_name)` |
| Error rate % | `sum(rate(calls_total{status_code="STATUS_CODE_ERROR"}[5m])) / sum(rate(calls_total[5m])) * 100` |
| Latency p50 by operation | `histogram_quantile(0.5, sum(rate(duration_milliseconds_bucket[5m])) by (le, span_name))` |
| Latency p99 by operation | `histogram_quantile(0.99, sum(rate(duration_milliseconds_bucket[5m])) by (le, span_name))` |
| Overall call rate | `sum(rate(calls_total[5m]))` |

### Process (barton-core-reference)

| Panel | PromQL |
|---|---|
| RSS memory (MB) | `process_memory_usage_bytes / 1024 / 1024` |
| Virtual memory (MB) | `process_memory_virtual_bytes / 1024 / 1024` |
| CPU usage (cores) | `rate(process_cpu_time_seconds_total[5m])` |

### System (host)

| Panel | PromQL |
|---|---|
| System memory used (MB) | `system_memory_usage_bytes / 1024 / 1024` |
| CPU by state (stacked) | `sum(rate(system_cpu_time_seconds_total[5m])) by (state)` |
| CPU idle % | `rate(system_cpu_time_seconds_total{state="idle"}[5m]) / ignoring(state) sum(rate(system_cpu_time_seconds_total[5m]))` |

> **Metric name translation:** OTel metric names use dots
> (`device.active.count`) but the Prometheus exporter converts them to
> underscores (`device_active_count`). Counters gain a `_total` suffix and
> histograms gain `_bucket` / `_sum` / `_count` suffixes.
