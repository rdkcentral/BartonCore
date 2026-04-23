## Context

SBMD (Spec-Based Matter Drivers) is a declarative system for mapping Matter device clusters to the Barton resource model using YAML specs and embedded JavaScript scripts. Currently, four mapper types exist: `read` (attribute subscription), `write` (writable attribute), `execute` (Matter commands), and `event` (Matter event subscription).

The `read` mapper works by registering the attribute in `MatterDevice::readableAttributeLookup`, so that when `RegenerateAttributeReport()` replays the cached `ClusterStateCache` at hub startup and `SynchronizeDevice()` time, the attribute value is dispatched through the same callback path that handles live subscription updates. This gives `read`-based resources their initial values automatically.

The `event` mapper registers in `MatterDevice::eventLookup` and receives values only when live events arrive via `OnEventData`. There is no equivalent replay mechanism for events. During the priming subscription (which populates the attribute cache before `MatterDevice::CacheCallback` is installed), events flow through `DeviceDataCache` with a null callback and are discarded. Even though `DeviceDataCache` stores events in `mEventDataCache`, there is no current `ReplayEventCache()` equivalent and replay semantics for events are complex (distinguishing initial state from historical transitions). As a result, event-driven resources are `null` until the first physical event fires.

For resources whose current value is best expressed by an event (e.g., `LockOperation`, which carries richer data than `LockState` alone) but whose current state is durably reflected in an attribute (e.g., `LockState`), a mechanism is needed to bridge the gap: read the attribute once at startup to seed the resource, then rely on events for all subsequent updates.

## Goals / Non-Goals

**Goals:**
- Allow an SBMD resource with `mapper.event` to declare an attribute alias as its initial-value source via `mapper.seedFrom`.
- Seed the resource value at both device configure time (commissioning) and device synchronize time (hub restart / communication restoration), because the attribute cache is populated in both contexts before the relevant driver methods run.
- Reuse the existing `AddAttributeReadMapper`/`MapAttributeRead` script engine interface — `seedFrom` uses the same JS callback shape as `read`, keeping the SBMD author experience consistent.
- Fail loudly at parse time for invalid `seedFrom` combinations.

**Non-Goals:**
- Replaying the event cache to derive initial state from historical events.
- Surfacing `EventHeader` metadata (`mEventNumber`, `mTimestamp`, `mPriorityLevel`) through `seedFrom` or `updateResource` metadata.
- Multi-instance cluster support.
- Any changes to the public C API (`api/c/public/`) or GObject interface.
- `seedFrom` for non-event resources (i.e., applying `seedFrom` without `event` is a parse error by design).

## Decisions

### Decision 1: `seedFrom` fires at commission AND synchronize time

**Chosen:** Override `DevicePersisted()` in `SpecBasedMatterDeviceDriver` to seed at commission time, and override `DoSynchronizeDevice()` to seed at synchronize time, both calling the shared `SeedInitialResourceValues()` helper.

**Why `DevicePersisted` at commission (not `DoConfigureDevice`):**
`DoConfigureDevice()` fires before `registerResources()` and `finalizeNewDevice()` in the commissioning sequence, so `updateResource()` would fail silently because the resource has not yet been written to the database. `DevicePersisted()` is called after the device is fully persisted and `DEVICE_ADDED` has been emitted — at that point resources exist in the database and `updateResource()` succeeds. `DevicePersisted()` is also on the commissioning thread, not the Matter thread; since `GetCachedAttributeData()` must be called from the Matter thread, `DevicePersisted()` schedules the seed synchronously via `RunOnMatterSync()`.

**Why `DoSynchronizeDevice` at synchronize time:**
At synchronize time the device already has resources in the database (they were persisted at commission). `DoSynchronizeDevice()` is called from inside `ConnectAndExecute`, which already runs on the Matter thread, so `GetCachedAttributeData()` is safe to call directly. Skipping `DoSynchronizeDevice` would leave the resource stale after a hub restart until the next physical event.

**Alternative considered for commission:** Fire only at `DoSynchronizeDevice`. Rejected: this creates a window between commission and first event where the resource value is unknown.

**Alternative considered for commission:** Use `DoConfigureDevice`. Rejected: resources do not exist in the database yet at that point; `updateResource()` silently fails.

### Decision 2: `resourceSeedFromBindings` is separate from `readableAttributeLookup`

**Chosen:** Add a distinct `resourceSeedFromBindings` map in `MatterDevice`. `BindResourceSeedFromInfo()` populates only `resourceSeedFromBindings` and never touches `readableAttributeLookup`.

**Why:** Adding the attribute to `readableAttributeLookup` would cause it to receive live subscription callbacks via `OnAttributeChanged`, making the attribute subscription actively overwrite event-derived values after the initial seed. The `seedFrom` attribute is intentionally a one-shot read; ongoing updates come exclusively from the `event` mapper. Keeping the maps separate enforces this invariant at the binding layer.

**Why not reuse `resourceReadBindings`:** `resourceReadBindings` is the backing store for `read` mappers and is used by `RegenerateAttributeReport()` replay. Mixing `seedFrom` entries there would risk unintended replay callbacks on every `SynchronizeDevice()` call through the normal attribute replay path (doubling the seeding), and would conceptually conflate two different mapper types.

### Decision 3: Script interface reuse — `AddAttributeReadMapper`/`MapAttributeRead`

**Chosen:** `seedFrom` scripts are registered and called using the identical `AddAttributeReadMapper(attribute, script)` / `MapAttributeRead(attribute, tlvData)` interface already used by `read` mappers.

**Why:** The input to both is a TLV-encoded attribute value from the cache. The output from both is a string resource value. There is no semantic difference at the script level. Introducing a separate `AddSeedFromMapper`/`MapSeedFromRead` interface would add surface area with no benefit. SBMD authors writing a `seedFrom.script` use exactly the same pattern as a `read.script`.

### Decision 4: Parse-time mutual exclusion — `seedFrom` + `read` is an error

**Chosen:** `SbmdParser` emits a parse error if both `read` and `seedFrom` are present on the same mapper.

**Why:** `read` already provides initial state via `RegenerateAttributeReport()` replay. Having both would be semantically redundant and would create confusion about which path is authoritative. The error is detected during the build-time schema validation step, before any device runtime code runs.

### Decision 5: `seedFrom` without `event` is a parse error

**Chosen:** `SbmdParser` emits a parse error if `seedFrom` is present but `event` is absent.

**Why:** `seedFrom` has no purpose without an event mapper — if a resource's ongoing updates come from attribute subscriptions (`read`), initial state is already handled by `RegenerateAttributeReport()`. `seedFrom` is specifically the bridge for the gap left by the event-only update path. Allowing `seedFrom` without `event` would be silently misleading.

### Decision 6: `seedFrom` requires an attribute alias (not a command alias)

**Chosen:** `SbmdParser` rejects an event-type alias in `seedFrom` and also has no command path (unlike `read`, which accepts `alias` or `command`).

**Why:** `seedFrom` reads from the `ClusterStateCache` using an attribute path. There is no concept of a command in a cache read. Accepting an event alias would be nonsensical (events are not in the attribute cache). The constraint is enforced during alias-resolution in `ParseMapper()`.

### Decision 7: `skippedOptionalResources` must be checked in `SeedInitialResourceValues`

**Chosen:** `SeedInitialResourceValues()` skips any resource URI present in `skippedOptionalResources` before attempting a seed read.

**Why:** Optional resources that are absent from the device are placed in `skippedOptionalResources` during `DoRegisterResources()`. Attempting to read their attribute from the cache would either return null or an unexpected value. This mirrors the existing guard in the `configureResource` lambda in `SpecBasedMatterDeviceDriver`.

## Architecture

```
SBMD YAML spec
    │
    ▼
SbmdParser::ParseMapper()
    ├── reads "seedFrom.alias" (attribute alias only)
    ├── reads "seedFrom.script"
    ├── validates: event present, read absent, alias is attribute type
    └── populates: hasInitialRead, initialReadAttribute, initialReadScript
    │
    ▼
SbmdSpec / SbmdMapper (data model, SbmdSpec.h)
    │
    ▼
SpecBasedMatterDeviceDriver::AddResourceMappers()
    └── script.AddAttributeReadMapper(initialReadAttribute, initialReadScript)
                  │
                  ▼
          MQuickJS / QuickJS runtime
          (same path as mapper.read scripts)

SpecBasedMatterDeviceDriver::configureResource() lambda
    └── device->BindResourceSeedFromInfo(uri, initialReadAttribute, endpointIndex)
                  │
                  ▼
          MatterDevice::resourceSeedFromBindings
          (NOT in readableAttributeLookup — one-shot only)

                  ┌───────────────────────────────────────────┐
                  │  DevicePersisted() [at commission]          │
                  │  DoSynchronizeDevice() [at restart]         │
                  └──────────────┬────────────────────────────-─┘
                                 │ calls
                                 ▼
          SeedInitialResourceValues(deviceId)
              for each resource with hasInitialRead:
                  skip if in skippedOptionalResources
                  MatterDevice::GetCachedAttributeData()
                      └─> DeviceDataCache / ClusterStateCache
                  script->MapAttributeRead(attribute, tlvData)
                      └─> JS script returns string value
                  updateResource(deviceUuid, endpointId, resourceId, value, nullptr)
```

## Thread Safety

`DoSynchronizeDevice()` is called from the Matter event loop thread (the same thread that runs `OnAttributeChanged` and `OnEventData`), so `GetCachedAttributeData()` and `updateResource()` are safe to call directly.

`DevicePersisted()` is called from the commissioning thread, not the Matter thread. Because `GetCachedAttributeData()` requires the Matter thread (it reads from `ClusterStateCache`), `DevicePersisted()` dispatches the seed work synchronously via `RunOnMatterSync()`. The `updateResource()` call also happens inside that block, on the Matter thread.

In both cases the seed execution is fully serial with respect to incoming events on the same thread, eliminating any race between the initial seed and the first arriving `LockOperation` event.

## Backward Compatibility and Public API

No changes to the public C API (`api/c/public/`) are required. `updateResource()` is called with `nullptr` metadata, identical to existing attribute and event update calls. The `seedFrom` mapper type is purely additive — existing SBMD specs with only `read`, `write`, `execute`, or `event` mappers are unaffected. The `door-lock.sbmd` change is a behavioral change for that spec only (locked resource transitions from attribute-subscription-driven to event-driven with seeding).

## Risks / Trade-offs

**Cache miss at seed time → null resource value**
If the device's `LockState` attribute is absent from the cache at seed time (e.g., device responded to the wildcard subscription but omitted this attribute), `GetCachedAttributeData()` returns null and `SeedInitialResourceValues()` should not call `updateResource()` with a null value.
→ Mitigation: Guard the call: only invoke `updateResource()` if `GetCachedAttributeData()` returns non-null TLV data.

**`seedFrom` and live event arrive out of order on replay**
At synchronize time, `SeedInitialResourceValues()` runs after `RegenerateAttributeReport()` (which does not touch `seedFrom` bindings). A live event that arrives concurrently could theoretically write the resource after `SeedInitialResourceValues()` writes it. However, since all writes happen on the Matter thread, execution is serial — no interleaving is possible.
→ No mitigation needed.

**`door-lock.sbmd` behavior change for existing deployments**
After this change, `locked` transitions from being driven by `LockState` attribute subscription to being driven by `LockOperation` events. Attribute subscription updates for `LockState` will no longer reach the `locked` resource at all (since it is removed from `readableAttributeLookup`). If a conforming Matter lock device sends attribute reports but not events, `locked` will only be seeded at configure/sync time and not updated thereafter.
→ Mitigation: This is intentional by design. Conforming Matter door locks send `LockOperation` events on every lock/unlock. The trade-off is documented. If a device is non-conforming, a `read`-only spec (without `event`) remains the correct choice.

**`DoorLockDevice.js` virtual device does not emit `LockOperation` events on sideband operations**
The sideband `lock`/`unlock` operations in `DoorLockDevice.js` directly mutate `agent.doorLock.state.lockState` without going through the Matter command path, so no `LockOperation` events are emitted. This means the existing sideband integration tests (`test_sideband_unlock_triggers_barton_update`, `test_sideband_lock_triggers_barton_update`) will fail after the `locked` resource moves out of `readableAttributeLookup`.
→ Mitigation: `DoorLockDevice.js` must be updated to emit `LockOperation` events on sideband lock/unlock (task 5.5). The Matter spec requires `LockOperation` events on all state changes regardless of source.

**`LockOperation` event script must handle non-state-change operation types**
The `LockOperationType` field in `LockOperationEvent` carries values 0 (Lock), 1 (Unlock), 2 (NonAccessUserEvent), 3 (ForcedUserEvent), and 4 (Unlatch). Only 0 and 1 represent lock state changes. Values 2–4 indicate events that do not change whether the door is locked.
→ Mitigation: The event script must return no `output` key (leaving the resource unchanged) for values 2–4.

**`SeedInitialResourceValues` iterates device-level and endpoint-level resources**
The iteration pattern mirrors `DoRegisterResources()` but must replicate the resource URI construction logic. Any divergence in URI construction between registration and seeding would silently mismatch.
→ Mitigation: Extract URI construction into a shared helper, or reuse the existing resource registration loop's URI directly from the device's registered resource list rather than reconstructing it.

## Open Questions

None. All design decisions have been resolved.
