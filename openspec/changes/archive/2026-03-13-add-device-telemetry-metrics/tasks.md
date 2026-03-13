## 1. Device Metadata Cache & Core Resource Telemetry

- [x] 1.1 Create `deviceTelemetry.c` / `deviceTelemetry.h` in `core/src/observability/` with lazy meter creation (`ensureDeviceTelemetryMetersCreated()`), `device.resource.value` gauge, `device.resource.state_change` counter, `device.resource.update` counter
- [x] 1.2 Implement per-device metadata cache (hash map keyed by device UUID) that stores: device class, endpoint profile, manufacturer, model, hardware version, firmware version, driver type
- [x] 1.3 Implement `getOrPopulateDeviceAttributes()` that looks up cached attributes or populates from device/endpoint/resource lookups on first access
- [x] 1.4 Add cache invalidation on device removal and firmware version resource updates
- [x] 1.5 Add `deviceTelemetryRecordResourceUpdate()` function that inspects resource type and emits the appropriate metric with full attribute set (gauge for numeric, counter for boolean, update counter for all changes)
- [x] 1.6 Integrate `deviceTelemetryRecordResourceUpdate()` into `updateResource()` in `core/src/deviceService.c`, gated by `#ifdef BARTON_CONFIG_OBSERVABILITY`, called after value-changed check
- [x] 1.7 Add unit tests for device telemetry: numeric gauge emission (integer + percentage resource types), boolean state change counter, string resource no-op, unchanged-value no-op, attribute population from device metadata cache, cache invalidation on device removal
- [x] 1.8 Run unit tests (`ctest`), run integration tests, fix any regressions
- [x] 1.9 Commit: "feat(observability): add device resource telemetry metrics with metadata cache"

## 2. Zigbee Link Quality

- [x] 2.1 Create `zigbee.device.rssi` gauge and `zigbee.device.lqi` gauge in `zigbeeDriverCommon.c` with lazy initialization
- [x] 2.2 Instrument `handleRssiLqiUpdated()` callback to record RSSI/LQI gauges with `device.id` attribute
- [x] 2.3 Add unit test verifying RSSI/LQI gauges are recorded on diagnostics update callback
- [x] 2.4 Run unit tests, run integration tests, fix any regressions
- [x] 2.5 Commit: "feat(observability): add Zigbee RSSI/LQI device telemetry gauges"

## 3. Matter Subscription & CASE Session

- [x] 3.1 Create `matter.subscription.established`, `matter.subscription.report`, and `matter.subscription.error` counters in `DeviceDataCache.cpp` with lazy initialization
- [x] 3.2 Instrument `OnSubscriptionEstablished()`, `OnReportEnd()`, and subscription error callbacks with counter increments, using `device.id` attribute
- [x] 3.3 Capture monotonic timestamp before `GetConnectedDevice()` in `DeviceDataCache::Start()`
- [x] 3.4 Create `matter.case.session.duration` histogram and `matter.case.session.error` counter with lazy initialization
- [x] 3.5 Record duration to histogram in `OnDeviceConnectedCallback()`; increment error counter with `error` attribute in `OnDeviceConnectionFailureCallback()`
- [x] 3.6 Add unit tests for Matter subscription counters and CASE session metrics (establishment, report cycle, error paths, duration histogram recording)
- [x] 3.7 Run unit tests, run integration tests (Matter light fixture: verify subscription.established and subscription.report counters increment during commissioning and attribute subscription flows), fix any regressions
- [x] 3.8 Commit: "feat(observability): add Matter subscription lifecycle and CASE session metrics"

## 4. Integration Test Expansion

- [x] 4.1 Add integration test: commission a Matter light fixture and verify `device.resource.value` gauge emits for dimming level and `device.resource.state_change` counter emits for on/off toggle
- [x] 4.2 Add integration test: verify `matter.subscription.established` counter increments after commissioning a Matter light fixture
- [x] 4.3 Add integration test: verify `device.resource.update` counter increments when a Matter light attribute report is received
- [x] 4.4 Run full integration test suite, fix any regressions
- [x] 4.5 Commit: "test(observability): add integration tests for device telemetry metrics"

## 5. Documentation

- [x] 5.1 Add "Device Telemetry Metrics" section to `docs/OBSERVABILITY_METRICS.md` with tables for resource gauges, state counters, update counters, Zigbee link quality, Matter subscription, and CASE session metrics
- [x] 5.2 Add "Device Telemetry" query group to the Prometheus Query Reference section with PromQL examples (fleet temperature, device update rates, RSSI distribution, state transitions, firmware version breakdown, subscription health)
- [x] 5.3 Update `docs/OBSERVABILITY.md` with device telemetry overview, cross-references to new metrics tables in OBSERVABILITY_METRICS.md, and any architecture diagram updates
- [x] 5.4 Commit: "docs(observability): add device telemetry metrics documentation and PromQL queries"

## 6. Bug Fixes & Polish (post-implementation)

- [x] 6.1 Expand `device.resource.value` gauge dispatch to cover all numeric resource types (lightLevel, temperature, seconds, minutes, milliWatts, watts, battery.voltage, humidity.relative, signalStrength, motionSensitivity, illuminance, magneticFieldStrength)
- [x] 6.2 Fix `ObservabilityGauge` to support multi-dimensional observations — replace single `currentValue`/`currentAttrs` with a map keyed by sorted attribute vectors so each unique attribute combination is preserved across scrape callbacks
- [x] 6.3 Add `OTEL_METRIC_EXPORT_INTERVAL_MS=1000` to all reference app VS Code launch configurations for higher-resolution gauge data during development
- [x] 6.4 Fix Zigbee RSSI/LQI PromQL metric names in OBSERVABILITY_METRICS.md (`zigbee_device_rssi_dBm`, `zigbee_device_lqi`)
- [x] 6.5 Add CASE session mean duration query to PromQL reference
- [x] 6.6 Update gauge description to list all 14 supported numeric resource types
