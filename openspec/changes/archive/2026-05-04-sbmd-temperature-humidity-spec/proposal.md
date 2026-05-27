## Why

The IKEA TIMMERFLOTTE sensor exposes a temperature sensor device type (0x0302) on one endpoint and a humidity sensor device type (0x0307) on another. Rather than adding generic multi-device-type matching logic, we handle this by binding to the device's vendor ID and product ID, creating a driver specific to the TIMMERFLOTTE. This avoids overloading the term "composite" (which has a distinct meaning in the Matter specification) and keeps the claiming model simple: vendor-specific drivers match on identity, generic drivers match on device type.

## What Changes

- Add optional `vendorId` and `productId` fields to `SbmdMatterMeta`. Update `ClaimDevice` so that when these fields are set, the driver claims by matching the BasicInformation cluster's VendorID and ProductID attributes instead of device types. Add `GetVendorId()` and `GetProductId()` accessors to `DeviceDataCache`.
- Add vendor-specific driver priority in `MatterDriverFactory::GetDriver` — drivers with `vendorId`/`productId` are tried first, then generic device-type drivers. This prevents a generic single-type driver from claiming a multi-function device that a vendor-specific driver should handle.
- Add a `ResolveEndpointForCluster` method to `MatterDevice` that tries the SBMD-mapped endpoint first, then falls back to cluster-based lookup when the mapped endpoint doesn't host the required cluster. This is needed for multi-endpoint devices where clusters span different Matter endpoints but are grouped under one SBMD endpoint.
- Create three new SBMD driver specs:
  - `ikea-timmerflotte.sbmd` — TIMMERFLOTTE-specific driver, claims by vendor/product ID, exposes both temperature and humidity resources
  - `temperature-sensor.sbmd` — deviceClass `"temperatureSensor"`, matches device type 0x0302
  - `humidity-sensor.sbmd` — deviceClass `"humiditySensor"`, matches device type 0x0307
- Add corresponding Python test devices and integration tests.

## Non-goals

- Changing existing SBMD drivers or their matching behavior.
- Modifying the public C API or GObject interfaces.
- Supporting vendor/product matching for non-SBMD (native C) drivers.
- Generic multi-device-type AND-matching semantics (`deviceTypeMatch: "all"` is not being added).

## Capabilities

### New Capabilities
- `vendor-product-claiming`: Add optional `vendorId`/`productId` fields to `SbmdMatterMeta`, add `GetVendorId()`/`GetProductId()` accessors to `DeviceDataCache`, and update `ClaimDevice` to support vendor/product ID matching with priority over device-type matching.
- `endpoint-cluster-fallback`: Add `ResolveEndpointForCluster` to `MatterDevice`, enabling cluster-based fallback for multi-endpoint devices where clusters span different Matter endpoints.
- `temperature-humidity-sbmd-drivers`: Create three new SBMD specs (`ikea-timmerflotte.sbmd`, `temperature-sensor.sbmd`, `humidity-sensor.sbmd`) with corresponding test devices and tests.

### Modified Capabilities
- `sbmd-system`: The `SbmdMatterMeta` struct gains optional `vendorId` and `productId` fields, and the SBMD parser must handle them.

## Impact

- **Subsystem** (`core/src/subsystems/matter/`): `DeviceDataCache` gains `GetVendorId()` and `GetProductId()` accessors.
- **Core drivers** (`core/deviceDrivers/matter/`): `MatterDeviceDriver::ClaimDevice`, `MatterDriverFactory::GetDriver`, `SbmdMatterMeta` struct, `SbmdParser::ParseMatterMeta`, `MatterDevice` (endpoint resolution).
- **SBMD specs** (`core/deviceDrivers/matter/sbmd/specs/`): Three new `.sbmd` files added.
- **Tests** (`core/test/`): New C++ unit tests for vendor/product claiming and endpoint fallback. New Python integration test devices and test cases.
- **CMake**: `BCORE_MATTER` flag relevant; new SBMD specs auto-discovered by existing CMake glob.
