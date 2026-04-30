## 1. Schema and Data Model

- [x] 1.1 Add `seedFromMapper` type to `sbmd-spec-schema.json`: object with required `alias` (string) and `script` (string), and add `seedFrom` as an optional property of the `mapper` object
- [x] 1.2 Add `seedFromAttribute` (`std::optional<SbmdAttribute>`) and `seedFromScript` (string) fields to `SbmdMapper` in `core/deviceDrivers/matter/sbmd/SbmdSpec.h`

## 2. Parser

- [x] 2.1 Add `seedFrom` parsing block in `SbmdParser::ParseMapper()` in `core/deviceDrivers/matter/sbmd/SbmdParser.cpp`: read `alias` and `script` fields, resolve alias via existing alias-resolution logic (attribute aliases only — emit parse error if alias is an event alias)
- [x] 2.2 Add parse error if `seedFrom` is present without `event` on the same mapper (add in `ParseMapper()` alongside the existing event-mapper validation, or in the existing `ValidateMapper()` helper in the same file — either location is acceptable; `ValidateMapper()` is preferred for cross-field consistency checks)
- [x] 2.3 Add parse error if both `read` and `seedFrom` are present on the same mapper (same placement guidance as 2.2)
- [x] 2.4 Add parse error if `seedFrom.script` is absent
- [x] 2.5 Extend `SetMapperIds()` in `SbmdParser.cpp` to stamp `resourceEndpointId` and `resourceId` onto `seedFromAttribute` when it is set (mirrors existing logic for `readAttribute`)

## 3. MatterDevice Binding

- [x] 3.1 Add `seedFromBindings` map to `MatterDevice` in `core/deviceDrivers/matter/MatterDevice.h`: keyed by resource URI, storing the resolved `ResourceBinding` (endpoint/cluster/attribute path) — NOT added to `readableAttributeLookup`
- [x] 3.2 Implement `MatterDevice::BindResourceSeedFromInfo()` in `MatterDevice.cpp`: populates `seedFromBindings` only, mirrors attribute-path portion of `BindResourceReadInfo()` but does not touch `readableAttributeLookup`
- [x] 3.3 Implement a seed-execute method on `MatterDevice` (e.g., `SeedResourceFromAttribute(uri)`): calls `GetCachedAttributeData()` for the binding's attribute path, calls `script->MapAttributeRead()` with the TLV, calls `updateResource()` if TLV is non-null

## 4. SpecBasedMatterDeviceDriver Integration

- [x] 4.1 In `SpecBasedMatterDeviceDriver::AddResourceMappers()` in `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp`: call `script.AddAttributeReadMapper(seedFromAttribute, seedFromScript)` when `seedFromAttribute.has_value()` (reuses existing script interface — no new script engine changes needed)
- [x] 4.2 In the `configureResource()` lambda in `SpecBasedMatterDeviceDriver`: call `device->BindResourceSeedFromInfo(uri, seedFromAttribute, sbmdEndpointIndex)` when `seedFromAttribute.has_value()`
- [x] 4.3 Implement private `SpecBasedMatterDeviceDriver::SeedInitialResourceValues(const std::string &deviceId)`: uses `ForEachNonSkippedResource()` to iterate resources, calls `device->SeedResourceFromAttribute(uri)` for each resource with `seedFromAttribute.has_value()`; URI construction uses `createDeviceResourceUri` for device-level resources and `createEndpointResourceUri` for endpoint-level
- [x] 4.4 Add `DevicePersisted()` override declaration in `SpecBasedMatterDeviceDriver.h` (matching base class signature: `icDevice *device`); implement in `SpecBasedMatterDeviceDriver.cpp` to schedule `SeedInitialResourceValues(deviceId)` on the Matter thread via `RunOnMatterSync()`. This fires after the device is fully persisted and resources exist in the database.
- [x] 4.5 Add `DoSynchronizeDevice()` override declaration in `SpecBasedMatterDeviceDriver.h` (matching base class signature: `promises`, `deviceId`, `exchangeMgr`, `sessionHandle`); implement in `SpecBasedMatterDeviceDriver.cpp` to call `SeedInitialResourceValues(deviceId)` directly (already on Matter thread, resources already in DB).
- [x] 4.6 Declare `SeedInitialResourceValues(const std::string &deviceId)` as a private method in `SpecBasedMatterDeviceDriver.h`

## 5. door-lock.sbmd and Virtual Device Update

- [x] 5.1 Add `lockOperation` event alias to `matterMeta.aliases` in `core/deviceDrivers/matter/sbmd/specs/door-lock.sbmd` (cluster `0x0101`, event `0x0002`)
- [x] 5.2 Add `lockOperation` as a second prerequisite on the `locked` resource in `door-lock.sbmd` (alongside existing `lockState` attribute alias prerequisite)
- [x] 5.3 Replace `mapper.read` with `mapper.event` (alias: `lockOperation`) on the `locked` resource in `door-lock.sbmd` — add the event script to decode `LockOperationEvent` TLV struct: return `"true"` for `LockOperationType` = 0 (Lock), `"false"` for `LockOperationType` = 1 (Unlock), and return no value (no `output` key) for all other operation types (2–4) so the resource is not updated on non-state-change events
- [x] 5.4 Add `mapper.seedFrom` (alias: `lockState`) to the `locked` resource in `door-lock.sbmd` — add the seed script to decode the `LockState` uint8 attribute and return `"true"` for locked (value `1`) and `"false"` for unlocked (value `2`)
- [x] 5.5 Update `testing/mocks/devices/matterjs/src/DoorLockDevice.js`: emit a `LockOperation` event (event ID `0x0002`) on both the sideband and Matter command paths — the `LockOperationType` field SHALL be `0` (Lock) for lock operations and `1` (Unlock) for unlock operations. **Sideband path**: modify `handleLock()` and `handleUnlock()` to emit the event in addition to updating `lockState`. **Matter command path**: introduce a `DoorLockServerWithEvents` subclass of `DoorLockRequirements.DoorLockServer` that overrides `lockDoor()` and `unlockDoor()` to call `super` then emit the event — this is required so that Barton-initiated lock/unlock commands (test 8.4) also update the `locked` resource, which is now event-driven rather than attribute-subscription-driven.

## 6. Parser Unit Tests

- [x] 6.1 Update `test_sbmdParserDoorLockFile` in `core/test/src/sbmdParserTest.cpp` to reflect the updated `door-lock.sbmd`: assert `hasRead == false`, `seedFromAttribute.has_value() == true`, correct `seedFromAttribute` fields (`clusterId`, `attributeId`, `name`), and `event` present
- [x] 6.2 Add `test_sbmdParserSeedFromValidSpec`: inline YAML with a valid `event` + `seedFrom` mapper; assert `seedFromAttribute.has_value() == true`, `seedFromAttribute` and `seedFromScript` populated correctly
- [x] 6.3 Add `test_sbmdParserSeedFromWithoutEvent`: inline YAML with `seedFrom` but no `event`; assert parse error returned
- [x] 6.4 Add `test_sbmdParserSeedFromMissingScript`: inline YAML with `seedFrom.alias` but no `seedFrom.script`; assert parse error returned
- [x] 6.5 Add `test_sbmdParserSeedFromWithRead`: inline YAML with both `read` and `seedFrom` on the same mapper; assert parse error returned
- [x] 6.6 Add `test_sbmdParserSeedFromEventAliasRejected`: inline YAML where `seedFrom.alias` names an event alias (not an attribute alias); assert parse error returned

## 7. Script Engine Unit Test

- [x] 7.1 Add `MapAttributeReadUint8ToBoolean` test in `core/test/src/SbmdScriptTest.cpp`
- [x] 7.2 Add `MapEvent` unit tests in `core/test/src/SbmdScriptTest.cpp` covering the full tri-state contract:
  - `MapEventNoMapper` — false when no mapper registered
  - `AddEventMapperRejectsEmptyScript` — false for empty script
  - `MapEventLockOperationLock` — LockOperationType 0 → "true"
  - `MapEventLockOperationUnlock` — LockOperationType 1 → "false"
  - `MapEventSuppressOnNoOutputKey` — LockOperationType 2 → true + empty outValue (suppress path)
  - `MapEventFailsOnPrimitiveStringReturn` — bare string return → false
  - `MapEventFailsOnNullReturn` — null return → false
  - `MapEventFailsOnUndefinedReturn` — no return statement → false
  - `MapEventFailsOnSyntaxError` — syntax error → false
  - `MapEventHasDeviceUuid` — `sbmdEventArgs.deviceUuid` exposed
  - `MapEventHasClusterId` — `sbmdEventArgs.clusterId` exposed
  - `MapEventHasEventId` — `sbmdEventArgs.eventId` exposed
  - `MapEventHasEventName` — `sbmdEventArgs.eventName` exposed
  - `MapEventHasEndpointId` — `sbmdEventArgs.endpointId` exposed

## 8. Integration Tests

- [x] 8.1 Add `test_locked_resource_seeded_on_commission` to `testing/test/door_lock_test.py`: commission a virtual door lock, immediately read the `locked` resource URI, assert the value is non-null and equals `"true"` (the default locked state of `DoorLockDevice.js`)
- [x] 8.2 Add `test_locked_resource_updated_by_event` to `testing/test/door_lock_test.py`: commission a virtual door lock, verify the initial seed via direct `get_resource_by_uri` read (covers the same ground as 8.1; this read happens before the event queue is created, since `DoRegisterResources` does not emit `RESOURCE_UPDATED`), then create a `resource_update_listener` queue and trigger a sideband unlock action (which after task 5.5 will fire a `LockOperation` event from the device), wait for and assert the `locked` resource becomes `"false"`, then trigger a sideband lock action and assert it becomes `"true"` again
- [x] 8.3 Verify that the existing `test_sideband_unlock_triggers_barton_update` and `test_sideband_lock_triggers_barton_update` tests continue to pass after task 5.5 (they depend on sideband ops causing `locked` resource updates, which after this change requires events to be emitted); update these tests if the expected behavior or assertion mechanism changes
- [x] 8.4 Verify that the existing `test_lock_unlock_via_barton` test continues to pass (Barton-initiated lock/unlock sends Matter commands to the virtual device, which should emit `LockOperation` events via the standard Matter command path — no change expected, but verify)
- [x] 8.5 Add `test_locked_resource_seeded_on_synchronize` to `testing/test/door_lock_test.py`: simulate a comm-fail/recovery scenario where the device state changes while Barton is offline, then assert Barton learns the new state on reconnect via `DoSynchronizeDevice`. Changes required: (a) add `goOffline`/`comeOnline` sideband ops to `VirtualDevice.js` — `goOffline` calls `initiateForceClose()` on all active Matter sessions without sending any Matter messages (no SessionClose, no StatusReport), leaving the `ServerNode` running so mDNS advertisement continues; `comeOnline` is a no-op in the base class (the `ServerNode` never stopped) and `DoorLockDevice.js` overrides it to update the `LockState` attribute before Barton reconnects; (b) `DoorLockDevice.js` accepts an optional `lockState` parameter in `handleComeOnline()` so the device can come back online in a different lock state than it went offline in. Test flow: (1) commission (device starts locked, `locked="true"`); (2) set `commFailOverrideSeconds=3` via `client.write_metadata` so the comm-fail watchdog fires in ~3 s; (3) send `goOffline` sideband op; (4) wait for `communicationFailure` resource → `"true"` (watchdog fires naturally after ~3 s); (5) send `comeOnline({lockState: "unlocked"})` sideband op — device updates its `LockState` attribute to `Unlocked`; (6) set `matterLivenessTimeoutOverrideMs=1` via `client.write_metadata` (see task 11.1) — the ReadClient's liveness timer still has ~14 s remaining at this point, so the 1 ms override fires it immediately, triggering DefaultResubscribePolicy to open a new CASE session; since the device is now online (the `ServerNode` never stopped), CASE succeeds; the priming report with `LockState=Unlocked` causes `communicationRestored` → `DoSynchronizeDevice` → `SeedInitialResourceValues`, which reads `LockState=Unlocked` from the refreshed attribute cache and calls `updateResource` with `"false"`; (7) assert `locked` becomes `"false"`.

## 9. Documentation

- [x] 9.1 Update `docs/SBMD.md` to document the `seedFrom` mapper type
- [x] 9.2 Update `openspec/specs/sbmd-system/spec.md` to add the `seedFrom` mapper type requirement alongside the existing mapper type requirements (this change merges the delta from `changes/sbmd-event-resource-seeding/specs/sbmd-system/spec.md` into the live spec)
- [x] 9.3 Update `openspec/specs/matterjs-door-lock-device/spec.md` to add a requirement that sideband `lock` and `unlock` operations SHALL emit a `LockOperation` event (in addition to updating the `LockState` attribute), to support event-driven resource updates

## 10. PR Review Fixes

- [x] 10.1 Fix `DevicePersisted` timing: move commission-time seeding from `DevicePersisted` into `DoRegisterResources`. `DoRegisterResources` is dispatched on the Matter thread via `RunOnMatterSync`, so `GetCachedAttributeData()` is safe to call there. Add `MatterDevice::ReadSeedValueFromAttribute(uri)` that returns `std::optional<std::string>` (cache read + script mapping, no `updateResource`). In `DoRegisterResources`, for each resource with `seedFromAttribute.has_value()`, call `ReadSeedValueFromAttribute` and pass the result as the initial value to `createDeviceResource`/`createEndpointResource`. Remove `DevicePersisted` override — no longer needed. `DoSynchronizeDevice` and `SeedResourceFromAttribute` (which delegates to `ReadSeedValueFromAttribute`) remain unchanged for the reconnect path.

## 11. Integration Test Infrastructure

- [x] 11.1 Add `matterLivenessTimeoutOverrideMs` device metadata property support to `MatterDeviceDriver` to support `test_locked_resource_seeded_on_synchronize` (task 8.5): (a) a new `metadataUpdated(driver, device, key, value)` callback is added to the `DeviceDriver` struct (parallel to `commFailTimeoutSecsChanged`) and called from `setMetadata()` in `deviceService.c` after each persisted change; (b) `MatterDeviceDriver` sets this callback as a lambda in its constructor, checks for the `matterLivenessTimeoutOverrideMs` key, and calls `DeviceDataCache::OverrideLiveness(uint32_t ms)` via `chip::DeviceLayer::SystemLayer().ScheduleLambda()` when the value is non-zero; (c) `DeviceDataCache::OverrideLiveness(uint32_t ms)` calls `readClient->OverrideLivenessTimeout(Milliseconds32(ms))` — when `ms=1` this causes `CHIP_ERROR_TIMEOUT` to fire on the next event-loop tick, which triggers `DefaultResubscribePolicy` to open a new CASE session (`ForceCASE=true`); (d) `DeviceDataCache::OnSubscriptionEstablished` resets `mLivenessTimeoutOverride` to `Clock::kZero` after each successful subscription to prevent rapid-resubscription loops. Both this property and `commFailOverrideSeconds` are set via `b_core_client_write_metadata` — the standard Barton client API — so no test-only code or ctypes bindings are added to the production binary.
- ~~11.2 Add `--max-interval <ms>` CLI flag to `testing/mocks/devices/matterjs/src/parseArgs.js` and wire it through `VirtualDevice.js` constructor (`maxInterval = 3000`) to `ServerNode.subscriptionOptions.maxInterval`. This allows individual tests to request a short subscription max-interval from the virtual device, reducing the liveness window that Barton negotiates for faster resubscription-related tests.~~ **Removed**: superseded by `matterLivenessTimeoutOverrideMs` (task 11.1), which collapses the liveness window on demand without requiring a device-side configuration change.
- ~~11.3 Add `extra_args: list = None` parameter to `MatterDevice.__init__` in `testing/mocks/devices/matter/matter_device.py` and `max_interval: int = None` to `MatterDoorLock.__init__` in `testing/mocks/devices/matter/matter_door_lock.py`, so tests can pass `--max-interval` (and other future flags) to the virtual device subprocess without modifying the fixture.~~ **Removed**: no longer needed after 11.2 was dropped.
- [x] 11.4 Make `commFailOverrideSeconds` metadata "live": extend `setMetadata()` in `deviceService.c` to call `deviceServiceCommFailSetDeviceTimeoutSecs(device, deviceServiceCommFailGetTimeoutSecs())` whenever `COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS` is persisted. This reprograms the running per-device watchdog timer immediately so the new timeout takes effect without a device reconnect or service restart.
- [x] 11.5 Add `barton.commFail.monitorIntervalSecs` property (`COMM_FAIL_MONITOR_INTERVAL_SECS_PROP` in `devicePrivateProperties.h`): read in `deviceService.c` immediately after `deviceServiceCommFailInit()` and applied via `deviceCommunicationWatchdogSetMonitorInterval()` when non-zero. Set to `"1"` in a dedicated `fast_commfail_environment` pytest fixture in `door_lock_test.py` (not in `DefaultEnvironmentOrchestrator` — only `test_locked_resource_seeded_on_synchronize` needs it). The property must be set before `start_client()` since it is consumed at `deviceServiceStart()` time. In production the property is absent and the 60 s default check interval is unchanged.
