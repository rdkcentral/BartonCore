## Why

BartonCore's existing 33 OTel meters cover service-level internals (device lifecycle, subsystem health, driver registration) but provide no per-device telemetry. Operators cannot answer basic fleet questions like "what's the battery level across all sensors?" or "which locks are jammed?" without querying every device individually. Device-specific metrics — temperatures, occupancy states, lock events, power consumption, link quality — are already flowing through the resource update path but are invisible to the observability stack.

## What Changes

- **Instrument the common `updateResource()` path** in `core/src/deviceService.c` to emit OTel metrics whenever a device resource value changes, covering both Zigbee and Matter devices with a single instrumentation point.
- **Emit numeric resource values as OTel gauges** (temperature, humidity, illuminance, power, battery level, dimmer level, position percentage) tagged with rich device metadata: device ID, device class, endpoint profile, resource name, manufacturer, model, hardware version, firmware version, and driver type.
- **Emit state transition counters** for boolean/enum resources (on/off, locked/unlocked, faulted/clear, occupied/vacant) with the same rich attribute set plus the new state value.
- **Emit resource update rate counters** to track how frequently each device is reporting, useful for detecting silent device failures or excessive reporting.
- **Populate metric attributes from device-level resources** (`manufacturer`, `model`, `hardwareVersion`, `firmwareVersion`) and device/endpoint properties (`deviceClass`, `profile`, `managingDeviceDriver`) — these are already stored on every device from the BasicInformation (Matter) and Basic (Zigbee) clusters at pairing time.
- **Instrument Zigbee link quality** (RSSI/LQI) at the existing `handleRssiLqiUpdated()` callback in `zigbeeDriverCommon.c` as per-device gauges.
- **Instrument Matter subscription lifecycle** at `DeviceDataCache` callbacks — subscription establishment counts, resubscription events, and report cycle counters — without touching SBMD spec files or JavaScript mappers.
- **Instrument Matter CASE session metrics** at the existing `OnDeviceConnected` / `OnDeviceConnectionFailure` callbacks in `DeviceDataCache` — session establishment duration histogram and connection error counter.
- **Update `docs/OBSERVABILITY_METRICS.md`** with the new device telemetry metrics tables and Prometheus query examples.

## Non-goals

- Per-driver instrumentation — the design avoids modifying individual Zigbee drivers or SBMD specs.
- Alerting rules, dashboards, or Grafana configuration.
- Historical aggregation or time-series downsampling — Prometheus handles retention.
- Modifying the resource model, event system, or device data structures.
- Adding new ZCL cluster attributes or Matter subscriptions beyond what already exists.

## Capabilities

### New Capabilities
- `otel-device-telemetry`: OTel metrics for per-device resource values (gauges for numeric resources, counters for state transitions), resource update rate tracking, device communication health indicators (Zigbee RSSI/LQI, Matter subscription lifecycle), and Matter CASE session metrics (duration, errors).

### Modified Capabilities
- `otel-reference-demo`: The collector config and Prometheus scrape targets remain unchanged, but documentation is updated with new device telemetry metrics tables and queries.

## Impact

- **Core service** (`core/src/deviceService.c`): `updateResource()` gains conditional observability calls to emit metrics based on resource type (numeric → gauge, boolean/enum → counter). Guarded by `BARTON_CONFIG_OBSERVABILITY`.
- **Zigbee driver common** (`core/deviceDrivers/zigbee/zigbeeDriverCommon.c`): `handleRssiLqiUpdated()` callback gains gauge recordings for RSSI and LQI per device.
- **Matter subsystem** (`core/src/subsystems/matter/DeviceDataCache.cpp`): Subscription lifecycle callbacks gain counters/gauges for establishment, resubscription, and report cycles.
- **Documentation** (`docs/OBSERVABILITY_METRICS.md`): New "Device Telemetry Metrics" section with all metrics and Prometheus queries.
- **CMake flags**: `BCORE_OBSERVABILITY` remains the relevant flag — no new flags needed.
- **Dependencies**: No new library dependencies; uses the existing OTel C API from `core/src/observability/`.
- **Performance**: Guarded by `#ifdef BARTON_CONFIG_OBSERVABILITY` so zero overhead when disabled. When enabled, adds device/endpoint lookups (O(1) hash maps) + OTel API calls per resource update. Device metadata attributes are cached per-device to avoid repeated resource lookups.
