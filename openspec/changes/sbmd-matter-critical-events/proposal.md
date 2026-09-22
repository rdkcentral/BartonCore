## Why

Matter Critical events (Informational or Critical priority) are delivered with higher reliability in congested mesh networks than attribute reports. Without event handlers, the three highest-priority sensor and lock drivers miss state updates that Matter devices are explicitly sending for reliability — defeating the purpose of the Critical event mechanism.

## What Changes

- **Door Lock driver** (`door-lock.sbmd.js`): Add event handlers for `DoorLockAlarm` and `LockOperation`. Introduce three new resources: `jammed`, `tampered`, and `invalidCodeEntryLimit` (all pre-defined in `commonDeviceDefs.h`). Bump the endpoint `profileVersion` to 4 to trigger reconfiguration for the new resources, and bump `driverVersion` to 2 as a content marker.
- **Contact Sensor driver** (`contact-sensor.sbmd.js`): Add event handler for `BooleanState.StateChange`. Bump `driverVersion` to 2.
- **Water Leak Detector driver** (`water-leak-detector.sbmd.js`): Add event handler for `BooleanState.StateChange`. Bump `driverVersion` to 2.
- **SBMD.md documentation fix**: Correct `args.event.data` → `args.event.tlvBase64` in the API table and all affected examples.
- **Integration test coverage**: Add door lock alarm/operation tests plus new Contact Sensor and Water Leak Detector virtual-device mocks, pytest fixtures, and `StateChange` event tests.

### Non-goals

- **`DoorStateChange` event** (Door Position Sensor feature): No field-deployed devices have this feature. The Aqara Smart Lock U400, which is known to be in use, does not have DPS. Punting to a future change.
- **Audit-trail events** (`LockOperationError`, `LockUserChange`): Out of scope per ticket decision.
- **`invalidCodeEntryLimit` time-based clearing**: SBMD v5 has no timer/scheduler primitive. The resource clears on any successful `LockOperation` as an approximation. Time-based clearing (mirroring the Zigbee driver's `UserCodeTemporaryDisableTime` timer) is a future enhancement requiring a scheduler API to be added to the SBMD v5 runtime.
- **`clusterFeatureMaps` C++ TODO**: Affects the door lock PIN path in execute handlers but is unrelated to event handling. Left for a separate fix.

## Capabilities

### New Capabilities

- `matter-door-lock-critical-events`: Critical event handling for the Door Lock SBMD driver — `DoorLockAlarm` → `jammed`/`tampered`/`invalidCodeEntryLimit`; `LockOperation` → `locked` update and state clearing. Mirrors the Zigbee `doorLockCluster.c` alarm/operation semantics.
- `matter-sensor-critical-events`: Critical event handling for Contact Sensor and Water Leak Detector SBMD drivers — `BooleanState.StateChange` → `faulted` resource update. Live state tracking moves to events exclusively; the attribute subscription path is removed.

### Modified Capabilities

<!-- none — the SBMD.md args.event fix is a doc bug correction, not a spec-level behavior change -->

## Impact

- **Affected files**:
  - Drivers: `core/deviceDrivers/matter/sbmd/specs/door-lock.sbmd.js`, `contact-sensor.sbmd.js`, `water-leak-detector.sbmd.js`
  - Docs: `docs/SBMD.md`
  - Test infrastructure (new): `testing/mocks/devices/matterjs/src/ContactSensorDevice.js`, `testing/mocks/devices/matterjs/src/WaterLeakDetectorDevice.js`, `testing/mocks/devices/matter/matter_contact_sensor.py`, `testing/mocks/devices/matter/matter_water_leak_detector.py`, `testing/test/contact_sensor_test.py`, `testing/test/water_leak_detector_test.py`
  - Test infrastructure (modified): `testing/mocks/devices/matterjs/src/DoorLockDevice.js` (alarm side-band), `testing/test/door_lock_test.py` (alarm/operation tests), `testing/conftest.py` (fixture registration)
- **CMake flags**: `BCORE_MATTER` (no new flags required)
- **Consumers**: Any client observing `jammed`, `tampered`, or `invalidCodeEntryLimit` on a Matter door lock endpoint will now receive live updates from Critical events. Previously these resources were never updated.
- **No breaking changes**: New resources (`jammed`, `tampered`, `invalidCodeEntryLimit`) are additive. Attribute-based handlers (`handleLockState`, `handleStateValue`) are removed; live state updates now arrive exclusively via event handlers. Clients observing existing resources see no behavioral change other than the source of updates.
- **Driver versioning**: The door lock bumps its endpoint `profileVersion` from 3 to 4 so `deviceServiceDeviceNeedsReconfiguring` reconfigures already-commissioned devices and registers the new resources. `driverVersion` is bumped to 2 in all three drivers as a content marker, but it does not itself trigger reconfiguration (it is only recorded and logged). The sensors add no new resources and need no reconfiguration.
