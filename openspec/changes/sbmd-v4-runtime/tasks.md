## 1. Observability Foundation (separate PR)

- [ ] 1.1 Create `core/src/observability/observabilityMetrics.h` with counter, gauge, histogram opaque types and C API (`observabilityCounterCreate`, `observabilityCounterAdd`, `observabilityGaugeCreate`, `observabilityGaugeRecord`, `observabilityHistogramCreate`, `observabilityHistogramRecord`, plus `WithAttrs` variants). Include no-op inline stubs when `BARTON_CONFIG_OBSERVABILITY` is OFF.
- [ ] 1.2 Implement `observabilityMetrics.cpp` — back instruments with in-process data structures (atomic counters, gauge maps keyed by attribute tuples, histogram with fixed bucket boundaries). Thread-safe.
- [ ] 1.3 Add `BARTON_CONFIG_OBSERVABILITY` CMake option (default ON). Wire `core/src/observability/` sources into `core/CMakeLists.txt`.
- [ ] 1.4 Add `gettelemetry`/`gt` command to the reference app. Route through the existing command IPC flow (like `getstatus`). Dump all registered metrics as JSON to stdout.
- [ ] 1.5 Write unit tests for counter, gauge, histogram instruments — verify increment, record, attribute-keyed tracking, and histogram bucket distribution.
- [ ] 1.6 Write unit test for JSON dump output format.

## 2. Staging — Move v3 Drivers Aside

- [x] 2.1 Move all `.sbmd` files from `core/deviceDrivers/matter/sbmd/specs/` to `core/deviceDrivers/matter/sbmd/specs/v3-pending/`.
- [x] 2.2 Disable non-light integration tests by adding a `@pytest.mark.skip(reason="pending v4 conversion")` or equivalent exclusion for thermostat, door-lock, contact-sensor, temperature-sensor, humidity-sensor, occupancy-sensor, air-quality-sensor, water-leak-detector, and IKEA Timmerflotte test files.
- [x] 2.3 Verify the build succeeds with no `.sbmd` files in the active specs directory and only light tests enabled.

## 3. Result Builder — `SbmdUtils.result()`

- [x] 3.1 Implement `SbmdUtils.result()` in `sbmd-utils.js` — mutable builder with `dataModel.updateResource()` (2/3/4-arg), `dataModel.setMetadata()`, `storage.setPersistentData()`, `storage.setTransientData()`, `device.sendCommand()`, `device.writeAttribute()`, `device.requestCommand()`, `device.readAttribute()`, `log()`, `success()`, `error()`. Non-terminals return builder, terminals return raw `{ops, terminal}` object.
- [ ] 3.2 Remove v3 `SbmdUtils.Response.*` helpers (`value`, `error`, `invoke`, `write`) from `sbmd-utils.js`. (deferred to task group 13 — v3 tests still reference these)
- [x] 3.3 Write JS-level unit tests for the result builder — verify chain structure, terminal enforcement, operation ordering, all operation types. (Can be run via mquickjs in a C++ test harness.)

## 4. SbmdDriver() Registration System

- [x] 4.1 Inject `SbmdDriver` capture function and `__sbmd_registration` global into the mquickjs context at initialization time (evaluate once via `JS_EVAL_REPL`).
- [x] 4.2 Implement constants extraction — text-scan for `constants:` block, brace-match, evaluate as `({...})` object literal, walk properties to get name→value pairs, generate `var` declaration preamble string.
- [x] 4.3 Implement file evaluation — prepend constants preamble, wrap in IIFE, evaluate with `JS_EVAL_REPL`. Read `__sbmd_registration`, reset to null.
- [x] 4.4 Implement registration extraction — walk the registration JSValue to extract metadata (schemaVersion, driverVersion, name, barton, matter, reporting) into C++ structs. Extract aliases, resources, endpoints declarations.
- [x] 4.5 Implement handler extraction — extract handler function JSValues from resource seed/read/write/execute declarations and from attributeHandlers/eventHandlers/commandHandlers entries. Extract supplement declarations.
- [x] 4.6 Write unit tests for constants extraction (valid blocks, edge cases: hex numbers, strings, booleans, trailing commas, empty block).
- [x] 4.7 Write unit tests for full file evaluation and registration extraction — load a minimal `.sbmd.js` test fixture, verify all metadata fields extracted correctly.

## 5. Driver Lifecycle — Activate / Deactivate

- [ ] 5.1 Create driver state model — metadata-only (inactive) vs handlers-rooted (active). Store file path or source text for re-evaluation on activation.
- [ ] 5.2 Implement `Activate()` — re-evaluate `.sbmd.js` file, GC-root handler JSValues via `JS_AddGCRef`. Build dispatch tables (attribute, event, command lookups).
- [ ] 5.3 Implement `Deactivate()` — release GC roots via `JS_DeleteGCRef`, clear dispatch tables.
- [ ] 5.4 Integrate with `SbmdFactory::RegisterDrivers()` — at startup, load all drivers as metadata-only. Then activate drivers that have paired devices in the database.
- [ ] 5.5 Integrate with commissioning flow — activate candidate drivers before claiming, deactivate losers that end up with no devices.
- [ ] 5.6 Write unit tests for activate/deactivate lifecycle — verify handlers are callable after activation, verify GC roots released after deactivation.

## 6. Handler Dispatch and Supplements

- [ ] 6.1 Implement dispatch table construction — resolve aliases to cluster+ID pairs, build `map<(clusterId, attrId/eventId/cmdId), vector<HandlerEntry>>` and wildcard tables. Handle alias form and explicit form (clusterId + attributeId/attributeIds/wildcard).
- [ ] 6.2 Implement supplements resolution — given a supplements declaration, read attribute values from `DeviceDataCache` and resource values from Barton resource store. Build `args.supplements` JS object.
- [ ] 6.3 Implement handler invocation — build `args` JS object (deviceUuid, endpointId, clusterFeatureMaps, trigger field, supplements), call handler JSValue via `JS_PushArg`/`JS_Call`, extract result JSValue.
- [ ] 6.4 Implement attribute handler dispatch — on attribute report callback, look up dispatch table, call matching handlers in priority order (specific → multi → wildcard).
- [ ] 6.5 Implement event handler dispatch — same pattern as attribute dispatch.
- [ ] 6.6 Implement command handler dispatch — same pattern, with pending-request check before falling through to commandHandlers.
- [ ] 6.7 Write unit tests for dispatch table construction, supplements resolution, and handler invocation with mock device data.

## 7. Result Chain Execution

- [ ] 7.1 Implement result JSValue walker — extract `ops` array and `terminal` object from the handler's return value. Walk each op's properties via `JS_GetPropertyStr`.
- [ ] 7.2 Implement non-terminal operation executors — `updateResource` (call Barton resource update API), `setMetadata`, `setPersistentData`, `setTransientData`, `log` (route to icLog). Skip unknown ops with warning.
- [ ] 7.3 Implement terminal executors — `success` (complete resource operation with value), `error` (complete with failure), `sendCommand` (invoke Matter command, use status as completion), `writeAttribute` (write Matter attribute, use status as completion).
- [ ] 7.4 Write unit tests for result execution — verify operations execute in order, terminals complete correctly, unknown ops are skipped.

## 8. Deferred Operations

- [ ] 8.1 Implement `PendingOperation` data structure — parked promise, operation log, trigger context, GC-rooted handler/onError JSValues, response match criteria, per-hop timer, overall deadline, deferral depth counter.
- [ ] 8.2 Implement `requestCommand` terminal — send Matter command, park resource operation, register pending response match, arm per-hop and overall timers.
- [ ] 8.3 Implement `readAttribute` terminal — read Matter attribute, park resource operation, register pending response, arm timers.
- [ ] 8.4 Implement response routing — on incoming command, check pending requests first. If match found, cancel hop timer, call stored handler, execute its result chain. If result is another deferral, re-arm pending state (swap GC roots, update match, reset hop timer). If result is a terminal, complete parked operation.
- [ ] 8.5 Implement timeout handling — on hop timeout, call `onError` handler. On overall deadline expiry, call `onError` for the current hop. Implement max deferral depth check.
- [ ] 8.6 Write unit tests for deferred operations — single-hop park-and-complete, multi-hop re-arming, timeout firing, overall deadline enforcement, max depth exceeded.

## 9. Update SpecBasedMatterDeviceDriver

- [ ] 9.1 Rework `DoRegisterResources` — iterate v4 resource declarations, check prerequisites (same logic, different data source), register Barton resources with modes.
- [ ] 9.2 Rework `DoReadResource` — look up read/seed handler, resolve supplements, invoke handler, execute result chain, return value.
- [ ] 9.3 Rework `DoWriteResource` — look up write handler, invoke, execute result chain (sendCommand/writeAttribute terminal).
- [ ] 9.4 Rework `ExecuteResource` — look up execute handler, invoke, execute result chain (may be deferred).
- [ ] 9.5 Rework `DoSynchronizeDevice` — call seed handlers for all seeded resources.
- [ ] 9.6 Wire attribute/event/command report callbacks to dispatch system (task group 6).
- [ ] 9.7 Integrate driver lifecycle (activate/deactivate) into the driver's `AddDevice`/remove-device flow.

## 10. Update SbmdFactory

- [ ] 10.1 Change `RegisterDriversFromDirectory` to scan for `.sbmd.js` files instead of `.sbmd` files.
- [ ] 10.2 Replace `SbmdParser::ParseFile` with v4 evaluation flow (constants extraction → IIFE eval → registration extraction).
- [ ] 10.3 Integrate with startup activation — after loading all drivers, query device database for paired devices, activate drivers that have devices.
- [ ] 10.4 Write unit test for factory loading `.sbmd.js` files.

## 11. Update Build System

- [ ] 11.1 Update `core/CMakeLists.txt` — remove `SbmdParser.cpp` from source list, remove yaml-cpp dependency from SBMD build (check if used elsewhere first). Add any new source files.
- [ ] 11.2 Replace SBMD schema validation in the build with `.sbmd.js` syntax validation (ensure files parse without errors).
- [ ] 11.3 Regenerate `SbmdUtilsEmbedded.h` from the updated `sbmd-utils.js` (the `embed-js-as-header.py` script).
- [ ] 11.4 Verify full build succeeds with the new source files and removed v3 files.

## 12. Light Driver Conversion

- [ ] 12.1 Write `light.sbmd.js` — constants (EP, CL, ATTR, CMD, RES), aliases (onOff, currentLevel), barton/matter metadata, endpoints with resources (isOn with seed+write, currentLevel optional with seed+write), attributeHandlers for onOff and currentLevel. Match v3 behavior exactly.
- [ ] 12.2 Place `light.sbmd.js` in `core/deviceDrivers/matter/sbmd/specs/`.
- [ ] 12.3 Run light integration tests (`testing/test/light_test.py`) — all must pass.
- [ ] 12.4 Profile JS heap usage with the v4 light driver loaded — compare against v3 baseline using `MQuickJsRuntime::LogMemoryUsage` and observability metrics.

## 13. Remove v3 Infrastructure

- [ ] 13.1 Delete `SbmdParser.h`, `SbmdParser.cpp`, `SbmdSpec.h` (after all drivers converted — can be deferred to after remaining driver conversions).
- [ ] 13.2 Delete `ScriptResult.h`, `ScriptResult.cpp` (replaced by v4 result chain execution).
- [ ] 13.3 Delete `core/deviceDrivers/matter/sbmd/schema/` directory (JSON schema files).
- [ ] 13.4 Remove `sbmdParserTest.cpp` from unit tests. Update `ScriptResultTest.cpp` or replace with v4 equivalents.
- [ ] 13.5 Delete `v3-pending/` staging directory once all drivers are converted.

## 14. Remaining Driver Conversions

- [ ] 14.1 Convert `contact-sensor.sbmd` → `contact-sensor.sbmd.js`, re-enable integration tests.
- [ ] 14.2 Convert `temperature-sensor.sbmd` → `temperature-sensor.sbmd.js`, re-enable integration tests.
- [ ] 14.3 Convert `humidity-sensor.sbmd` → `humidity-sensor.sbmd.js`, re-enable integration tests.
- [ ] 14.4 Convert `occupancy-sensor.sbmd` → `occupancy-sensor.sbmd.js`, re-enable integration tests.
- [ ] 14.5 Convert `water-leak-detector.sbmd` → `water-leak-detector.sbmd.js`, re-enable integration tests.
- [ ] 14.6 Convert `air-quality-sensor.sbmd` → `air-quality-sensor.sbmd.js`, re-enable integration tests.
- [ ] 14.7 Convert `thermostat.sbmd` → `thermostat.sbmd.js`, re-enable integration tests.
- [ ] 14.8 Convert `door-lock.sbmd` → `door-lock.sbmd.js`, re-enable integration tests.
- [ ] 14.9 Convert `ikea-timmerflotte.sbmd` → `ikea-timmerflotte.sbmd.js`, re-enable integration tests.
- [ ] 14.10 Verify all integration tests pass with all v4 drivers.
