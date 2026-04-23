## Why

SBMD drivers that rely on Matter events (rather than attribute subscriptions) to update a resource's value have no way to populate that resource's initial state at device commissioning or hub restart. Because the priming subscription's event stream is discarded before `MatterDevice` is constructed, and there is no event-replay equivalent to `RegenerateAttributeReport()`, an event-driven resource remains `null` until the next physical event occurs on the device — which may never happen if the device is already in its steady state.

## What Changes

- **New `seedFrom` mapper type** added to the SBMD spec schema. A resource with `mapper.seedFrom` declares an attribute alias whose cached value is read once at device configure time and again at synchronize time to seed the resource's initial value. It is mutually exclusive with `mapper.read` and requires `mapper.event` to also be present.
- **`SbmdMapper` data model** gains `hasInitialRead`, `initialReadAttribute`, and `initialReadScript` fields.
- **`SbmdParser`** is extended to parse `seedFrom` with the same alias-resolution logic as `read`, with explicit error on missing `script`, missing `event`, use of an event alias instead of an attribute alias, and presence of both `read` and `seedFrom`.
- **`MatterDevice`** gains a `seedFromBindings` map (separate from `readableAttributeLookup`) used only for the one-time initial read; `seedFrom` attributes are never registered as live subscription callbacks.
- **`SpecBasedMatterDeviceDriver`** overrides `DoConfigureDevice()` and `DoSynchronizeDevice()` to call a shared `SeedInitialResourceValues()` helper that reads from the attribute cache and calls `updateResource()` for each resource with a `seedFrom` mapper.
- **`door-lock.sbmd`** is updated: the `locked` resource's `mapper.read` is replaced with `mapper.event` (using `LockOperation`) plus `mapper.seedFrom` (using `LockState`). A `lockOperation` event alias is added to `matterMeta.aliases`.
- **SBMD spec schema** (`sbmd-spec-schema.json`) is updated to define the `seedFromMapper` type and add it to the `mapper` object.
- **Tests** are added and updated: `sbmdParserTest.cpp`, `SbmdScriptTest.cpp`, and `door_lock_test.py`.
- **`docs/SBMD.md`** is updated to document the `seedFrom` mapper.
- **`openspec/specs/sbmd-system/spec.md`** is updated to add the `seedFrom` requirement.

## Capabilities

### New Capabilities

- `sbmd-event-resource-seeding`: Defines the `seedFrom` mapper type — its schema shape, validation rules, runtime semantics, and interaction with `event` mappers. Covers one-time attribute cache reads at configure and synchronize time to seed initial resource values for event-driven resources.

### Modified Capabilities

- `sbmd-system`: Adds the `seedFrom` mapper requirement alongside the existing `read`, `write`, `execute`, and `event` mapper requirements.
- `matterjs-door-lock-device`: Adds a requirement that sideband `lock`/`unlock` operations emit `LockOperation` events (in addition to updating the `LockState` attribute), so that event-driven SBMD resources are updated on sideband-triggered state changes.

## Impact

- **Drivers layer** (`core/deviceDrivers/matter/sbmd/`, `core/deviceDrivers/matter/MatterDevice.*`, `core/deviceDrivers/matter/MatterDeviceDriver.*`): New fields in `SbmdSpec.h`, new parsing in `SbmdParser.cpp`, new binding map in `MatterDevice`, new overrides in `SpecBasedMatterDeviceDriver`.
- **SBMD specs** (`core/deviceDrivers/matter/sbmd/specs/door-lock.sbmd`): Behavioral change — `locked` resource transitions from attribute-subscription-driven to event-driven with attribute seeding. **Existing sideband integration tests will break** unless `DoorLockDevice.js` is updated to emit `LockOperation` events on sideband lock/unlock.
- **Virtual device** (`testing/mocks/devices/matterjs/src/DoorLockDevice.js`): Sideband `lock`/`unlock` ops currently mutate the `LockState` attribute directly without invoking the Matter command path, so no `LockOperation` events are emitted. Must be updated to emit events so that the `locked` resource updates on sideband state changes after this feature lands.
- **Tests** (`core/test/src/sbmdParserTest.cpp`, `core/test/src/SbmdScriptTest.cpp`, `testing/test/door_lock_test.py`): Existing door-lock parser test and sideband integration tests updated; new parser, script, and integration tests added.
- **Documentation** (`docs/SBMD.md`, `openspec/specs/sbmd-system/spec.md`, `openspec/specs/matterjs-door-lock-device/spec.md`): New mapper type documented; `matterjs-door-lock-device` spec updated to require `LockOperation` event emission.
- **CMake flags**: No new flags. Affects `BCORE_MATTER` builds only (guarded by existing Matter build configuration).
- **Non-goals**: Event replay from the `ClusterStateCache` event buffer for missed events during hub downtime is explicitly out of scope. Surfacing `EventHeader` metadata (`mEventNumber`, `mTimestamp`, `mPriorityLevel`) in `sbmdEventArgs` or `updateResource` metadata is out of scope. Multi-instance cluster support is out of scope.
