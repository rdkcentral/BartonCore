## Context

SBMD (Spec-Based Matter Drivers) lets BartonCore support new Matter devices via declarative YAML specs without writing C/C++ code. Each spec lists Matter device type IDs in `matterMeta.deviceTypes`, and the base `MatterDeviceDriver::ClaimDevice` method checks endpoints using OR semantics — if ANY endpoint on the device matches ANY listed device type, the driver claims it.

Some real-world Matter devices are composite: a single node exposes a temperature sensor (device type 0x0302) on one endpoint and a humidity sensor (0x0307) on another. To write a combined `temperatureHumiditySensor` driver that only claims devices with BOTH types, we need AND-match semantics. The single-function `temperatureSensor` and `humiditySensor` drivers use the existing OR semantics.

Composite devices also create an endpoint resolution challenge: when clusters span multiple Matter endpoints but are grouped under one SBMD endpoint, the current direct SBMD-index-to-endpoint mapping may point to an endpoint that doesn't host the required cluster. A fallback mechanism is needed.

## Goals / Non-Goals

**Goals:**
- Support `deviceTypeMatch: "all"` in SBMD specs for AND-match claim semantics
- Enable endpoint resolution fallback for composite devices via `ResolveEndpointForCluster`
- Add three new SBMD driver specs: temperature-humidity-sensor, temperature-sensor, humidity-sensor
- Full backward compatibility — existing specs default to `"any"` (OR) matching

**Non-Goals:**
- Changing native C driver claim logic
- Modifying the public C API or GObject interfaces
- Supporting `deviceTypeMatch` values other than `"any"` and `"all"`
- Adding new device class profiles (reusing existing `sensor` profile where applicable)

## Decisions

### 1. Add `deviceTypeMatch` field to `SbmdMatterMeta`

Add an optional `std::string deviceTypeMatch` field to `SbmdMatterMeta` (default: `"any"`). The SBMD parser reads it from the YAML `matterMeta` section.

**Why a string over an enum**: An `enum class` would be technically superior (compile-time safety, `-Wswitch` coverage, integer comparison in the hot path). However, the existing SBMD convention stores YAML-native types in spec structs — e.g., `modes` is `std::vector<std::string>` converted at the point of use. We follow that convention here for consistency. The parser validates allowed values (`"any"`, `"all"`) at parse time.

**Future improvement**: Consider migrating `deviceTypeMatch` (and `modes`) to enums parsed at load time. This would catch typos at compile time and avoid repeated string comparisons.

### 2. Override `ClaimDevice` in `SpecBasedMatterDeviceDriver`

Rather than modifying the base `MatterDeviceDriver::ClaimDevice`, override it in `SpecBasedMatterDeviceDriver` to implement the `"all"` logic. The base method already handles `"any"` and is used by non-SBMD Matter drivers.

**"all" algorithm**: Collect the set of device type IDs seen across all non-root endpoints on the device. Check that every device type in `matterMeta.deviceTypes` appears in that collected set. This is O(E×D + S) where E = endpoints, D = device types per endpoint, S = supported types.

**Thread safety**: `ClaimDevice` runs on the Matter event loop during discovery. It reads the `DeviceDataCache` which is populated from subscription reports on the same loop — no cross-thread concern.

### 3. Add `ResolveEndpointForCluster` to `MatterDevice`

Add a new method `MatterDevice::ResolveEndpointForCluster` that:
1. If an SBMD endpoint index is provided, resolves it to a Matter endpoint via `GetEndpointForSbmdIndex`
2. Checks if that endpoint hosts the required cluster via `DeviceDataCache::EndpointHasServerCluster`
3. If the mapped endpoint doesn't host the cluster (or if no SBMD index is provided), falls back to `GetEndpointForCluster` (cluster-based scan)

The three existing duplicated if/else patterns in `BindResourceReadInfo` (attribute path and command path) and `BindResourceEventInfo` are replaced with calls to this single method, reducing code duplication.

**Why a single method**: The current code repeats the same "if sbmdIndex, use mapped endpoint; else scan for cluster" pattern three times. A fallback-aware method consolidates this and adds the cluster verification needed for composite devices in one place.

### 4. Temperature/humidity SBMD spec resource mapping

```
temperature-humidity-sensor.sbmd architecture:
┌──────────────────────────────────────────────┐
│ SBMD Spec: temperature-humidity-sensor       │
│ deviceTypes: [0x0302, 0x0307]                │
│ deviceTypeMatch: "all"                       │
│                                              │
│ SBMD Endpoint "1" (profile: sensor)          │
│   ├── temperature (cluster 0x0402, attr 0)   │
│   └── humidity    (cluster 0x0405, attr 0)   │
│                                              │
│ Matter device:                               │
│   EP 1: type 0x0302, cluster 0x0402          │
│   EP 2: type 0x0307, cluster 0x0405          │
│                                              │
│ Resolution:                                  │
│   SBMD EP "1" maps to Matter EP 1 (first     │
│   matching device type 0x0302).              │
│   temperature read → EP 1 has 0x0402 → OK    │
│   humidity read → EP 1 lacks 0x0405 →        │
│     fallback → EP 2 has 0x0405 → OK          │
└──────────────────────────────────────────────┘
```

The single-function specs (`temperature-sensor.sbmd`, `humidity-sensor.sbmd`) each use the default `"any"` matching and a single SBMD endpoint with a straightforward 1:1 mapping.

### 5. Driver claim priority — composite before single

When both the composite and single-function drivers list device type 0x0302, a device with BOTH 0x0302 and 0x0307 could be claimed by any of them. The composite driver's `"all"` check is more restrictive, so it naturally avoids claiming single-function devices. However, without explicit ordering a single-function driver could claim a composite device first.

**Solution**: `MatterDriverFactory::GetDriver` uses two-pass claiming. The first pass iterates only composite drivers (those using `deviceTypeMatch: "all"` / `IsCompositeDriver() == true`); the second pass tries non-composite drivers. Within each pass, iteration order is deterministic (drivers are stored in a `std::map`). This guarantees composite drivers always get priority regardless of filename ordering or registration order.

## Risks / Trade-offs

- **[Risk] Test device coverage**: Integration tests require Matter virtual devices that expose multiple device types on separate endpoints. → Mitigation: Use the existing `matterjs` virtual device framework to create test fixtures.
- **[Trade-off] String vs. enum for `deviceTypeMatch`**: Strings are more readable in YAML but lack compile-time checking. → Accepted: parse-time validation is sufficient, and extensibility outweighs compile-time safety for a YAML-driven system.

## Open Questions

- None blocking.
