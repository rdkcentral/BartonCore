Tasks are organized into two implementation commits. Each commit should build and pass tests independently.

---

## Commit 1: Vendor/Product ID Claiming + Endpoint Cluster Fallback

Infrastructure changes — no new SBMD driver specs yet.

### 1a. DeviceDataCache Vendor/Product Accessors

- [x] 1.1 Add `GetVendorId(uint16_t &outValue)` and `GetProductId(uint16_t &outValue)` methods to `DeviceDataCache` in `DeviceDataCache.h` and `DeviceDataCache.cpp`, reading BasicInformation cluster attributes 0x0002 and 0x0004 via TLV decoding from the cluster state cache
- [x] 1.2 Add unit tests for `GetVendorId` and `GetProductId` (value present → returns success, cache empty → returns error)

### 1b. Vendor/Product ID Claiming

- [x] 1.3 Add `vendorId` and `productId` fields (`std::optional<uint16_t>`) to `SbmdMatterMeta` in `SbmdSpec.h`
- [x] 1.4 Update `SbmdParser::ParseMatterMeta` in `SbmdParser.cpp` to read and validate `vendorId`/`productId` from YAML (both or neither required; reject if only one is set)
- [x] 1.5 Add unit tests for `SbmdParser` parsing `vendorId`/`productId` (both set, neither set, only one set → error)
- [x] 1.6 Add `GetSupportedVendorId()` and `GetSupportedProductId()` virtual methods to `MatterDeviceDriver` (default return 0), override in `SpecBasedMatterDeviceDriver` to return values from `SbmdMatterMeta`
- [x] 1.7 Add `IsVendorSpecificDriver()` to `MatterDeviceDriver` (returns true when `GetSupportedVendorId() != 0`)
- [x] 1.8 Update `MatterDeviceDriver::ClaimDevice` to check vendor/product ID first: if `GetSupportedVendorId() != 0`, read `DeviceDataCache::GetVendorId`/`GetProductId` and match; otherwise fall through to existing device-type matching
- [x] 1.9 Update `MatterDriverFactory::GetDriver` two-pass claiming: first pass tries vendor-specific drivers (`IsVendorSpecificDriver()`), second pass tries generic device-type drivers
- [x] 1.10 Update SBMD JSON schema (`sbmd-spec-schema.json`) to add optional `vendorId` and `productId` integer fields to `matterMeta`
- [x] 1.11 Add unit tests for `ClaimDevice` with vendor/product matching (match → true, wrong product → false, no vendor set → falls through to device type matching)
- [ ] 1.12 ~~Add unit test for `GetDriver` priority~~ — deferred: `MatterDriverFactory::RegisterDriver` calls external C API `deviceDriverManagerRegisterDriver`, making isolated unit testing impractical; covered by integration tests in Commit 2

### 1c. Endpoint Cluster Fallback

- [x] 1.13 Add `ResolveEndpointForCluster` to `MatterDevice` — given an SBMD index and cluster ID, resolve to the mapped endpoint if it hosts the cluster, otherwise scan all endpoints for one that does
- [x] 1.14 Replace the duplicated if/else endpoint resolution blocks in `BindResourceReadInfo` (attribute path), `BindResourceReadInfo` (command path), and `BindResourceEventInfo` with calls to `ResolveEndpointForCluster`
- [x] 1.15 Update `PopulateTestCache` in `MatterDeviceEndpointMapTest.cpp` to accept an optional `endpointServerClusters` map and inject cluster data into the cache
- [x] 1.16 Add unit test `BindReadInfoFallsBackToClusterWhenNotOnMappedEndpoint` — multi-endpoint device where SBMD index 0 maps to EP 1 (temperature), but binding a humidity cluster attribute falls back to EP 2
- [x] 1.17 Add unit test `BindEventInfoFallsBackToClusterWhenNotOnMappedEndpoint` — same setup, verifying event binding falls back when the mapped endpoint doesn't host the event cluster

### 1d. Remove Composite Matching (cleanup from prior approach)

- [x] 1.18 Remove `deviceTypeMatch` field from `SbmdMatterMeta` in `SbmdSpec.h`
- [x] 1.19 Remove `deviceTypeMatch` parsing from `SbmdParser::ParseMatterMeta` and `sbmd-spec-schema.json`
- [x] 1.20 Remove `IsCompositeDriver()` from `MatterDeviceDriver` and `SpecBasedMatterDeviceDriver`
- [x] 1.21 Remove the ALL-match `ClaimDevice` logic from `MatterDeviceDriver::ClaimDevice`
- [x] 1.22 Update/remove unit tests that tested `deviceTypeMatch`/composite matching, replace with vendor/product tests

### 1e. Build and Validate

- [x] 1.23 Build with `./build.sh` and run unit tests with `ctest --output-on-failure --test-dir build`
- [ ] 1.24 Commit: `feat(sbmd): add vendor/product ID claiming and endpoint fallback`

---

## Commit 2: Temperature & Humidity Sensor SBMD Drivers + Tests

New driver specs and test coverage, built on the infrastructure from Commit 1.

### 2a. SBMD Specs

- [ ] 2.1 Rename `temperature-humidity-sensor.sbmd` to `ikea-timmerflotte.sbmd` in `core/deviceDrivers/matter/sbmd/specs/` — change from `deviceTypeMatch: "all"` to `vendorId`/`productId` (IKEA TIMMERFLOTTE), keep deviceTypes `[0x0302, 0x0307]` for endpoint mapping, single endpoint with `temperature` and `humidity` resources and mapper scripts
- [ ] 2.2 Create `temperature-sensor.sbmd` in `core/deviceDrivers/matter/sbmd/specs/` with deviceClass `"temperatureSensor"`, deviceTypes `[0x0302]`, single endpoint with `temperature` resource and mapper script
- [ ] 2.3 Create `humidity-sensor.sbmd` in `core/deviceDrivers/matter/sbmd/specs/` with deviceClass `"humiditySensor"`, deviceTypes `[0x0307]`, single endpoint with `humidity` resource and mapper script
- [ ] 2.4 Verify all three specs parse correctly via unit tests (SbmdParser roundtrip)

### 2b. Python Test Devices

- [ ] 2.5 Update the temperature-humidity virtual Matter test device to expose IKEA vendor ID and TIMMERFLOTTE product ID in BasicInformation, plus EP 1 (device type 0x0302, cluster 0x0402) and EP 2 (device type 0x0307, cluster 0x0405)
- [ ] 2.6 Create a standalone temperature virtual Matter test device (Python) exposing one endpoint with device type 0x0302 and cluster 0x0402
- [ ] 2.7 Create a standalone humidity virtual Matter test device (Python) exposing one endpoint with device type 0x0307 and cluster 0x0405

### 2c. Integration Tests

- [ ] 2.8 Add integration test: commission TIMMERFLOTTE device, verify claimed by `ikea-timmerflotte` driver (via vendor/product match), read both `temperature` and `humidity` resources
- [ ] 2.9 Add integration test: commission standalone temperature device, verify claimed by `temperature-sensor` driver, read `temperature` resource
- [ ] 2.10 Add integration test: commission standalone humidity device, verify claimed by `humidity-sensor` driver, read `humidity` resource
- [ ] 2.11 Add negative test: `ikea-timmerflotte` driver rejects a device with matching device types but different vendor/product ID
- [ ] 2.12 Add negative test: standalone `temperature-sensor` driver does not claim a TIMMERFLOTTE device (vendor-specific driver wins via priority)
- [ ] 2.13 Add negative test: standalone `humidity-sensor` driver does not claim a TIMMERFLOTTE device (vendor-specific driver wins via priority)

### 2d. Build and Validate

- [ ] 2.14 Build with `./build.sh`, run unit tests with `ctest --output-on-failure --test-dir build`, and run integration tests with `./scripts/ci/run_integration_tests.sh`
- [ ] 2.15 Commit: `feat(sbmd): add IKEA TIMMERFLOTTE, temperature, and humidity sensor drivers`
