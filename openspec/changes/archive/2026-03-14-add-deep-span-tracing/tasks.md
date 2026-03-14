## 1. Span context infrastructure

- [x] 1.1 Add `observabilitySpanContextRef()` to `observabilityTracing.h/.cpp`: atomically increment ref_count; add no-op inline stub for `!BARTON_CONFIG_OBSERVABILITY`
- [x] 1.2 Add `_Thread_local ObservabilitySpanContext*` and implement `observabilitySpanContextSetCurrent()` (refs new, releases old) and `observabilitySpanContextGetCurrent()` (returns borrowed pointer) in `observabilityTracing.cpp`; add no-op inline stubs for disabled builds
- [x] 1.3 Unit test: verify `Ref`/`Release` lifecycle (ref count increments, release decrements, double-ref then double-release frees)
- [x] 1.4 Unit test: verify `SetCurrent`/`GetCurrent` TLS isolation across two threads; verify `SetCurrent(NULL)` clears; verify `GetCurrent` returns NULL on fresh thread
- [x] 1.5 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 1.6 Commit working tree (excluding `openspec/`): "feat(observability): add span context ref-counting and thread-local current context API"

## 2. Context propagation plumbing

- [x] 2.1 Add `ObservabilitySpanContext*` field to file-local `commissionMatterDeviceArgs` and `OnboardMatterDeviceArgs` structs in `deviceService.c`; extract context from root span, ref, and pass to thread; release in thread function after setting TLS
- [x] 2.2 Add `ObservabilitySpanContext*` member to `CommissioningOrchestrator`; ref-acquire in constructor, release in destructor; read from TLS (`GetCurrent`) in thread entry to initialize
- [x] 2.3 Add `ObservabilitySpanContext*` member to `DeviceDataCache`; ref-acquire from orchestrator's stored context, release in destructor; set TLS in each Matter platform thread callback before creating child spans
- [x] 2.4 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 2.5 Commit working tree (excluding `openspec/`): "refactor(observability): plumb span context through commissioning orchestrator and device data cache"

## 3. Device lifecycle spans

- [x] 3.1 Add `device.commission` root span in `deviceServiceCommissionDevice()` with attributes `commissioning.payload` and `commissioning.timeout_seconds`; end with success/failure status in `commissionMatterDeviceFunc()`
- [x] 3.2 Add `device.pair` root span in `deviceServiceAddMatterDevice()` with attributes `device.node_id` and `pairing.timeout_seconds`; end with success/failure status in `pairMatterDeviceFunc()`
- [x] 3.3 Add `device.remove` root span in `deviceServiceRemoveDevice()` with attribute `device.uuid`; add `device.driver.remove` child span around the driver `deviceRemoved` callback
- [x] 3.4 Add `device.commission_window.open` root span in `deviceServiceOpenCommissioningWindow()` with attributes `device.node_id` and `window.timeout_seconds`
- [x] 3.5 Convert existing `device.found` span from root to child: use `observabilitySpanStartWithParent(name, observabilitySpanContextGetCurrent())` so it inherits from the discovery or commission context set in TLS
- [x] 3.6 Add `device.configure` child span around driver configure callback in the device-found flow with attribute `device.class`; parent inherited from TLS
- [x] 3.7 Add `device.persist` child span around `jsonDatabaseAddDevice()` in the device-add flow with attribute `device.uuid`; parent inherited from TLS
- [x] 3.8 Unit test: verify `device.commission`, `device.pair`, `device.remove` span creation and attribute setting
- [x] 3.9 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 3.10 Commit working tree (excluding `openspec/`): "feat(observability): add device lifecycle spans for commission, pair, remove, and found"

## 4. Discovery span hierarchy

- [x] 4.1 In `deviceServiceStartDiscovery()`, extract context from the existing `device.discovery` span and set it as TLS current via `observabilitySpanContextSetCurrent()` so callbacks on the same thread inherit it
- [x] 4.2 Verify `device.found` (task 3.5) now becomes a child of `device.discovery` automatically via TLS; clear TLS when discovery ends
- [x] 4.3 Integration test: verify `device.discovery` → `device.found` parent-child relationship during Zigbee discovery
- [x] 4.4 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 4.5 Commit working tree (excluding `openspec/`): "feat(observability): wire discovery span hierarchy via thread-local context"

## 5. Matter subsystem spans

- [x] 5.1 Add `matter.case.connect` child span in `DeviceDataCache`: set TLS from stored context, start span when `GetConnectedDevice()` is called, end in `OnDeviceConnected()` (OK) or `OnDeviceConnectionFailure()` (ERROR with `error.code` attribute)
- [x] 5.2 Add `matter.subscribe` child span in `DeviceDataCache`: start when subscription request is sent in `OnDeviceConnected()`, end in `OnSubscriptionEstablished()` (OK) or `OnError()` (ERROR); use stored context as parent
- [x] 5.3 Add `matter.report` span in `DeviceDataCache`: set TLS from stored context in `OnReportBegin()`, create span, end in `OnReportEnd()` with attribute `device.id`
- [x] 5.4 Add `matter.fabric.remove` child span in `MatterDeviceDriver::DeviceRemoved()`: set TLS from current context before `RunOnMatterSync()` to propagate the `device.remove` span as parent
- [x] 5.5 Unit test: verify `DeviceDataCache` span lifecycle — `matter.case.connect` and `matter.subscribe` spans created and ended correctly with ref-counted context
- [x] 5.6 Integration test: verify `device.commission` → `matter.case.connect` → `matter.subscribe` span hierarchy with shared trace ID and correct parent-child `spanId` relationships
- [x] 5.7 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 5.8 Commit working tree (excluding `openspec/`): "feat(observability): add Matter subsystem spans for CASE, subscription, and fabric removal"

## 6. Zigbee subsystem spans

- [x] 6.1 Add `zigbee.device.discovered` child span in `deviceDiscoveredCallback()` in `zigbeeDriverCommon.c` with attributes `device.eui64` and `device.manufacturer`; parent inherited from TLS (discovery context)
- [x] 6.2 Add `zigbee.cluster.read` span in the `readResource()` path in `zigbeeDriverCommon.c` with attributes `device.uuid`, `resource.uri`, and `endpoint.id`; parent from TLS if available (resource.read context)
- [x] 6.3 Add `zigbee.cluster.write` span in the `writeResource()` path in `zigbeeDriverCommon.c` with attributes `device.uuid`, `resource.uri`, and `endpoint.id`; set as TLS current for child `resource.update` span
- [x] 6.4 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 6.5 Commit working tree (excluding `openspec/`): "feat(observability): add Zigbee subsystem spans for discovery, cluster read, and cluster write"

## 7. Resource update spans

- [x] 7.1 Add `resource.update` span in `MatterDevice::CacheCallback::OnAttributeChanged()` covering the `updateResource()` call with attributes `device.uuid`, `resource.name`, and `endpoint.profile`
- [x] 7.2 Add `resource.update` child span in `writeResource()` in `zigbeeDriverCommon.c` as child of the `zigbee.cluster.write` span covering the `updateResource()` call
- [x] 7.3 Integration test: verify `device.remove` span with `device.driver.remove` child span
- [x] 7.4 Integration test: verify `resource.read` and `resource.write` spans appear with correct attributes
- [x] 7.5 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 7.6 Commit working tree (excluding `openspec/`): "feat(observability): add resource update spans for Matter and Zigbee driver paths"

## 8. Documentation

- [x] 8.1 Update `docs/OBSERVABILITY.md` with span hierarchy diagrams showing the new parent-child relationships
- [x] 8.2 Update `docs/OBSERVABILITY_METRICS.md` tracing section with the full span name inventory and their attributes
- [x] 8.3 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 8.4 Commit working tree (excluding `openspec/`): "docs(observability): add deep span tracing hierarchy and span inventory"
