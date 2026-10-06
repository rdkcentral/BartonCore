## Context

The SBMD subsystem loads every `.sbmd.js` spec at startup. Today `SbmdFactory::RegisterDriversFromDirectory` reads each file, parses it into an `SbmdRegistration`, wraps it in an `SbmdDriver`, calls `Activate()` immediately, wraps that in a `SpecBasedMatterDeviceDriver`, and registers it with `MatterDriverFactory` and the `deviceDriverManager`. Every driver therefore holds, for the process lifetime: the parsed registration (endpoints/resources/aliases/handlers), the retained spec source text (the dominant cost, ~10–50 KB per spec), rooted handler JSValues in the shared mquickjs heap, and built dispatch tables.

The `SbmdDriver` class already implements an `Activate()` / `Deactivate()` / `IsActivated()` lifecycle:
- **Load** produces metadata + registration without rooting handler JSValues.
- **Activate** re-evaluates the source (from an in-memory copy), roots handler JSValues, and builds dispatch tables.
- **Deactivate** releases the rooted JSValues and clears dispatch tables.

The existing specs (`sbmd-v4-runtime`, `sbmd-system`) already state that claiming must not require activation, that a matching driver is activated on claim, and that a driver is deactivated when its last device is removed. **The implementation never conformed** — `Activate()` is only ever called eagerly at startup, and `Deactivate()` is only ever called from unit tests. This change makes the implementation conform and extends the idle footprint reclamation.

**Constraints:**
- `SbmdDriver::Activate()` / `Deactivate()` require the caller to hold `MQuickJsRuntime::GetMutex()`.
- Claiming (`MatterDriverFactory::GetDriver` → `ClaimDevice`) runs on the Matter thread and must use only C++ metadata.
- `SpecBasedMatterDeviceDriver::AddDevice` is the single funnel reached by both fresh commissioning (`CommissioningOrchestrator`) and post-restart re-synchronization (`ConfigureDevice`/`SynchronizeDevice` → `AddDeviceIfRequired`).
- Per-driver device tracking already exists: `MatterDeviceDriver::devices` (map keyed by UUID) guarded by `devicesMutex`.

```
                         ┌─────────────────────────────────────────┐
                         │              Matter subsystem            │
                         └─────────────────────────────────────────┘
   startup                         claim (metadata only)         device bind / unbind
      │                                    │                             │
      ▼                                    ▼                             ▼
┌───────────────┐   register    ┌──────────────────────┐   Add/Remove  ┌──────────────────────┐
│  SbmdFactory  │──────────────▶│  MatterDriverFactory  │──────────────▶│ SpecBasedMatterDevice │
│  (load only)  │   inactive    │  GetDriver/ClaimDevice │              │ Driver (activate hook)│
└───────────────┘               └──────────────────────┘               └──────────┬───────────┘
      │ load, extract stub,                                                        │ Activate()/Deactivate()
      │ release source+registration                                               ▼   (under JS mutex)
      ▼                                                                     ┌──────────────┐
  claim stub resident ◀──────────────── Deactivate (last device) ──────────│  SbmdDriver  │
  (name, deviceTypes,                                                       │ active⇄stub  │
   vendor/product, class, path)  ─────── Activate (first device) ─────────▶ └──────────────┘
                                          reads spec file from disk
```

## Goals / Non-Goals

**Goals:**
- Unused (unclaimed) SBMD specs hold only a small claim stub at steady state — no source text, no heavy registration, no rooted JSValues, no dispatch tables.
- Drivers activate on first device bind and deactivate when their last device is removed, matching the existing spec.
- Preserve all existing behavior: claiming/priority, endpoint resolution, resource binding, read/write/execute, dispatch, deferred operations, prerequisites.
- Add observability for activate/deactivate and the active-driver count.

**Non-Goals:**
- Caching compiled bytecode or parse results to speed re-activation (explicit follow-up).
- Deferring creation of the `SpecBasedMatterDeviceDriver` / `DeviceDriver` C struct / driver-manager entry until claim ("Tier 3" fully-lazy instantiation).
- Any change to non-SBMD drivers, non-Matter subsystems, the public GObject API, or the persistence format.

## Decisions

### D1 — Activation hook: top of `SpecBasedMatterDeviceDriver::AddDevice`
`AddDevice` is the single point that both commissioning and restart re-sync pass through, and it already needs an active driver (it reads `GetCommandDispatch()`/`GetRegistration()`). Activate at the very top of the override, before any registration access, under the JS mutex; make it idempotent so repeated binds are no-ops.

*Alternatives considered:* Activating inside `ClaimDevice` — rejected: claiming must stay metadata-only per spec, and a claim does not guarantee a bind. Activating in `ConfigureDevice`/`SynchronizeDevice` individually — rejected: two entry points instead of one funnel, easy to miss a path.

### D2 — Deactivation hook: last-device-removed, driven by per-driver count
Deactivate when the driver's `devices` map becomes empty after removal. Rather than duplicate `MatterDeviceDriver::DeviceRemoved`, add a protected virtual hook (e.g. `OnLastDeviceRemoved()`) that the base invokes after erasing the device under `devicesMutex` when the map is empty; `SpecBasedMatterDeviceDriver` overrides it to deactivate under the JS mutex. Base default is a no-op, so non-SBMD drivers are unaffected.

*Alternatives considered:* Overriding `DeviceRemoved` entirely in the SBMD driver — rejected: duplicates fabric-removal/cleanup logic and risks drift. Reference-counting separate from the `devices` map — rejected: redundant with existing per-driver tracking.

### D3 — Reduce `SbmdDriver`'s registration to a claim stub when inactive
`SbmdDriver` keeps its `SbmdRegistration` object for its whole lifetime, but while inactive the registration is reduced to its claim stub: only the fields used for claiming remain populated — `name`, `filePath`, `barton.deviceClass`, and `matter.deviceTypes`/`vendorId`/`productId`. Deactivation (and the load-time `Shrink()`) release the `source` string, the heavy parsed collections (endpoints, aliases, handler vectors), the dispatch tables, and every non-claim metadata field (`schemaVersion`, `driverVersion`, `deviceClassVersion`, `matter.revision`, `featureClusters`, `defaultTimeoutMs`, `reporting`) — swap-with-empty releases the vector capacity. The claim-time metadata accessors (`GetSupportedDeviceTypes`, `GetSupportedVendorId`, `GetSupportedProductId`, `IsVendorSpecificDriver`, device class) therefore read straight from the resident registration, which is safe in any state. `Activate()` reads the spec file from `filePath` on disk, parses, and rebuilds the full registration.

*Alternatives considered:* A separate `SbmdClaimStub` struct that backs the accessors — rejected: keeping the reduced `SbmdRegistration` avoids re-pointing every accessor and a parallel type, and reduces to the same resident footprint. Keeping the full registration but only dropping JSValues ("Tier 1") — rejected: leaves the dominant source-text and registration cost resident, failing the goal. Keeping `source` in memory and re-parsing from it — rejected: `source` is the biggest single cost we want to shed.

### D4 — Disk re-read on activation, with claim-identity validation
`SbmdDriver::Activate()` obtains the spec content by re-reading `filePath` from disk whenever the retained `source` has been released (which it always has in the production flow, since the factory shrinks every driver right after registration). A not-yet-shrunk driver still holding its `source` re-evaluates that copy instead — this keeps unit tests that load from an in-memory string working. Load-time records `filePath`. Guard file-read failure: log and fail activation so the bind fails cleanly rather than dispatching against a half-built driver.

After re-parsing, validate the re-read spec against the claim stub before going active: compare the claim-identity fields (name, `matter.deviceTypes`, `vendorId`, `productId`, device class). On any mismatch, fail activation, log, and abort the bind. This is scoped to the fields that drove the claim decision — not a deep structural diff — so cosmetic edits to handlers do not falsely reject, but a spec that no longer matches what claimed the device can never dispatch against it.

Separately, `SpecBasedMatterDeviceDriver` caches the device-class version and endpoint profile versions at construction (published to the device-service layer for commissioning/reconfiguration). After a bind-time activation it verifies the re-read spec still carries those same versions and rejects the bind (rolling the activation back) on any change, so the device-service layer never publishes/compares a version that disagrees with the handlers now running.

*Alternatives considered:* Trust the file without validation — rejected: a silently-diverged spec could bind handlers inconsistent with the claim. Compare a whole-file hash captured at load — rejected: rejects benign whitespace/comment edits and couples activation to byte-exactness rather than the claim contract.

### D5 — Thread safety
Activation/deactivation happen on the Matter thread and take `MQuickJsRuntime::GetMutex()` around the `Activate()`/`Deactivate()` call, exactly as the current startup path does. The activation state transition itself is serialized by the device-lifecycle path, but to be safe the stub↔registration swap is performed only while the JS mutex is held, and `IsActivated()` checks remain the guard for dispatch entry points (`HandleAttributeReport`, `HandleEvent`, `HandleCommand`, resource ops), which already exist. No new lock ordering is introduced: the JS mutex is a leaf lock and is never held while acquiring `devicesMutex`.

### D6 — Observability
Add metrics, gated by `BARTON_CONFIG_SBMD_METRICS`, following the existing observability helpers:
- Attributed activate/deactivate event counters (`ObservabilityCounter` with a driver-stem attribute) for churn and per-driver attribution.
- A current active-driver count as a synchronous `ObservabilityGauge` **set to the live count on each activate/deactivate transition**, mirroring the existing `registeredDriversGauge` / `RecordRegisteredDriverCount` pattern. The factory also publishes this gauge once after startup registration (counting `IsActivated()` drivers — normally zero), so the gauge reflects true startup state rather than being absent until the first bind.
- A per-activation duration histogram (`sbmd.driver.activation.duration_ms`) recorded around the on-demand `Activate()` call, separate from the startup `sbmd.driver.load.duration_ms` (now load-and-register only).

The active-count gauge and event counters live in the SBMD driver metrics owned by the activation path (`SpecBasedMatterDeviceDriver`); the process-wide active count is maintained as the drivers transition.

*Alternatives considered:* Deriving the current active count by subtracting two monotonic counters (activations − deactivations) — rejected: harder to dashboard, can drift, and the codebase already has a set-on-change gauge idiom.

## Risks / Trade-offs

- **[Re-parse cost on every activation]** → Activation now does a disk read + full parse instead of reusing an in-memory copy. This occurs once per driver until its device count returns to zero, on an already-async commissioning/sync path; acceptable. Bytecode caching is the documented follow-up if this ever matters.
- **[Spec file changed or removed on disk after startup]** → Activation reads the live file, which could differ from what was loaded at startup. Mitigation (decided, see D4): treat read/parse failure as an activation failure, and validate the re-read spec's claim-identity fields against the stub, rejecting on mismatch (bind fails, logged).
- **[A dispatch or resource path that assumes always-active]** → Any code reading the heavy `GetRegistration()` collections (or `reporting`) while inactive would be unsafe. Mitigation: audit all `GetRegistration()`/dispatch callers; the dispatch entry points already gate on `IsActivated()`, and `GetDesiredSubscriptionIntervalSecs()` falls back to the base default when inactive rather than reading the zeroed `reporting`. The claim-time metadata accessors read only the claim fields, which remain resident on the reduced registration in every state.
- **[Re-entering the Matter loop during last-device teardown]** → `OnLastDeviceRemoved()` runs inside `DeviceRemoved`'s `RunOnMatterSync` block, so anything it calls that re-enters the Matter loop (nested `RunOnMatterSync`/`ConnectAndExecute`) would self-deadlock. Mitigation: the hook is documented as Matter-loop-reentrant-forbidden, and `CancelAllPendingOperations()` only settles promises and releases JS roots (no Matter-loop work).
- **[Concurrent bind/remove racing activation state]** → Mitigation: perform the state swap under the JS mutex and rely on the existing serialization of device-lifecycle operations; keep `Activate()`/`Deactivate()` idempotent (they already early-return on redundant calls).
- **[Deactivation while an in-flight deferred operation is outstanding]** → A deferred op holds handler `SafeJSValue` roots and could complete after the last device is removed, dispatching against an inactive driver and blocking root reclamation. Mitigation (implemented): `OnLastDeviceRemoved()` calls `CancelAllPendingOperations()` — completing every outstanding deferred op as a failure and releasing its handler roots — before taking the JS mutex and deactivating.

## Migration Plan

- Pure internal refactor of the Matter SBMD driver layer; no public API, GIR, signal, property, or persistence-format change, so no client migration is required.
- Land behind the existing `BCORE_MATTER` build; no new CMake flag.
- Rollback is a straight revert — no on-disk state is written or migrated. If a runtime escape hatch is desired for triaging, an optional config/env toggle could force eager activation at startup (retaining old behavior), but it is not required.

## Open Questions

_Resolved:_
- **Activation validates the re-read spec against the claim stub and rejects on mismatch** (claim-identity fields only). See D4.
- **Active-driver count is a set-on-change `ObservabilityGauge`** (mirroring `registeredDriversGauge`), alongside attributed activate/deactivate counters. See D6.
