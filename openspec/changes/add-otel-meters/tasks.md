## 1. Extend Metrics C API — Attributes and Histograms

- [x] 1.1 Add `observabilityCounterAddWithAttrs()` declaration and no-op stub to `observabilityMetrics.h` — NULL-terminated variadic `const char *key, const char *value` pairs
- [x] 1.2 Add `observabilityGaugeRecordWithAttrs()` declaration and no-op stub to `observabilityMetrics.h`
- [x] 1.3 Add `ObservabilityHistogram` opaque type, `observabilityHistogramCreate()`, `observabilityHistogramRecord()`, `observabilityHistogramRecordWithAttrs()`, and `observabilityHistogramRelease()` declarations with no-op stubs to `observabilityMetrics.h`
- [x] 1.4 Implement `observabilityCounterAddWithAttrs()` in `observabilityMetrics.cpp` — parse variadic args into `opentelemetry::common::KeyValueIterableView` and call `Counter::Add(value, attributes)`
- [x] 1.5 Implement `observabilityGaugeRecordWithAttrs()` in `observabilityMetrics.cpp` — store attributes alongside value for the observable callback
- [x] 1.6 Implement histogram functions in `observabilityMetrics.cpp` — `CreateDoubleHistogram`, record, record-with-attrs, release with ref-counting

## 2. Metrics API Unit Tests

- [x] 2.1 Add Google Test cases for `observabilityCounterAddWithAttrs` — verify attributes appear in in-memory metric export
- [x] 2.2 Add Google Test cases for `observabilityHistogramCreate`/`Record` — verify sum, count, and bucket distribution in exported data
- [x] 2.3 Add Google Test case for `observabilityGaugeRecordWithAttrs` — verify attributes in exported gauge data
- [x] 2.4 Verify no-op stubs compile and link when `BARTON_CONFIG_OBSERVABILITY` is undefined

## 3. Verify & Commit — Metrics API

- [x] 3.1 Build the project and run all BartonCore unit tests (`ctest --output-on-failure --test-dir build`) — verify zero regressions
- [x] 3.2 Commit: `feat(otel): extend metrics C API with attribute support and histograms`

## 4. Device Lifecycle Meters

- [x] 4.1 Add `#include "observability/observabilityMetrics.h"` and static counter/histogram declarations to `deviceService.c` for discovery, add, remove, and rejection meters
- [x] 4.2 Create all device lifecycle meters in `deviceServiceInitialize()` — `device.discovery.started`, `device.discovery.completed`, `device.discovered.count`, `device.add.success`, `device.add.failed`, `device.remove.success`, `device.remove.failed`, `device.rejected.count`, `device.discovery.duration`
- [x] 4.3 Instrument `deviceServiceDiscoverStart()` — increment `device.discovery.started` with subsystem attribute, capture start time
- [x] 4.4 Instrument discovery completion path — increment `device.discovery.completed`, record `device.discovery.duration` histogram
- [x] 4.5 Instrument `finalizeNewDevice()` / device add path — increment `device.add.success` or `device.add.failed` with `device.class` and `subsystem.name` attributes, increment `device.discovered.count`
- [x] 4.6 Instrument `deviceServiceRemoveDevice()` — increment `device.remove.success` or `device.remove.failed` with attributes
- [x] 4.7 Instrument `checkDeviceDiscoveryFilters()` rejection path — increment `device.rejected.count` with attributes
- [x] 4.8 Release all device lifecycle meters in `deviceServiceShutdown()`

## 5. Device Lifecycle Meters Tests

- [x] 5.1 Add Google Test unit tests verifying device lifecycle counter creation and increment calls
- [x] 5.2 Add pytest integration test verifying `device.discovery.started` and `device.add.success` metrics appear in OTLP export after a simulated device lifecycle

## 6. Verify & Commit — Device Lifecycle Meters

- [x] 6.1 Build the project and run all BartonCore unit tests — verify zero regressions
- [x] 6.2 Run integration tests (`scripts/ci/run_integration_tests.sh`) — verify zero regressions
- [x] 6.3 Commit: `feat(otel): add device lifecycle meters`

## 7. Matter Subsystem Meters

- [x] 7.1 Add static counter/histogram/gauge declarations to `matterSubsystem.cpp` and/or `CommissioningOrchestrator` for commissioning, pairing, OTA, and init meters
- [x] 7.2 Create all Matter meters during Matter subsystem initialization — `matter.commissioning.attempt`, `matter.commissioning.success`, `matter.commissioning.failed`, `matter.commissioning.duration`, `matter.pairing.attempt`, `matter.pairing.success`, `matter.pairing.failed`, `matter.ota.query_image.received`, `matter.ota.query_image.available`, `matter.ota.query_image.not_available`, `matter.ota.apply_update.received`, `matter.initializing`
- [x] 7.3 Instrument `CommissioningOrchestrator::Commission()` — increment attempt counter at entry, success/failed counters at completion with `failure.stage` attribute, record duration histogram
- [x] 7.4 Instrument `CommissioningOrchestrator::Pair()` — increment pairing attempt/success/failed counters
- [x] 7.5 Instrument `OTAProviderImpl::HandleQueryImage()` and `HandleApplyUpdateRequest()` — increment OTA counters
- [x] 7.6 Instrument `maybeInitMatter()` — set `matter.initializing` gauge to 1 at start, 0 at completion
- [x] 7.7 Release all Matter meters during Matter subsystem shutdown

## 8. Matter Meters Tests

- [x] 8.1 Add Google Test unit tests for Matter commissioning and pairing counter instrumentation
- [x] 8.2 Add pytest integration test verifying Matter-specific metrics appear in OTLP export (requires Docker for Matter stack)

## 9. Verify & Commit — Matter Meters

- [x] 9.1 Build the project and run all BartonCore unit tests — verify zero regressions
- [x] 9.2 Run integration tests — verify zero regressions
- [x] 9.3 Commit: `feat(otel): add Matter subsystem meters`

## 10. Zigbee Subsystem Meters

- [x] 10.1 Add `#include "observability/observabilityMetrics.h"` and static counter/gauge declarations to `zigbeeEventHandler.c`, `zigbeeHealthCheck.c`, `zigbeeDefender.c`, and `zigbeeSubsystem.c`
- [x] 10.2 Create all Zigbee meters during Zigbee subsystem initialization — `zigbee.device.join.count`, `zigbee.device.announce.count`, `zigbee.device.processed.count`, `zigbee.health_check.performed`, `zigbee.health_check.enabled`, `zigbee.interference.detected`, `zigbee.interference.resolved`, `zigbee.interference.active`, `zigbee.pan_id_attack.detected`, `zigbee.pan_id_attack.active`, `zigbee.network.startup.completed`
- [x] 10.3 Instrument `zigbeeEventHandler.c` — increment join, announce, and processed counters in respective callbacks
- [x] 10.4 Instrument `zigbeeHealthCheck.c` — increment `zigbee.health_check.performed` on each cycle, set `zigbee.health_check.enabled` gauge on start/stop
- [x] 10.5 Instrument `zigbeeHealthCheckSetProblem()` — increment `zigbee.interference.detected`/`resolved`, set active gauge
- [x] 10.6 Instrument `zigbeeDefenderSetPanIdAttack()` — increment `zigbee.pan_id_attack.detected`, set active gauge
- [x] 10.7 Instrument Zigbee network startup path — increment `zigbee.network.startup.completed`
- [x] 10.8 Release all Zigbee meters during Zigbee subsystem shutdown

## 11. Zigbee Meters Tests

- [x] 11.1 Add Google Test unit tests for Zigbee meter creation and counter increment calls
- [x] 11.2 Add pytest integration test verifying Zigbee-specific metrics appear in OTLP export (requires Docker for Zigbee stack)

## 12. Verify & Commit — Zigbee Meters

- [x] 12.1 Build the project and run all BartonCore unit tests — verify zero regressions
- [x] 12.2 Run integration tests — verify zero regressions
- [x] 12.3 Commit: `feat(otel): add Zigbee subsystem meters`

## 13. Subsystem Manager Meters

- [x] 13.1 Add `#include "observability/observabilityMetrics.h"` and static counter/gauge/histogram declarations to `subsystemManager.c`
- [x] 13.2 Create all subsystem manager meters in `subsystemManagerInitialize()` — `subsystem.init.started`, `subsystem.init.completed`, `subsystem.init.failed`, `subsystem.initialized.count`, `subsystem.ready_for_devices`, `subsystem.init.duration`
- [x] 13.3 Instrument `initializeSubsystemCallback()` — increment `subsystem.init.started` with `subsystem.name`, capture start time
- [x] 13.4 Instrument `onSubsystemInitialized()` — increment `subsystem.init.completed` with `subsystem.name`, record duration histogram, update `subsystem.initialized.count` gauge
- [x] 13.5 Instrument `readyForDevicesCB()` — set `subsystem.ready_for_devices` gauge to 1
- [x] 13.6 Release all subsystem manager meters in `subsystemManagerShutdown()`

## 14. Subsystem Manager Meters Tests

- [x] 14.1 Add Google Test unit tests for subsystem manager meters — verify init counters and readiness gauge
- [x] 14.2 Add pytest integration test verifying `subsystem.init.completed` and `subsystem.ready_for_devices` metrics in OTLP export

## 15. Verify & Commit — Subsystem Manager Meters

- [x] 15.1 Build the project and run all BartonCore unit tests — verify zero regressions
- [x] 15.2 Run integration tests — verify zero regressions
- [x] 15.3 Commit: `feat(otel): add subsystem manager meters`

## 16. Internal Service Meters — Driver Manager, Watchdog, Storage, Events

- [ ] 16.1 Add static meter declarations and includes to `deviceDriverManager.c` — `driver.init.started`, `driver.init.success`, `driver.init.failed`, `driver.discovery.requested`, `driver.discovery.completed`, `driver.registered.count`
- [ ] 16.2 Instrument driver init and discovery paths in `deviceDriverManager.c`, create meters at init, release at shutdown
- [ ] 16.3 Add static meter declarations to `deviceCommunicationWatchdog.c` — `device.commfail.current` gauge, `device.communication.check.performed` counter
- [ ] 16.4 Instrument watchdog thread — increment check counter per cycle, update `device.commfail.current` gauge when devices enter/exit comm-fail
- [ ] 16.5 Add static meter declarations to `database/jsonDatabase.c` — `storage.device.persist` counter, `storage.device.count` gauge
- [ ] 16.6 Instrument database persist and add/remove paths, create meters at database init, release at shutdown
- [ ] 16.7 Add `storage.restore.attempt`, `storage.restore.success`, `storage.restore.failed` counters to `deviceService.c` restore path
- [ ] 16.8 Add static meter declarations to `event/deviceEventProducer.c` — `event.produced` counter
- [ ] 16.9 Instrument GObject signal emission paths in `deviceEventProducer.c` — increment `event.produced` with `event.type` attribute per signal

## 17. Internal Service Meters Tests

- [ ] 17.1 Add Google Test unit tests for driver manager, watchdog, storage, and event meter creation and increment
- [ ] 17.2 Add pytest integration test verifying `driver.init.success`, `storage.device.count`, and `event.produced` metrics in OTLP export

## 18. Verify & Commit — Internal Service Meters

- [ ] 18.1 Build the project and run all BartonCore unit tests — verify zero regressions
- [ ] 18.2 Run integration tests — verify zero regressions
- [ ] 18.3 Commit: `feat(otel): add internal service meters (drivers, watchdog, storage, events)`

## 19. Documentation Update

- [ ] 19.1 Update `docs/OBSERVABILITY.md` — add section listing all new metrics with names, types, attributes, and descriptions
- [ ] 19.2 Update `openspec/specs/otel-metrics/spec.md` — reflect expanded API and metric catalog

## 20. Verify & Commit — Documentation

- [ ] 20.1 Build the project and run all BartonCore unit tests — final full regression check
- [ ] 20.2 Run integration tests — final full regression check
- [ ] 20.3 Commit: `docs(otel): document new metrics catalog and updated API`
