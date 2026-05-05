## Context

SBMD (Spec-Based Matter Drivers) lets BartonCore support new Matter devices via declarative YAML specs without writing C/C++ code. Each spec lists Matter device type IDs in `matterMeta.deviceTypes`, and the base `MatterDeviceDriver::ClaimDevice` method checks endpoints using OR semantics — if ANY endpoint on the device matches ANY listed device type, the driver claims it.

The IKEA TIMMERFLOTTE sensor is a multi-endpoint device: it exposes a temperature sensor (device type 0x0302) on one endpoint and a humidity sensor (0x0307) on another. Rather than adding generic AND-matching logic (which would overload the Matter specification's meaning of "composite"), we match the TIMMERFLOTTE by vendor ID and product ID. This is a more precise and extensible approach — vendor-specific drivers will be needed for other product-specific behaviors anyway.

Multi-endpoint devices also create an endpoint resolution challenge: when clusters span multiple Matter endpoints but are grouped under one SBMD endpoint, the current direct SBMD-index-to-endpoint mapping may point to an endpoint that doesn't host the required cluster. A fallback mechanism is needed.

## Goals / Non-Goals

**Goals:**
- Support `vendorId`/`productId` in SBMD specs for vendor-specific device claiming
- Add `GetVendorId()`/`GetProductId()` accessors to `DeviceDataCache`
- Give vendor-specific drivers claiming priority over generic device-type drivers
- Enable endpoint resolution fallback for multi-endpoint devices via `ResolveEndpointForCluster`
- Add three new SBMD driver specs: ikea-timmerflotte, temperature-sensor, humidity-sensor
- Full backward compatibility — existing specs lack `vendorId`/`productId` and are unaffected

**Non-Goals:**
- Changing native C driver claim logic
- Modifying the public C API or GObject interfaces
- Generic multi-device-type AND-matching semantics (no `deviceTypeMatch` field)
- Adding new device class profiles (reusing existing `sensor` profile where applicable)

## Decisions

### 1. Add `vendorId` and `productId` fields to `SbmdMatterMeta`

Add two optional fields to `SbmdMatterMeta`: `std::optional<uint16_t> vendorId` and `std::optional<uint16_t> productId`. The SBMD parser reads them from the YAML `matterMeta` section.

**Why `std::optional<uint16_t>`**: These fields are only set for vendor-specific drivers. Using `std::optional` makes the "not set" state explicit and avoids sentinel values. The YAML stores them as integers (e.g., `vendorId: 0x117C`), matching how `deviceTypes` are already stored as hex integers.

**Validation**: The parser validates that if either `vendorId` or `productId` is set, both must be set. A spec with only one is rejected at parse time.

### 2. Add `GetVendorId()` and `GetProductId()` to `DeviceDataCache`

The wildcard subscription already caches all BasicInformation attributes, including VendorID (attr 0x0002) and ProductID (attr 0x0004). We just need convenience accessors.

These use `GetAttributeData()` to read from the `ClusterStateCache` and decode the uint16 TLV value directly, following the same pattern as `GetDeviceTypes()` (which also decodes non-string TLV data from the cache).

**Thread safety**: Same as existing accessors — `ClaimDevice` runs on the Matter event loop, and the cache is populated from subscription reports on the same loop.

### 3. Update `ClaimDevice` for vendor/product matching

Add vendor/product matching to `MatterDeviceDriver::ClaimDevice` (the base class), since this is a general capability not specific to SBMD. When `GetSupportedVendorId()` and `GetSupportedProductId()` return non-zero values, `ClaimDevice` matches against the `DeviceDataCache`'s vendor/product IDs. If both match, the device is claimed. If the driver has no vendor/product IDs set, it falls through to the existing device-type matching logic.

`SpecBasedMatterDeviceDriver` overrides `GetSupportedVendorId()` and `GetSupportedProductId()` to return values from the SBMD spec's `matterMeta`.

### 4. Add `ResolveEndpointForCluster` to `MatterDevice`

Add a new method `MatterDevice::ResolveEndpointForCluster` that:
1. If an SBMD endpoint index is provided, resolves it to a Matter endpoint via `GetEndpointForSbmdIndex`
2. Checks if that endpoint hosts the required cluster via `DeviceDataCache::EndpointHasServerCluster`
3. If the mapped endpoint doesn't host the cluster (or if no SBMD index is provided), falls back to `GetEndpointForCluster` (cluster-based scan)

The three existing duplicated if/else patterns in `BindResourceReadInfo` (attribute path and command path) and `BindResourceEventInfo` are replaced with calls to this single method, reducing code duplication.

**Why a single method**: The current code repeats the same "if sbmdIndex, use mapped endpoint; else scan for cluster" pattern three times. A fallback-aware method consolidates this and adds the cluster verification needed for multi-endpoint devices in one place.

### 5. IKEA TIMMERFLOTTE SBMD spec resource mapping

```
ikea-timmerflotte.sbmd architecture:
┌──────────────────────────────────────────────┐
│ SBMD Spec: ikea-timmerflotte                 │
│ vendorId: 0x117C (IKEA)                      │
│ productId: <TIMMERFLOTTE product ID>         │
│ deviceTypes: [0x0302, 0x0307]                │
│                                              │
│ SBMD Endpoint "1" (profile: sensor)          │
│   ├── temperature (cluster 0x0402, attr 0)   │
│   └── humidity    (cluster 0x0405, attr 0)   │
│                                              │
│ Matter device:                               │
│   EP 1: type 0x0302, cluster 0x0402          │
│   EP 2: type 0x0307, cluster 0x0405          │
│                                              │
│ Claiming: matches on vendorId + productId    │
│                                              │
│ Resolution:                                  │
│   SBMD EP "1" maps to Matter EP 1 (first     │
│   matching device type 0x0302).              │
│   temperature read → EP 1 has 0x0402 → OK    │
│   humidity read → EP 1 lacks 0x0405 →        │
│     fallback → EP 2 has 0x0405 → OK          │
└──────────────────────────────────────────────┘
```

The single-function specs (`temperature-sensor.sbmd`, `humidity-sensor.sbmd`) use device-type matching and a single SBMD endpoint with a straightforward 1:1 mapping.

### 6. Driver claim priority — vendor-specific before generic

When both the TIMMERFLOTTE driver and a generic temperature-sensor driver could claim a device with temperature + humidity endpoints, the vendor-specific driver should win.

**Solution**: `MatterDriverFactory::GetDriver` uses two-pass claiming. The first pass iterates only vendor-specific drivers (those with `vendorId`/`productId` set / `IsVendorSpecificDriver() == true`); the second pass tries generic device-type drivers. Within each pass, iteration order is deterministic (drivers are stored in a `std::map`). This guarantees vendor-specific drivers always get priority regardless of filename ordering or registration order.

## Risks / Trade-offs

- **[Risk] Test device coverage**: Integration tests require Matter virtual devices that expose multiple device types on separate endpoints with specific vendor/product IDs. → Mitigation: Use the existing `matterjs` virtual device framework to create test fixtures with configurable BasicInformation attributes.
- **[Risk] Unknown TIMMERFLOTTE product ID**: We need the actual product ID from a real device or from IKEA's documentation. → Mitigation: Use the test device's product ID for now; update when real hardware is available.
- **[Trade-off] `std::optional` vs. sentinel zero**: Zero is technically a valid vendor ID (though reserved by CSA). Using `std::optional` avoids this edge case at the cost of slightly more verbose code.

## Open Questions

- What is the TIMMERFLOTTE's actual Matter product ID? (Does not block implementation — test devices can use any value.)
