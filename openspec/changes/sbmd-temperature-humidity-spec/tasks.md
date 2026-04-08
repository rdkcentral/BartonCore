Tasks are organized into two implementation commits. Each commit should build and pass tests independently.

---

## Commit 1: Composite Device Type Matching + Endpoint Cluster Fallback

Infrastructure changes — no new SBMD driver specs yet.

### 1a. Composite Device Type Matching

- [x] 1.1 Add `deviceTypeMatch` field (std::string, default "any") to `SbmdMatterMeta` in `SbmdSpec.h`
- [x] 1.2 Update `SbmdParser::ParseMatterMeta` in `SbmdParser.cpp` to read and validate `deviceTypeMatch` from YAML (accept "any", "all"; reject others)
- [x] 1.3 Add unit tests for `SbmdParser` parsing `deviceTypeMatch` (valid values, omitted field defaults, invalid value rejected)
- [x] 1.4 Override `ClaimDevice` in `SpecBasedMatterDeviceDriver` to implement "all"-match logic: collect device types from all non-root endpoints; if `deviceTypeMatch == "all"`, verify every `matterMeta.deviceTypes` entry is present; otherwise delegate to base class
- [x] 1.5 Add unit tests for `ClaimDevice` with "any" semantics (existing behavior preserved) and "all" semantics (all present → true, subset → false, same endpoint → true)

### 1b. Endpoint Cluster Fallback

- [x] 1.6 Add `ResolveEndpointForCluster` to `MatterDevice` — given an SBMD index and cluster ID, resolve to the mapped endpoint if it hosts the cluster, otherwise scan all endpoints for one that does
- [x] 1.7 Replace the duplicated if/else endpoint resolution blocks in `BindResourceReadInfo` (attribute path), `BindResourceReadInfo` (command path), and `BindResourceEventInfo` with calls to `ResolveEndpointForCluster`
- [x] 1.8 Update `PopulateTestCache` in `MatterDeviceEndpointMapTest.cpp` to accept an optional `endpointServerClusters` map and inject cluster data into the cache
- [x] 1.9 Add unit test `BindReadInfoFallsBackToClusterWhenNotOnMappedEndpoint` — composite device where SBMD index 0 maps to EP 1 (temperature), but binding a humidity cluster attribute falls back to EP 2
- [x] 1.10 Add unit test `BindEventInfoFallsBackToClusterWhenNotOnMappedEndpoint` — same composite setup, verifying event binding falls back when the mapped endpoint doesn't host the event cluster

### 1c. Build and Validate

- [x] 1.11 Build with `./build.sh` and run unit tests with `ctest --output-on-failure --test-dir build`
- [x] 1.12 Commit: `feat: add composite match and endpoint fallback`

---

## Commit 2: Temperature & Humidity Sensor SBMD Drivers + Tests

New driver specs and test coverage, built on the infrastructure from Commit 1.

### 2a. SBMD Specs

- [x] 2.1 Create `temperature-humidity-sensor.sbmd` in `core/deviceDrivers/matter/sbmd/specs/` with deviceClass `"temperatureHumiditySensor"`, `deviceTypeMatch: "all"`, deviceTypes `[0x0302, 0x0307]`, single endpoint with `temperature` and `humidity` resources and mapper scripts
- [x] 2.2 Create `temperature-sensor.sbmd` in `core/deviceDrivers/matter/sbmd/specs/` with deviceClass `"temperatureSensor"`, deviceTypes `[0x0302]`, single endpoint with `temperature` resource and mapper script
- [x] 2.3 Create `humidity-sensor.sbmd` in `core/deviceDrivers/matter/sbmd/specs/` with deviceClass `"humiditySensor"`, deviceTypes `[0x0307]`, single endpoint with `humidity` resource and mapper script
- [x] 2.4 Verify all three specs parse correctly via unit tests (SbmdParser roundtrip)

### 2b. Python Test Devices

- [x] 2.5 Create a composite temperature-humidity virtual Matter test device (Python) exposing EP 1 (device type 0x0302, cluster 0x0402) and EP 2 (device type 0x0307, cluster 0x0405)
- [x] 2.6 Create a standalone temperature virtual Matter test device (Python) exposing one endpoint with device type 0x0302 and cluster 0x0402
- [x] 2.7 Create a standalone humidity virtual Matter test device (Python) exposing one endpoint with device type 0x0307 and cluster 0x0405

### 2c. Integration Tests

- [x] 2.8 Add integration test: commission composite temperature-humidity device, verify claimed by `temperature-humidity-sensor` driver, read both `temperature` and `humidity` resources
- [x] 2.9 Add integration test: commission standalone temperature device, verify claimed by `temperature-sensor` driver, read `temperature` resource
- [x] 2.10 Add integration test: commission standalone humidity device, verify claimed by `humidity-sensor` driver, read `humidity` resource
- [x] 2.11 Add negative test: composite `temperature-humidity-sensor` driver rejects a device with only temperature (no humidity endpoint)
- [x] 2.12 Add negative test: composite `temperature-humidity-sensor` driver rejects a device with only humidity (no temperature endpoint)
- [x] 2.13 Add negative test: standalone `temperature-sensor` driver does not claim a composite device that has both temperature and humidity (composite driver wins via priority)
- [x] 2.14 Add negative test: standalone `humidity-sensor` driver does not claim a composite device that has both temperature and humidity (composite driver wins via priority)

### 2d. Build and Validate

- [x] 2.15 Build with `./build.sh`, run unit tests with `ctest --output-on-failure --test-dir build`, and run integration tests with `./scripts/ci/run_integration_tests.sh`
- [ ] 2.16 Commit: `feat: add temperature and humidity sensor SBMD drivers`
