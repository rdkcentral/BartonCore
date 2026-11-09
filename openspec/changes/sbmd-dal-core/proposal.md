## Why

Jira: **BARTON-421** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

SBMD drivers read data through ad hoc per-source fetchers in `SpecBasedMatterDeviceDriver` (`MakeAttrFetcher`, `MakeResFetcher`, `MakePersistFetcher`, `MakeTransientFetcher`) and write through per-op executors (`SbmdHandlerInvoker::ExecuteOps`, `MakeTransientSetter`). Supplements are captured once, before the handler runs. When a handler returns `requestCommand`, the operation parks and its deferred callbacks run later with whatever the driver stashed into `context` at park time. Writing that snapshot back (as `camera.sbmd.js` does with its session list) silently discards anything that changed in between (BARTON-411, a lost update). Separately, failures before a command leaves the stack never reach the driver's `onError` (BARTON-412, fixed on its own branch and a prerequisite of this epic).

The epic replaces the ad hoc fetch/write plumbing with a **DAL Transaction** (a caller-agnostic unit of routed reads, staged writes, and commit/abort) owned by an **SBMD Transaction** (the driver-level execution of one request) that is driven by an orchestrator thread. BARTON-423 (`sbmd-transaction-orchestrator`) lands first and introduces the orchestrator on today's data paths behind an internal seam, `SbmdDataAccess`, implemented by `LegacySbmdDataAccess`. Under the orchestrator, a step's supplement fetch, JS run, and commit are separate thread hops, so concurrent SBMD Transactions on one device can interleave before any park (e.g. two camera `createSession` calls both reading `TD_NEXT_SESSION_ID`).

This change delivers the DAL core — plugin registry, plugin and binding interfaces, `DataKey`, `DalTransaction` — plus the atomic primitives that close that interleaving gap (sequence allocation and conditional writes, D11) and `DalSbmdDataAccess`, the DAL-backed implementation of the 423 seam. It holds no data store and no versions; all reads go to source plugins. It also holds the epic-wide architecture design (glossary, decisions D1–D13, threading model, delivery order, open questions).

## What Changes

- **New `barton::sbmd::dal` component** under `core/deviceDrivers/matter/sbmd/dal/`:
  - `DataKey` — namespace (`Attribute`, `Resource`, `Metadata`, `Property`, `Transient`) + optional device ID + path; hashable and ordered.
  - `DataSourcePlugin` — interface implemented by sources: `Scope()` (namespace + optional device ID), `Caps()` (readable / writable / `atomic`), `Affinity()` (executor: `MainLoop` or `MatterThread`), `Bind(DalTransactionId)` returning a `PluginBinding`.
  - `PluginBinding` — per-transaction handle: batched `Read(keys)`, `Commit(writes)` (writable plugins only; `Atomic` plugins check conditional writes atomically), `AllocateSequence(key)` (`Atomic` plugins only), `Release()`.
  - `DalPluginRegistry` — runtime registry confined to the device-service main loop. `Register`/`Unregister` callable from any thread (marshaled to the main loop). Unregistering a plugin aborts every DAL Transaction bound to it with failure type `aborted`. The registerer unregisters; the DAL never guesses plugin lifetimes.
  - `DalTransaction` — lazy per-plugin binding on first touch, batched routed reads, staged writes, read-your-own-staged-writes, commit with prior-value rollback across plugins (D8), abort. Bindings released and staged state cleared when the transaction ends.
  - **Atomic primitives (D11)**: `DalTransaction::AllocateSequence(key)` — immediate atomic increment, never rolled back; `DalTransaction::StageConditional(key, expected, value)` — compare-and-set checked atomically at commit; a mismatch fails the commit with `commitFailed` reason `conditionFailed` and triggers D8 rollback.
  - `DalExecutor` — generic executor hop: the DAL runs a plugin's binding calls on the plugin's affinity executor and waits with a deadline. When the caller already owns the main loop (`g_main_context_is_owner`), DAL calls run inline.
  - `Dal` — façade marshaling each batched request onto the main loop (one hop per step).
- **`DalSbmdDataAccess`**: implements BARTON-423's `SbmdDataAccess` seam on top of `Dal` (batched supplement fetch including sequences, staging of ops including conditional writes, attribute-write overlay, commit/abort, failure mapping). Built and tested here; BARTON-430 switches the orchestrator to it, replacing `LegacySbmdDataAccess`.
- **Tests**: unit tests with fake plugins covering routing, batching, staging, rollback, atomic primitives, abort-on-unregister, affinity hops, deadlines, inline execution, and `DalSbmdDataAccess` mapping.
- **No driver-visible behavior change.** The orchestrator keeps using `LegacySbmdDataAccess` until BARTON-430.

### Non-goals

- A DAL data store, cached values, per-key versions, ingress queues, live-update reconciliation, optimistic validation, or automatic conflict detection/retry (D3, D4).
- Per-device serialization of SBMD Transactions (rejected in favor of D11).
- Concrete plugins and the switch from `LegacySbmdDataAccess` to `DalSbmdDataAccess` — BARTON-430 (`sbmd-dal-source-plugins`).
- `SbmdTransaction`, `SbmdTransactionOrchestrator`, `SbmdWorker`, operation log, the `SbmdDataAccess` seam and `LegacySbmdDataAccess` — BARTON-423 (`sbmd-transaction-orchestrator`).
- JS exposure of sequences and conditional writes (`sequence` supplements, `storage.setTransientDataIf`), fresh supplements, `storage.mergeTransientData` — BARTON-422 (`sbmd-dal-driver-api`).
- Driver-level `onTransactionFailure(args)` and the D12 recovery rule — BARTON-424 (`sbmd-transaction-failure-handler`).
- Driver migration and 411/412 regression tests — BARTON-425 (`sbmd-dal-driver-migration`).
- Two-phase commit across plugins (future work only, if ever needed).
- Changes to non-SBMD drivers (native Zigbee, philipsHue, legacy `MatterDeviceDriver` subclasses).

## Capabilities

### New Capabilities

- `sbmd-data-access-layer`: Plugin-routed, store-less data access for SBMD: `DalPluginRegistry`, `DataSourcePlugin`/`PluginBinding` interfaces, `DataKey`, `DalTransaction` with batched routed reads, staged writes, read-your-own-staged-writes, best-effort commit with prior-value rollback, atomic sequence allocation and conditional writes, abort (including abort on plugin unregister), main-loop confinement, executor affinity, inline execution when the caller owns the main loop, and the `DalSbmdDataAccess` implementation of the orchestrator's data-access seam.

### Modified Capabilities

None. `DalSbmdDataAccess` is not activated in this change, so `sbmd-v4-runtime` requirements are unchanged.

## Impact

- **Affected layers**: core device drivers (new component in `core/deviceDrivers/matter/sbmd/dal/`; `DalSbmdDataAccess` beside BARTON-423's `SbmdDataAccess`), core service (uses the device-service GLib main loop run on the `glibEvt` thread by `startMainLoop` in `core/src/deviceService.c`; no code change there). No subsystem changes. No public API (`api/c/public/`) changes; no GObject signals, properties, or GIR changes.
- **Affected files (new)**: `core/deviceDrivers/matter/sbmd/dal/DataKey.h`, `DataSourcePlugin.h`, `DalTypes.h`, `DalExecutor.h/.cpp`, `DalPluginRegistry.h/.cpp`, `DalTransaction.h/.cpp`, `Dal.h/.cpp`, `DalSbmdDataAccess.h/.cpp`; tests `core/test/src/SbmdDal*Test.cpp`.
- **Affected files (modified)**: the SBMD source list in `core/deviceDrivers/matter/CMakeLists.txt` (or the SBMD sub-list it includes), `core/test/CMakeLists.txt`, `docs/SBMD.md` (architecture section).
- **CMake flags**: The DAL core itself has no Matter SDK dependency (GLib + C++17 only) and is built with the SBMD sources. `BCORE_MATTER` gating applies only to the Matter attribute plugin (BARTON-430), whose `MatterThread` executor depends on the Matter SDK. No new flags.
- **Backward compatibility**: Fully backward compatible. Drivers, SBMD schema, and public API unchanged.
- **Dependencies**: Depends on BARTON-423 (`sbmd-transaction-orchestrator`, for the `SbmdDataAccess` seam), which itself depends on the BARTON-412 fix. Delivery order: 412 fix → 423 → 421 → 430 → 422 → 424 → 425. BARTON-430, 422, 424, 425 depend on this change.
