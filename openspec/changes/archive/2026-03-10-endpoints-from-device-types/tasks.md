## 1. Endpoint Map in MatterDevice

- [x] 1.1 Add `std::map<int, chip::EndpointId> sbmdEndpointMap` member to `MatterDevice` in `MatterDevice.h`
- [x] 1.2 Implement `ResolveEndpointMap(const std::vector<uint16_t> &driverSupportedDeviceTypes)` in `MatterDevice.cpp`: iterate `DeviceDataCache::GetEndpointIds()`, skip endpoint 0, call `GetDeviceTypes()` on each, collect endpoints whose device type list intersects `driverSupportedDeviceTypes`, assign to map by position
- [x] 1.3 Add `GetEndpointForSbmdIndex(int sbmdIndex, chip::EndpointId &outEndpointId)` method to `MatterDevice`
- [x] 1.4 Unit test `ResolveEndpointMap()`: no data, empty types, null cache, multiple types
- [x] 1.5 Unit test `GetEndpointForSbmdIndex()`: valid index returns correct endpoint, out-of-range index returns false, empty map

## 2. Update Resource Binding to Use Endpoint Map

- [x] 2.1 Add `std::optional<int> sbmdEndpointIndex` parameter (default `std::nullopt`) to `BindResourceReadInfo()`, `BindWriteInfo()`, `BindExecuteInfo()`, and `BindResourceEventInfo()` in `MatterDevice.h`. When provided, use `GetEndpointForSbmdIndex()` for endpoint resolution; when `nullopt` (device-level resources), fall back to `GetEndpointForCluster()`
- [x] 2.2 Update `BindResourceReadInfo()` implementation: resolve endpoint at bind time via endpoint map or cluster lookup depending on whether `sbmdEndpointIndex` is provided
- [x] 2.3 Update `BindWriteInfo()` and `BindExecuteInfo()` to resolve endpoint at bind time and store as `resolvedEndpointId` on the binding (no deferred lookup at write/execute time)
- [x] 2.4 Update `BindResourceEventInfo()` implementation: resolve endpoint at bind time via endpoint map or cluster lookup
- [x] 2.5 Unit test binding methods: verify endpoint-level resources get correct Matter endpoint from map; verify device-level resources (nullopt) fall back to cluster lookup; verify write/execute bindings store pre-resolved endpoint; verify null URI rejection

## 3. Integrate in SpecBasedMatterDeviceDriver

- [x] 3.1 In `SpecBasedMatterDeviceDriver::AddDevice()`, call `ResolveEndpointMap(spec->matterMeta.deviceTypes)` before resource binding; fail `AddDevice()` if `ResolveEndpointMap()` returns false
- [x] 3.2 Update the endpoint resource binding loop in `AddDevice()` to pass the SBMD endpoint positional index to the binding methods; device-level resources pass `std::nullopt`
- [x] 3.3 Unit test `AddDevice()` with a mock `DeviceDataCache` that has multi-endpoint device types: verify correct endpoint index is passed to bindings
- [x] 3.4 Unit test `AddDevice()` failure when no matching device types exist

## 4. GetEndpointForCluster Usage

- [x] 4.1 Update `GetEndpointForCluster()` comment to document its role: used for device-level SBMD resources (which aren't tied to a specific SBMD endpoint) and for non-SBMD code paths (e.g., `UpdateCachedFeatureMaps`)
- [x] 4.2 Verify `UpdateCachedFeatureMaps()` and any non-SBMD callers of `GetEndpointForCluster()` still work correctly
- [x] 4.3 Run existing unit tests to confirm no regressions

## 5. Integration Verification

- [x] 5.1 Build with `BCORE_MATTER=ON` and verify compilation (requires Docker)
- [x] 5.2 Run existing Matter device driver unit tests to confirm backward compatibility
- [x] 5.3 Run integration tests for Matter device discovery and resource operations (requires Docker)
