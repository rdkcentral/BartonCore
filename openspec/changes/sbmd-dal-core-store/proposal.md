## Why

Jira: **BARTON-421** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

SBMD drivers currently read data through a patchwork of per-source fetchers built ad hoc in `SpecBasedMatterDeviceDriver` (`MakeAttrFetcher`, `MakeResFetcher`, `MakePersistFetcher`, `MakeTransientFetcher`) and write through per-op executors (`SbmdHandlerInvoker::ExecuteOps`, `MakeTransientSetter`, `SetTransientData`). None of these sources carries a version, and supplements are captured once, before the handler runs. When a handler returns `requestCommand`, the operation parks in `pendingOperations` and its callbacks run seconds later with whatever the driver stashed into `context` at park time. Writing that snapshot back (as `camera.sbmd.js` `handleAllocateForSolicit` does with `TD_SESSIONS`) silently discards anything that changed in between — a concurrently created session is erased or a destroyed one is resurrected (BARTON-411).

This is not a data race (all of this state is Matter-thread-confined, see the `transientStore` invariant in `SpecBasedMatterDeviceDriver.h`); it is a lost update caused by a read-modify-write spanning a park. Fixing it per-driver is impossible today, and fixing it per-call-site would scatter concurrency logic into every driver.

This change introduces the foundation of a C++ **SBMD Data Access Layer (DAL)**: an authoritative, versioned, in-memory view of every piece of data an SBMD driver can read or write, with live-update reconciliation so the view stays current while operations are parked. Later changes in the epic (BARTON-422..425) build the driver-facing API, optimistic transactions, cleanup hooks, and driver migration on top of it. This change also holds the full architecture design for the epic.

## What Changes

- **New `barton::sbmd::dal` component** under `core/deviceDrivers/matter/sbmd/dal/`:
  - `DataKey` — a tagged, hashable key covering four namespaces: Matter attribute `(deviceId, endpoint, cluster, attribute)`, Barton resource `(deviceId, endpoint, resourceId)`, SBMD persistent data `(deviceId, key)`, and SBMD transient data `(deviceId, key)`.
  - `VersionedValue` — an immutable, shared (`std::shared_ptr<const ...>`) value handle plus a per-key monotonically increasing `uint64_t` version and optional expiry (for transient TTL).
  - `DataStore` — hash-indexed (`std::unordered_map`) O(1) store of `DataKey → Entry`, Matter-thread-confined, with no locking on the hot path. Reads return a shared handle (no copy); writes swap the handle and bump the version.
  - `DataSource` adapters — read-through/write-through bridges to the existing backends: `MatterDevice::GetCachedAttributeData` (attribute cache), `deviceServiceGetResourceById` / `updateResource` (resources), `deviceServiceGetMetadata` / `deviceServiceSetMetadata` with the `sbmd.` prefix (persistent data), and the DAL itself as the system of record for transient data (replacing `SpecBasedMatterDeviceDriver::transientStore`).
  - `IngressQueue` — the single cross-thread entry point: a mutex-protected queue drained on the Matter thread via `chip::DeviceLayer::PlatformMgr().ScheduleWork`, used by writers that are not on the Matter thread.
  - `DataAccessLayer` — façade owning the store, sources, and ingress, with `Read`, `Write`, `Invalidate`, and `Reconcile` entry points plus a `ChangeObserver` hook used by later changes.
- **Live-update reconciliation**: attribute reports (`SpecBasedMatterDeviceDriver::HandleAttributeReport` / `MatterDevice` cache callback), resource writes from other subsystems and the public API (`updateResource`, `deviceServiceWriteResource`), metadata writes, and SBMD transient writes all flow into the DAL, which updates the view and bumps the key's version.
- **Internal re-plumbing only**: the four existing `Make*Fetcher` factories and `MakeTransientSetter` are re-implemented as thin wrappers over the DAL so existing drivers behave identically. No driver-visible behavior change in this story.
- **Metrics**: DAL counters (reads, writes, reconciliations, version bumps, ingress queue depth) via the existing `SpecBasedMatterDeviceDriverMetrics` / observability plumbing.

### Non-goals

- Driver-facing API changes (fresh reads in deferred callbacks, per-member/merge transient writes) — BARTON-422 (`sbmd-dal-driver-api`).
- Optimistic transactions, read/write-set tracking, commit validation, conflict merge — BARTON-423 (`sbmd-dal-optimistic-transactions`).
- Driver cleanup/compensation hooks — BARTON-424 (`sbmd-dal-cleanup-hooks`).
- Migrating `camera.sbmd.js` and other drivers; BARTON-411/412 reproduction tests — BARTON-425 (`sbmd-dal-driver-migration`).
- Exactly-once `onError` on pre-send failures (`FailOperation` sites, `CompletePendingOperation(pendingId, false)`) — BARTON-412, fixed separately. It is a **prerequisite** of BARTON-423/424, not part of this epic.
- Blocking locks around driver operations. Rejected by design (see design D2).
- Persisting transient data across restarts.
- Any change to non-SBMD (native C Zigbee, philipsHue, legacy `MatterDeviceDriver` subclasses) data paths beyond the shared reconciliation hook in `updateResource`.

## Capabilities

### New Capabilities

- `sbmd-data-access-layer`: Versioned, authoritative in-memory data view for SBMD drivers covering Matter attributes, Barton resources, SBMD persistent data, and SBMD transient data; O(1) hash-indexed lookups; per-key monotonic versions; shared copy-on-write value handles; Matter-thread confinement with a single mutex-guarded cross-thread ingress; live-update reconciliation from attribute reports, resource/metadata writes, and public API writes.

### Modified Capabilities

- `sbmd-v4-runtime`: The "Supplements pre-loading" requirement is restated so supplement values are served by the DAL view (same values, same shape). No behavior change visible to drivers.

## Impact

- **Affected layers**: core device drivers (SBMD runtime in `core/deviceDrivers/matter/sbmd/`), core service (`core/src/deviceService.c` — reconciliation notification from `updateResource`, `deviceServiceWriteResource`, `deviceServiceSetMetadata`), Matter subsystem glue (`core/deviceDrivers/matter/MatterDevice.cpp` attribute cache callback). No public API (`api/c/public/`) changes; no GObject/GIR changes.
- **Affected files (new)**: `core/deviceDrivers/matter/sbmd/dal/DataKey.h`, `VersionedValue.h`, `DataStore.h/.cpp`, `DataSource.h`, `AttributeCacheSource.h/.cpp`, `ResourceSource.h/.cpp`, `PersistentDataSource.h/.cpp`, `IngressQueue.h/.cpp`, `DataAccessLayer.h/.cpp`; tests under `core/test/src/SbmdDal*Test.cpp`.
- **Affected files (modified)**: `SpecBasedMatterDeviceDriver.h/.cpp` (fetchers/setter delegate to the DAL; `transientStore` and `TransientEntry` removed), `core/deviceDrivers/matter/sbmd/CMakeLists.txt` or the enclosing SBMD source list, `core/src/deviceService.c` (change-notification hook), `docs/SBMD.md` (architecture section 2), `core/test/CMakeLists.txt`.
- **CMake flags**: `BCORE_MATTER` (the DAL is compiled only when Matter/SBMD is enabled). No new flags.
- **Backward compatibility**: Fully backward compatible. Existing SBMD drivers, the SBMD v5 schema (`core/deviceDrivers/matter/sbmd/schema/sbmd-spec-schema.json`), and the public API are unchanged.
- **Dependencies**: None upstream within the epic. BARTON-422, BARTON-423, BARTON-424, BARTON-425 depend on this change.
