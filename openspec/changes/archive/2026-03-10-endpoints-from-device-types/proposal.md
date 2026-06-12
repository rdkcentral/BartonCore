## Why

`MatterDevice::GetEndpointForCluster()` resolves a Barton endpoint's Matter endpoint by searching for the first endpoint that hosts a given cluster ID. This is fragile for multi-endpoint devices: many Matter endpoints can host the same cluster (e.g., On/Off on endpoints 1, 2, and 3 of an On/Off Light Switch), and the current code always returns the first match. There is no way for an SBMD spec to say "use the 2nd endpoint of device type 0x000F" — it just gets whichever endpoint happens to appear first in the PartsList. Matching by device type is the correct semantic: SBMD specs already declare `matterMeta.deviceTypes`, and Matter endpoints already expose their device type list via the Descriptor cluster.

## What Changes

- **Replace cluster-based endpoint lookup with device-type-based lookup**: `GetEndpointForCluster()` will be replaced (or augmented) with a method that finds Matter endpoints whose Descriptor device type list matches the driver's `matterMeta.deviceTypes`, then selects the correct one by index.
- **Support Nth endpoint selection**: SBMD endpoints (currently hardcoded as `id: "1"`) will use their positional index to select the Nth matching Matter endpoint. For example, the first SBMD endpoint maps to the 1st matching Matter endpoint, the second SBMD endpoint maps to the 2nd, etc.
- **Cache device-type-to-endpoint mapping**: Build a resolved mapping of SBMD endpoint → Matter endpoint during device initialization so resource binding, attribute reads/writes, event processing, and feature map extraction all use the correct endpoint without repeated lookups.
- **Update resource binding and event routing**: All call sites that currently use `GetEndpointForCluster()` — resource binding (`BindResourceReadInfo`, `BindResourceWriteInfo`), attribute data callbacks, event processing — will use the pre-resolved endpoint mapping instead.

## Non-goals

- Changing the SBMD YAML schema for resource mappers — cluster IDs in `mapper.read.attribute.clusterId` remain as-is; only the endpoint resolution changes.
- Supporting dynamic re-mapping if a device's endpoint composition changes at runtime.
- Modifying how non-SBMD (native C) device drivers find endpoints.
- Changes to the Zigbee subsystem or IP device drivers.

## Capabilities

### New Capabilities
- `device-type-endpoint-resolution`: Resolve Matter endpoints by matching device type lists from the Descriptor cluster against SBMD `matterMeta.deviceTypes`, with support for Nth endpoint selection.

### Modified Capabilities

## Impact

- **Core drivers layer** (`core/deviceDrivers/matter/`): `MatterDevice.cpp` and `MatterDevice.h` — major changes to endpoint lookup; `SpecBasedMatterDeviceDriver.cpp` — endpoint mapping during `ConfigureDevice`.
- **Matter subsystem** (`core/src/subsystems/matter/`): `DeviceDataCache` — may need `GetDeviceTypes()` usage expanded for batch queries.
- **SBMD specs** (`core/deviceDrivers/matter/sbmd/specs/`): No schema changes required, but multi-endpoint specs (future) will benefit from Nth endpoint support.
- **CMake flags**: `BCORE_MATTER` — changes are scoped to Matter-enabled builds only.
- **Tests**: Unit tests for `MatterDevice`, `SpecBasedMatterDeviceDriver`, and `DeviceDataCache` will need updates. Integration tests for Matter device discovery and resource operations may need verification.
