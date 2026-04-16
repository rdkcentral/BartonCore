## Why

Some Matter devices expose multiple device types across different endpoints — for example, a temperature sensor (0x0302) on one endpoint and a humidity sensor (0x0307) on another. The current SBMD `ClaimDevice` logic uses OR semantics: a driver claims a device if ANY listed device type matches ANY endpoint. This prevents creating a single driver that requires ALL listed device types to be present (AND semantics). Without this, composite sensor devices cannot be properly matched by a single driver, nor can single-function temperature or humidity sensors be supported individually.

## What Changes

- Add a `deviceTypeMatch` field to `SbmdMatterMeta` (values: `"any"` default, `"all"`). Update `ClaimDevice` in the SBMD driver to support ALL-match logic — when set to `"all"`, the device is only claimed if every listed device type is found across the device's endpoints. Existing drivers are unaffected (they default to `"any"`).
- Add a `ResolveEndpointForCluster` method to `MatterDevice` that tries the SBMD-mapped endpoint first, then falls back to cluster-based lookup when the mapped endpoint doesn't host the required cluster. Update all resource and event binding call sites to use this method. This is needed for composite devices where clusters span multiple Matter endpoints but are grouped under one SBMD endpoint.
- Create three new SBMD driver specs:
  - `temperature-humidity-sensor.sbmd` — deviceClass `"temperatureHumiditySensor"`, `deviceTypeMatch: "all"`, requires both 0x0302 and 0x0307
  - `temperature-sensor.sbmd` — deviceClass `"temperatureSensor"`, matches 0x0302
  - `humidity-sensor.sbmd` — deviceClass `"humiditySensor"`, matches 0x0307
- Add corresponding Python test devices and integration tests.

## Non-goals

- Changing existing SBMD drivers or their matching behavior (all default to `"any"` and are unaffected).
- Modifying the public C API or GObject interfaces.
- Supporting `deviceTypeMatch` for non-SBMD (native C) drivers.

## Capabilities

### New Capabilities
- `composite-device-type-matching`: Add `deviceTypeMatch` field to `SbmdMatterMeta` and update `ClaimDevice` to support `"all"` (AND) matching semantics alongside the existing `"any"` (OR) default.
- `endpoint-cluster-fallback`: Add `ResolveEndpointForCluster` to `MatterDevice`, enabling cluster-based fallback for composite devices where clusters span multiple Matter endpoints.
- `temperature-humidity-sbmd-drivers`: Create three new SBMD specs (`temperature-humidity-sensor.sbmd`, `temperature-sensor.sbmd`, `humidity-sensor.sbmd`) with corresponding test devices and tests.

### Modified Capabilities
- `sbmd-system`: The `SbmdMatterMeta` struct gains a new optional `deviceTypeMatch` field, and the SBMD parser must handle it.

## Impact

- **Core drivers** (`core/deviceDrivers/matter/`): `MatterDeviceDriver::ClaimDevice`, `SbmdMatterMeta` struct, `SbmdParser::ParseMatterMeta`, `MatterDevice` (endpoint resolution).
- **SBMD specs** (`core/deviceDrivers/matter/sbmd/specs/`): Three new `.sbmd` files added.
- **Tests** (`core/test/`): New C++ unit tests for ALL-match claim logic and endpoint fallback. New Python integration test devices and test cases.
- **CMake**: `BCORE_MATTER` flag relevant; new SBMD specs auto-discovered by existing CMake glob.
