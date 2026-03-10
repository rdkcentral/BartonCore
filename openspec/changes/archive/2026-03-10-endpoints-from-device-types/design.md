## Context

MatterDevice currently resolves the Matter endpoint for a given resource mapper by calling `GetEndpointForCluster(clusterId)`, which iterates all endpoints via `DeviceDataCache::GetEndpointIds()` and returns the **first** endpoint that hosts the requested cluster. This works for single-endpoint devices but breaks for multi-endpoint devices where the same cluster appears on multiple endpoints (e.g., an On/Off Light Switch with On/Off on endpoints 1, 2, and 3).

The SBMD spec already declares `matterMeta.deviceTypes` (the Matter device type IDs this driver supports) and `endpoints` (the list of Barton endpoints with their resource mappers). Each SBMD endpoint uses a hardcoded `id: "1"` because there has been no need to distinguish multiple endpoints. The Matter Descriptor cluster on each endpoint exposes a device type list, which is already accessible via `DeviceDataCache::GetDeviceTypes()`.

```
Current flow:
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│  SBMD Spec      │     │  MatterDevice    │     │  DeviceDataCache │
│  resource.mapper│────▶│  GetEndpoint     │────▶│  GetEndpointIds  │
│  .clusterId     │     │  ForCluster()    │     │  EndpointHas     │
│                 │     │  → first match   │     │  ServerCluster() │
└─────────────────┘     └──────────────────┘     └──────────────────┘

Proposed flow:
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│  SBMD Spec      │     │  MatterDevice    │     │  DeviceDataCache │
│  matterMeta     │     │  ResolveEndpoint │────▶│  GetDeviceTypes  │
│  .deviceTypes   │────▶│  Map()           │     │  (per endpoint)  │
│  endpoints[N]   │     │  N-th matching   │     │                  │
│                 │     │  endpoint        │     │                  │
└─────────────────┘     └──────────────────┘     └──────────────────┘
```

**Affected layers**: Matter device drivers (`core/deviceDrivers/matter/`), Matter subsystem (`core/src/subsystems/matter/`).

## Goals / Non-Goals

**Goals:**
- Replace cluster-based endpoint resolution with device-type-based resolution in `MatterDevice`
- Support Nth endpoint selection so multi-endpoint SBMD specs can target specific endpoints
- Build the endpoint mapping once during initialization and reuse it for all resource bindings, attribute callbacks, and event routing

**Non-Goals:**
- Changing the SBMD YAML schema (cluster IDs in mappers remain as-is)
- Modifying non-SBMD device drivers (native C Zigbee drivers, philipsHue)
- Supporting runtime re-composition of endpoint mappings
- Changes to the public GObject API

## Decisions

### 1. Build an endpoint map during MatterDevice initialization

**Decision**: Add a `std::map<int, chip::EndpointId> sbmdEndpointMap` to `MatterDevice` that maps SBMD endpoint index (0-based position in `spec->endpoints`) to the resolved Matter `EndpointId`. This map is populated once in a new `ResolveEndpointMap()` method called from `SpecBasedMatterDeviceDriver::AddDevice()` before resource binding.

**Rationale**: The current approach resolves endpoints lazily during `BindResourceReadInfo()` etc. via `GetEndpointForCluster()`. Building the map eagerly means all binding and callback code can use a simple index lookup instead of querying the cache repeatedly. It also ensures consistent endpoint assignment across all resources in the same SBMD endpoint.

**Alternative considered**: Resolve per-resource at bind time using device types instead of clusters. Rejected because it duplicates work and risks inconsistent mapping if endpoints are iterated in different orders.

### 2. Match endpoints by device type list intersection

**Decision**: `ResolveEndpointMap()` iterates all Matter endpoints via `DeviceDataCache::GetEndpointIds()`, calls `GetDeviceTypes()` on each, and checks whether any device type matches `matterMeta.deviceTypes`. Matching endpoints are collected in order and assigned to SBMD endpoints by position (first match → SBMD endpoint 0, second match → SBMD endpoint 1, etc.).

**Rationale**: This mirrors the semantic already used in `ClaimDevice()` but extends it to collect all matches rather than stopping at the first. The device type on an endpoint is a stable identity defined by the Matter specification, whereas cluster presence is a potentially ambiguous signal (many clusters are shared across device types).

**Alternative considered**: Add an explicit `matterEndpointId` field to SBMD endpoint definitions. Rejected because it couples specs to specific device compositions and is fragile.

### 3. Replace `GetEndpointForCluster()` with `GetEndpointForSbmdIndex()`

**Decision**: Introduce `GetEndpointForSbmdIndex(int sbmdIndex, chip::EndpointId &outEndpointId)` that looks up the pre-resolved map. `GetEndpointForCluster()` is retained but deprecated (it may still be used by non-SBMD code paths like OTA). All SBMD resource binding code passes the SBMD endpoint index instead of letting each mapper independently search by cluster.

**Rationale**: Keeps the change incremental — existing non-SBMD callers continue to work. SBMD paths get the correct behavior.

### 4. Pass SBMD endpoint index through the binding API

**Decision**: Modify `BindResourceReadInfo()`, `BindWriteInfo()`, `BindExecuteInfo()`, and `BindResourceEventInfo()` to require an `int sbmdEndpointIndex` parameter. All callers must pass the SBMD endpoint index; there is no default. Use `GetEndpointForSbmdIndex()` instead of `GetEndpointForCluster()`.

**Rationale**: A required parameter makes the endpoint selection explicit at every call site, preventing accidental use of the old cluster-based path. Breaking changes to existing specs and calling code are acceptable to achieve a cleaner design.

### 5. Thread safety: map is built before subscription starts

**Decision**: `ResolveEndpointMap()` runs during `AddDevice()` before `StartSubscription()`. The map is read-only after construction and accessed from the Matter event loop callbacks. No mutex needed since construction and reads are temporally separated.

**Rationale**: Follows the existing pattern — `resourceReadBindings` and `eventLookup` are also built during `AddDevice()` and read during callbacks without locking.

## Risks / Trade-offs

- **[Risk] No matching device-type endpoint found** → Mitigation: `ResolveEndpointMap()` logs an error and returns false, causing `AddDevice()` to fail the device — same behavior as current cluster-not-found path.
- **[Risk] Fewer matching endpoints than SBMD endpoints defined** → Mitigation: Log a warning for unmapped SBMD endpoints; resources on those endpoints won't be bound but the device still functions for mapped endpoints.
- **[Risk] Endpoint ordering varies across devices** → Mitigation: Matter PartsList order is deterministic per device; SBMD specs are authored with knowledge of expected endpoint composition. This is an inherent property of Matter device composition.
- **[Trade-off] `GetEndpointForCluster()` retained but deprecated** → Adds minor code surface, but ensures non-SBMD callers (OTA, `FindServerEndpoints`) continue working without changes.

## Open Questions

- Should `UpdateCachedFeatureMaps()` (which uses `GetEndpointForCluster()` for `featureClusters`) also switch to the endpoint map, or continue using cluster-based lookup since feature clusters are explicitly about specific clusters?
