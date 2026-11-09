## Why

Jira: **BARTON-426** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

By the time this change lands (delivery order 412 fix → 423 → 421 → 426 → 422 → 424 → 425), BARTON-423 (`sbmd-transaction-orchestrator`) runs every SBMD Transaction through an orchestrator that performs all data access via the `SbmdDataAccess` seam, implemented by `LegacySbmdDataAccess` over today's fetchers/setters (`MakeAttrFetcher`, `MakeResFetcher`, `MakePersistFetcher`, `MakeTransientFetcher`, `MakeTransientSetter`, `SbmdHandlerInvoker::ExecuteOps`) and `SpecBasedMatterDeviceDriver::transientStore`. BARTON-421 (`sbmd-dal-core`) delivers the store-less DAL, the D11 atomic primitives, and `DalSbmdDataAccess`, but registers no plugins.

This change supplies the concrete plugins, switches the orchestrator from `LegacySbmdDataAccess` to `DalSbmdDataAccess`, and moves transient data — the state lost in BARTON-411 — into a DAL-native, main-loop-confined plugin that supports sequence allocation and conditional writes. Because the orchestrator already schedules all data access, no Matter-thread code touches transient data after the switch and no locking is needed.

## What Changes

- **Device-service properties plugin** (`Property` namespace, service lifetime, `MainLoop` affinity, readable + writable) over `deviceServiceGetSystemProperty` / `deviceServiceSetSystemProperty`.
- **Device resources + metadata plugin** (`Resource` and `Metadata` namespaces, service lifetime, `MainLoop` affinity):
  - `Resource`: reads via `deviceServiceGetResourceById` (freeing the result with `resourceDestroy`), writes via `updateResource`.
  - `Metadata`: reads/writes via `deviceServiceGetMetadata` / `deviceServiceSetMetadata` on `/devices/<uuid>/metadata/<path>`; SBMD persistent data is the `sbmd.`-prefixed subset, as today.
- **Transient-data plugin** (`Transient` namespace, service lifetime, `MainLoop` affinity, readable + writable + `Atomic`, DAL-native, main-loop-confined with no mutex): owns per-device transient values with per-key TTL, preserving today's semantics (absolute expiry from write time on `steady_clock`, lazy eviction on read, last write wins, no persistence). Implements the D11 `Atomic` capability: sequence allocation (atomic increment of a decimal counter, immediate, never rolled back) and conditional writes (all conditions in a batch checked before any write is applied). D8 rollback restores the prior value with its remaining TTL. Adds per-device purge on device removal.
- **Per-device Matter attribute plugin** (`Attribute` namespace scoped to one device ID, `MatterThread` affinity, read-only): reads `MatterDevice::GetCachedAttributeData` → `DeviceDataCache::GetAttributeData` and returns base64 TLV, identical to today's attribute supplements. Registered by `MatterDeviceDriver::AddDevice` after the device is inserted; unregistered by `MatterDeviceDriver::DeviceRemoved` and `MatterDeviceDriver::Shutdown` before the `MatterDevice` is destroyed. Read-your-own-writes: after a successful `writeAttribute`, the orchestrator records the written value through the seam's overlay hook (`DalTransaction::SetOverlay`), and later reads in the same DAL Transaction return it.
- **Registration ownership**: each plugin's registerer unregisters it. Service-lifetime plugins are registered/unregistered with SBMD driver startup/shutdown via a `SbmdDalPlugins` helper; per-device attribute plugins by the Matter driver. The DAL never infers lifetimes.
- **Matter-thread execution**: `MatterThread`-affinity reads use the DAL's `DalExecutor`, an adapter over BARTON-423's shared `ThreadHop` (`DefaultThreadHop::OnMatter` → `PlatformMgr().ScheduleWork`); no separate executor hooks are installed.
- **Switch-over**: the orchestrator is configured with `DalSbmdDataAccess` instead of `LegacySbmdDataAccess`. Deletes `SpecBasedMatterDeviceDriver::transientStore`, `TransientEntry`, `SetTransientData`, `GetTransientData`, `MakeTransientFetcher`, `MakeTransientSetter`, and `LegacySbmdDataAccess` in its entirety (no fallback). The shared `SbmdDataAccess` contract test suite owned by BARTON-423 is run against `DalSbmdDataAccess` with the real plugins; its `LegacySbmdDataAccess` target is removed with the implementation.

### Non-goals

- The DAL core, atomic primitives, and `DalSbmdDataAccess` — BARTON-421 (`sbmd-dal-core`).
- `SbmdTransaction`, orchestrator, operation log, the `SbmdDataAccess` seam — BARTON-423.
- JS exposure of sequences (`sequence` supplements) and conditional writes (`storage.setTransientDataIf`), `storage.mergeTransientData`, fresh supplements per step — BARTON-422.
- Attribute writes through the DAL. Attribute writes are device operations tracked by the SBMD Transaction (D10); the attribute plugin is read-only.
- `Atomic` support in namespaces other than `Transient`.
- Persisting transient data across restarts; changing TTL semantics.
- Plugins for non-SBMD drivers or Zigbee.

## Capabilities

### New Capabilities

- `sbmd-dal-source-plugins`: Concrete DAL plugins for device-service properties, device resources and metadata, transient data (DAL-native, main-loop-confined replacement of `transientStore` with TTL and the `Atomic` capability), and per-device read-only Matter attributes with a per-transaction read-your-own-writes overlay; explicit registerer-owned lifetimes; orchestrator switched to `DalSbmdDataAccess`.

### Modified Capabilities

None at the driver-visible level. Supplement values and op effects are unchanged; transient data moves from the Matter thread to the main loop. Atomic operations become available to the runtime but are not exposed to JS until BARTON-422.

## Impact

- **Affected layers**: core device drivers (`core/deviceDrivers/matter/sbmd/dal/plugins/`, `SpecBasedMatterDeviceDriver`, `MatterDeviceDriver`, orchestrator data-access configuration), core service APIs consumed read/write as-is (`api/c/public/private/deviceService.h`, `deviceServicePrivate.h`), Matter subsystem (`DeviceDataCache` read path, unchanged). No public API, GObject, or GIR changes.
- **Affected files (new)**: `core/deviceDrivers/matter/sbmd/dal/plugins/PropertyPlugin.h/.cpp`, `ResourceMetadataPlugin.h/.cpp`, `TransientPlugin.h/.cpp`, `MatterAttributePlugin.h/.cpp`, `SbmdDalPlugins.h/.cpp`; tests `core/test/src/SbmdDal*PluginTest.cpp`.
- **Affected files (modified)**: `core/deviceDrivers/matter/MatterDeviceDriver.{h,cpp}` (register/unregister attribute plugin in `AddDevice`, `DeviceRemoved`, `Shutdown`), `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.{h,cpp}` (delete `transientStore` and legacy transient accessors; select `DalSbmdDataAccess`), `LegacySbmdDataAccess.{h,cpp}` and its tests (deleted), CMake source lists, `docs/SBMD.md`.
- **CMake flags**: `BCORE_MATTER` gates the Matter attribute plugin. The properties, resources/metadata, and transient plugins depend only on core service APIs and are built with the SBMD sources (which themselves require `BCORE_MATTER`). No new flags.
- **Backward compatibility**: No driver-visible change. Transient data remains in-memory with identical TTL behavior.
- **Dependencies**: Depends on BARTON-423 (`sbmd-transaction-orchestrator`, the `SbmdDataAccess` seam and `LegacySbmdDataAccess`) and BARTON-421 (`sbmd-dal-core`, DAL core, D11, `DalSbmdDataAccess`). Required by BARTON-422 (`sbmd-dal-driver-api`), 424, and 425. Delivery order: 412 fix → 423 → 421 → 426 → 422 → 424 → 425.
