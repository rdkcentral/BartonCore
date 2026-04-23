## 1. Schema and Data Model

- [x] 1.1 Add `seedFromMapper` type to `sbmd-spec-schema.json`: object with required `alias` (string) and `script` (string), and add `seedFrom` as an optional property of the `mapper` object
- [x] 1.2 Add `hasInitialRead` (bool, default false), `initialReadAttribute` (`std::optional<SbmdAttribute>`), and `initialReadScript` (string) fields to `SbmdMapper` in `core/deviceDrivers/matter/sbmd/SbmdSpec.h`

## 2. Parser

- [x] 2.1 Add `seedFrom` parsing block in `SbmdParser::ParseMapper()` in `core/deviceDrivers/matter/sbmd/SbmdParser.cpp`: read `alias` and `script` fields, resolve alias via existing alias-resolution logic (attribute aliases only — emit parse error if alias is an event alias)
- [x] 2.2 Add parse error if `seedFrom` is present without `event` on the same mapper (add in `ParseMapper()` alongside the existing event-mapper validation, or in the existing `ValidateMapper()` helper in the same file — either location is acceptable; `ValidateMapper()` is preferred for cross-field consistency checks)
- [x] 2.3 Add parse error if both `read` and `seedFrom` are present on the same mapper (same placement guidance as 2.2)
- [x] 2.4 Add parse error if `seedFrom.script` is absent
- [x] 2.5 Extend `SetMapperIds()` in `SbmdParser.cpp` to stamp `resourceEndpointId` and `resourceId` onto `initialReadAttribute` when `hasInitialRead` is true (mirrors existing logic for `readAttribute`)

## 3. MatterDevice Binding

- [x] 3.1 Add `seedFromBindings` map to `MatterDevice` in `core/deviceDrivers/matter/MatterDevice.h`: keyed by resource URI, storing the resolved `ResourceBinding` (endpoint/cluster/attribute path) — NOT added to `readableAttributeLookup`
- [x] 3.2 Implement `MatterDevice::BindResourceSeedFromInfo()` in `MatterDevice.cpp`: populates `seedFromBindings` only, mirrors attribute-path portion of `BindResourceReadInfo()` but does not touch `readableAttributeLookup`
- [x] 3.3 Implement a seed-execute method on `MatterDevice` (e.g., `SeedResourceFromAttribute(uri)`): calls `GetCachedAttributeData()` for the binding's attribute path, calls `script->MapAttributeRead()` with the TLV, calls `updateResource()` if TLV is non-null

## 4. SpecBasedMatterDeviceDriver Integration

- [x] 4.1 In `SpecBasedMatterDeviceDriver::AddResourceMappers()` in `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp`: call `script.AddAttributeReadMapper(initialReadAttribute, initialReadScript)` when `hasInitialRead` is true (reuses existing script interface — no new script engine changes needed)
- [x] 4.2 In the `configureResource()` lambda in `SpecBasedMatterDeviceDriver`: call `device->BindResourceSeedFromInfo(uri, initialReadAttribute, sbmdEndpointIndex)` when `hasInitialRead` is true
- [x] 4.3 Implement private `SpecBasedMatterDeviceDriver::SeedInitialResourceValues(const std::string &deviceId)`: iterates device-level and endpoint-level resources from `spec`, skips resources in `skippedOptionalResources` using `MakeResourceKey()`, calls `device->SeedResourceFromAttribute(uri)` for each resource with `hasInitialRead`; URI construction mirrors the `configureResource()` lambda (`createDeviceResourceUri` for device-level, `createEndpointResourceUri` for endpoint-level)
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

- [x] 6.1 Update `test_sbmdParserDoorLockFile` in `core/test/src/sbmdParserTest.cpp` to reflect the updated `door-lock.sbmd`: assert `hasRead == false`, `hasInitialRead == true`, correct `initialReadAttribute` fields (`clusterId`, `attributeId`, `name`), and `event` present
- [x] 6.2 Add `test_sbmdParserSeedFromValidSpec`: inline YAML with a valid `event` + `seedFrom` mapper; assert `hasInitialRead == true`, `initialReadAttribute` and `initialReadScript` populated correctly
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
- [x] 8.2 Add `test_locked_resource_updated_by_event` to `testing/test/door_lock_test.py`: commission a virtual door lock, trigger a sideband unlock action (which after task 5.5 will fire a `LockOperation` event from the device), wait for and assert the `locked` resource becomes `"false"`, then trigger a sideband lock action and assert it becomes `"true"` again
- [x] 8.3 Verify that the existing `test_sideband_unlock_triggers_barton_update` and `test_sideband_lock_triggers_barton_update` tests continue to pass after task 5.5 (they depend on sideband ops causing `locked` resource updates, which after this change requires events to be emitted); update these tests if the expected behavior or assertion mechanism changes
- [x] 8.4 Verify that the existing `test_lock_unlock_via_barton` test continues to pass (Barton-initiated lock/unlock sends Matter commands to the virtual device, which should emit `LockOperation` events via the standard Matter command path — no change expected, but verify)

## 9. Documentation

- [x] 9.1 Update `docs/SBMD.md` to document the `seedFrom` mapper type
- [x] 9.2 Update `openspec/specs/sbmd-system/spec.md` to add the `seedFrom` mapper type requirement alongside the existing mapper type requirements (this change merges the delta from `changes/sbmd-event-resource-seeding/specs/sbmd-system/spec.md` into the live spec)
- [x] 9.3 Update `openspec/specs/matterjs-door-lock-device/spec.md` to add a requirement that sideband `lock` and `unlock` operations SHALL emit a `LockOperation` event (in addition to updating the `LockState` attribute), to support event-driven resource updates
