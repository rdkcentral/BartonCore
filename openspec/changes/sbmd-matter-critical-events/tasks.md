## 1. Documentation Fix

- [x] 1.1 Fix `args.event` API table in `docs/SBMD.md`: change `data` to `tlvBase64`
- [x] 1.2 Fix door-lock example event handlers in `docs/SBMD.md`: replace `args.event.data[0]` with `Sbmd.Tlv.decode(args.event.tlvBase64)[0]` in `handleLockOperation`, `handleLockAlarms`, and any other example functions using `args.event.data`

## 2. Contact Sensor Driver

- [x] 2.1 Add `EVT_STATE_CHANGE: 0x0000` constant to `contact-sensor.sbmd.js`
- [x] 2.2 Add `stateChange: { clusterId: CL_BOOLEAN_STATE, eventId: EVT_STATE_CHANGE }` alias to `contact-sensor.sbmd.js`
- [x] 2.3 Add `seed` handler to `faulted` resource: reads `StateValue` attribute supplement to establish initial state (StateValue=true → `"false"`)
- [x] 2.4 Retain `attributeHandlers.handleStateValue` in `contact-sensor.sbmd.js` alongside the new event handler (dual-path)
- [x] 2.5 Add `eventHandlers.handleStateChange` to `contact-sensor.sbmd.js`: decode `args.event.tlvBase64`, map StateValue → `faulted` (StateValue=true → `"false"`)
- [x] 2.6 Bump `driverVersion` to `2` in `contact-sensor.sbmd.js`
- [x] 2.7 Validate `contact-sensor.sbmd.js` against the v5 JSON schema

## 3. Water Leak Detector Driver

- [x] 3.1 Add `EVT_STATE_CHANGE: 0x0000` constant to `water-leak-detector.sbmd.js`
- [x] 3.2 Add `stateChange: { clusterId: CL_BOOLEAN_STATE, eventId: EVT_STATE_CHANGE }` alias to `water-leak-detector.sbmd.js`
- [x] 3.3 Add `seed` handler to `faulted` resource: reads `StateValue` attribute supplement to establish initial state (StateValue=true → `"true"`)
- [x] 3.4 Retain `attributeHandlers.handleStateValue` in `water-leak-detector.sbmd.js` alongside the new event handler (dual-path)
- [x] 3.5 Add `eventHandlers.handleStateChange` to `water-leak-detector.sbmd.js`: decode `args.event.tlvBase64`, map StateValue → `faulted` (StateValue=true → `"true"`)
- [x] 3.6 Bump `driverVersion` to `2` in `water-leak-detector.sbmd.js`
- [x] 3.7 Validate `water-leak-detector.sbmd.js` against the v5 JSON schema

## 4. Door Lock Driver — New Resources

- [x] 4.1 Add event ID constants to `door-lock.sbmd.js`: `EVT_DOOR_LOCK_ALARM: 0x0000`, `EVT_LOCK_OPERATION: 0x0002`
- [x] 4.2 Add alarm code constants: `ALARM_LOCK_JAMMED: 0x00`, `ALARM_WRONG_CODE_ENTRY_LIMIT: 0x04`, `ALARM_FRONT_ESCUTCHEON_REMOVED: 0x05`, `ALARM_DOOR_FORCED_OPEN: 0x06`
- [x] 4.3 Add operation type constants: `OP_TYPE_LOCK: 0x00`, `OP_TYPE_UNLOCK: 0x01`, `OP_TYPE_UNLATCH: 0x04`, `OP_SOURCE_MANUAL: 0x01`
- [x] 4.4 Add resource name constants: `RES_JAMMED: 'jammed'`, `RES_TAMPERED: 'tampered'`, `RES_INVALID_CODE_ENTRY_LIMIT: 'invalidCodeEntryLimit'`
- [x] 4.5 Declare `jammed`, `tampered`, `invalidCodeEntryLimit` resources in `endpoints['1'].resources` with `type: 'boolean', modes: ['read'], prerequisites: [CL_DOOR_LOCK]`

## 5. Door Lock Driver — Event Aliases and Handlers

- [x] 5.1 Add event aliases to `door-lock.sbmd.js`: `doorLockAlarm: { clusterId: CL_DOOR_LOCK, eventId: EVT_DOOR_LOCK_ALARM }`, `lockOperation: { clusterId: CL_DOOR_LOCK, eventId: EVT_LOCK_OPERATION }`
- [x] 5.2 Retain `attributeHandlers.handleLockState` in `door-lock.sbmd.js` alongside the new event handlers (dual-path)
- [x] 5.3 Add `eventHandlers.handleDoorLockAlarm`: decode TLV, branch on alarm code per the alarm-to-resource table, log unresourced codes
- [x] 5.4 Add `eventHandlers.handleLockOperation`: decode TLV fields 0 (opType) and 1 (source), update `locked`, clear `tampered` and `invalidCodeEntryLimit` always, clear `jammed` only if source == `OP_SOURCE_MANUAL`
- [x] 5.5 Bump `driverVersion` to `2` in `door-lock.sbmd.js` (content marker only)
- [x] 5.6 Validate `door-lock.sbmd.js` against the v5 JSON schema
- [x] 5.7 Bump the endpoint `1` `profileVersion` from `3` to `4` so `deviceServiceDeviceNeedsReconfiguring` reconfigures already-commissioned devices and registers the new resources (the top-level `driverVersion` does not trigger reconfiguration)

## 6. Door Lock Integration Tests

- [x] 6.1 Add an `alarm` side-band operation and `handleAlarm(alarmCode)` to `testing/mocks/devices/matterjs/src/DoorLockDevice.js` that emits a `DoorLockAlarm` event
- [x] 6.2 Add `test_alarm_jammed_sets_jammed_resource` to `testing/test/door_lock_test.py` (AlarmCode 0x00 → `jammed = "true"`)
- [x] 6.3 Add `test_alarm_wrong_code_sets_invalid_code_entry_limit_resource` (AlarmCode 0x04 → `invalidCodeEntryLimit = "true"`)
- [x] 6.4 Add `test_alarm_escutcheon_sets_tampered_resource` (AlarmCode 0x05 → `tampered = "true"`)
- [x] 6.5 Add `test_lock_operation_clears_tampered_and_invalid_code` (set both `tampered` (0x05) and `invalidCodeEntryLimit` (0x04) true first, then a lock operation clears both)
- [x] 6.6 Update the existing `test_locked_resource_updated_by_event` comments and docstring to reflect that live `locked` updates now arrive via `LockOperation` events rather than the attribute subscription
- [x] 6.7 Add a `manualOperation` side-band to `DoorLockDevice.js` that emits a `LockOperation` with `OperationSource == Manual`
- [x] 6.8 Add `test_manual_lock_operation_clears_jammed` (Manual operation clears `jammed`) and `test_non_manual_lock_operation_leaves_jammed_set` (ProprietaryRemote operation clears `tampered` but leaves `jammed` set)
- [x] 6.9 Add `test_alarm_door_forced_open_sets_tampered_resource` (AlarmCode 0x06 → `tampered = "true"`; covers the second alarm code in the shared tampered branch)

## 7. Sensor Integration Tests (Mocks, Fixtures, Tests)

- [x] 7.1 Create `testing/mocks/devices/matterjs/src/ContactSensorDevice.js` (device type 0x0015, BooleanState cluster, `stateChange` event enabled via `.alter`, `setStateValue` side-band; initial state closed)
- [x] 7.2 Create `testing/mocks/devices/matterjs/src/WaterLeakDetectorDevice.js` (device type 0x0043, BooleanState cluster with required `stateChange`, `setStateValue` side-band; initial state dry)
- [x] 7.3 Create `testing/mocks/devices/matter/matter_contact_sensor.py` fixture (`matter_contact_sensor`)
- [x] 7.4 Create `testing/mocks/devices/matter/matter_water_leak_detector.py` fixture (`matter_water_leak_detector`)
- [x] 7.5 Register both fixtures in `testing/conftest.py` `pytest_plugins`
- [x] 7.6 Create `testing/test/contact_sensor_test.py` (commission, seed, open→faulted, close→clear)
- [x] 7.7 Create `testing/test/water_leak_detector_test.py` (commission, seed, water→faulted, dry→clear)

## 8. Verification

- [x] 8.1 Build BartonCore and confirm build-time SBMD schema validation passes for all three drivers
- [x] 8.2 Run the full C/C++ unit test suite (401 tests) — all pass
- [x] 8.3 Run the full integration test suite (68 tests, including the 12 new door-lock/sensor tests) — all pass

## 9. Review response (Matter 1.5.1 conformance + Zigbee parity)

- [x] 9.1 Restore dual-path live updates: keep `handleLockState` / `handleStateValue` alongside the event handlers (StateChange/LockOperation are optional/INFO in 1.5.1; same-value updates are suppressed so no duplicate events)
- [x] 9.2 Seed `jammed`, `tampered`, `invalidCodeEntryLimit` to `'false'` at commission (Zigbee parity; avoids null/unknown ambiguity)
- [x] 9.3 Attach `{source, userId}` metadata to the `locked` update from `LockOperation` (map `OperationSourceEnum` → `DOORLOCK_PROFILE_LOCKED_SOURCE_*`, UserIndex at TLV tag 2)
- [x] 9.4 Guard `alarmCode.toString(16)` against a missing TLV tag 0
- [x] 9.5 Align the contact-sensor seed polarity with its event handler (undefined → faulted)
- [x] 9.6 Add `ContactSensorNoEventDevice.js` mock + fixture and a negative test asserting `faulted` still tracks state via the attribute when `StateChange` is absent
- [x] 9.7 Add a door-lock test asserting `jammed`/`tampered`/`invalidCodeEntryLimit` are seeded `'false'` at commission
- [x] 9.9 Add a test asserting unresourced `DoorLockAlarm` codes are log-only (no fault-resource change)
- [x] 9.10 Add `emitLockOperation` side-band + tests for Unlatch (0x04 → `locked=false`) and a non-lock/unlock (no-op) operation type
- [x] 9.11 Add a contact-sensor synchronize-reseed test (goOffline → change state → reconnect → `faulted` re-seeded), plus a `stateValue`-aware `comeOnline` on the mock
- [x] 9.12 Document, in the door-lock spec, the `invalidCodeEntryLimit` timer-vs-operation clearing interface difference and the DoorAjar/DPS out-of-scope consistency note
- [x] 9.13 Make the door-lock fault seed handler preserve an already-set value (read via `supplements.resources`) so a live fault is not clobbered when `SeedInitialResourceValues` re-runs on synchronize; add a test asserting `jammed` survives a reconnect
- [x] 9.14 Add a door-lock test asserting `LockOperation`-driven `locked` updates carry `{source, userId}` metadata (event-only emission via `setState=false` side-band)
- [x] 9.15 Mark `args.event.tlvBase64` optional/absent-for-payload-less in `docs/SBMD.md` and `sbmd-script.d.ts`; rename the sensor spec requirement titles from "event-only" to "dual-path"
- [ ] 9.8 (Deferred — see design.md Risks and the follow-up user stories) `lastUserInteractionDate` (SBMD loader does not extract device-level `resources`; needs a C++ runtime change), timer-based `invalidCodeEntryLimit` clearing (needs an SBMD scheduler primitive), the v3→v4 migration test (harness cannot swap driver versions mid-test), and `doorLock` profileVersion cross-stack reconciliation
