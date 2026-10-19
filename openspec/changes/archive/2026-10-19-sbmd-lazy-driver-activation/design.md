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
      │ load, reduce to claim metadata,                                                        │ Activate()/Deactivate()
      │ release source+registration                                               ▼   (under JS mutex)
      ▼                                                                     ┌──────────────┐
  claim metadata resident ◀──────────── Deactivate (last device) ──────────│  SbmdDriver  │
  (name, deviceTypes,                                                       │ active⇄reduced│
   vendor/product, class, path)  ─────── Activate (first device) ─────────▶ └──────────────┘
                                          reads spec file from disk
```

## Goals / Non-Goals

**Goals:**
- Unused (unclaimed) SBMD specs hold only their small claim metadata at steady state — no source text, no heavy registration, no rooted JSValues, no dispatch tables.
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

### D3 — Reduce `SbmdDriver`'s registration to its claim metadata when inactive
`SbmdDriver` keeps its `SbmdRegistration` object for its whole lifetime, but while inactive the registration is reduced to its claim metadata: only the fields used for claiming remain populated — `name`, `filePath`, `barton.deviceClass`, and `matter.deviceTypes`/`vendorId`/`productId`. Deactivation (and the load-time `ReduceToClaimMetadata()`) release the `source` string, the heavy parsed collections (endpoints, aliases, handler vectors), the dispatch tables, and every non-claim metadata field (`schemaVersion`, `driverVersion`, `deviceClassVersion`, `matter.revision`, `featureClusters`, `defaultTimeoutMs`, `reporting`) — swap-with-empty releases the vector capacity. `Activate()` replaces the `SbmdRegistration` object wholesale with the freshly parsed one, so the claim-time metadata accessors (`GetSupportedDeviceTypes`, `GetSupportedVendorId`, `GetSupportedProductId`, `IsVendorSpecificDriver`) must **not** read the live registration — a claim-time caller runs without the JS mutex and could dereference the object mid-swap. `SpecBasedMatterDeviceDriver` therefore caches the claim-identity metadata in immutable members at construction and the accessors read those; activation already rejects any re-read spec whose claim identity changed, so the cached copies never go stale. `Activate()` reads the spec file from `filePath` on disk, parses, and rebuilds the full registration.

*Alternatives considered:* A separate `SbmdClaimStub` struct that backs the accessors — rejected: keeping the reduced `SbmdRegistration` avoids re-pointing every accessor and a parallel type, and reduces to the same resident footprint. Keeping the full registration but only dropping JSValues ("Tier 1") — rejected: leaves the dominant source-text and registration cost resident, failing the goal. Keeping `source` in memory and re-parsing from it — rejected: `source` is the biggest single cost we want to shed.

### D4 — Disk re-read on activation, with claim-identity validation
`SbmdDriver::Activate()` obtains the spec content by re-reading `filePath` from disk whenever the retained `source` has been released (which it always has in the production flow, since the factory reduces every driver to its claim metadata right after registration). A driver not yet reduced that still holds its `source` re-evaluates that copy instead — this keeps unit tests that load from an in-memory string working. Load-time records `filePath`. Guard file-read failure: log and fail activation so the bind fails cleanly rather than dispatching against a half-built driver.

After re-parsing, validate the re-read spec against the claim metadata before going active: compare the claim-identity fields (name, `matter.deviceTypes`, `vendorId`, `productId`, device class). On any mismatch, fail activation, log, and abort the bind. This is scoped to the fields that drove the claim decision — not a deep structural diff — so cosmetic edits to handlers do not falsely reject, but a spec that no longer matches what claimed the device can never dispatch against it.

Separately, `SpecBasedMatterDeviceDriver` caches the device-class version and endpoint profile versions at construction (published to the device-service layer for commissioning/reconfiguration). After a bind-time activation it verifies the re-read spec still carries those same versions and rejects the bind (rolling the activation back) on any change, so the device-service layer never publishes/compares a version that disagrees with the handlers now running. Profile versions are cached as 8-bit values, so a re-read `profileVersion` outside the `uint8` range is rejected outright rather than narrowed for comparison — a narrowed value could alias onto the cached byte and falsely pass. The check compares the complete cached and re-read profile sets, not just the profiles the re-read spec still declares: a profile *removed* on disk would otherwise pass while the cached `endpointProfileVersions` keeps advertising its startup version to the device-service layer, so a differing profile set (added or removed) is rejected too.

The same reasoning applies to every numeric claim/version field that is stored narrower than the JS number it is read from, so the rejection is enforced uniformly at **parse time** (not only on re-read): `matter.deviceTypes`, `vendorId`, and `productId` must fit `uint16`, and `barton.deviceClassVersion` and endpoint `profileVersion` must fit `uint8`. The loader reads each with a saturating conversion and rejects anything out of range, so neither the claim-identity comparison nor the version comparison can be defeated by a value that wraps/narrows to alias an in-range one, and the authoring-time schema (`sbmd-spec-schema.json`, enforced by `validate_sbmd_specs`) encodes the same bounds. *Alternatives considered:* widening the stored fields to `uint32` and comparing at full width — rejected: every one of these fields is consumed downstream as `uint16`/`uint8` (e.g. the `DeviceDriver` C struct stores `deviceClassVersion` as `uint8_t`), so an out-of-range value is already meaningless once published; rejecting at parse is more honest than silently truncating. A consequence is that SBMD accepts only `uint16` Matter device types, whereas Matter device-type IDs are formally 32-bit (vendor-prefixed, e.g. `0xFFF1_xxxx`); this matches the pre-existing storage width but narrows what loads — see the open question in `proposal.md`.

*Alternatives considered:* Trust the file without validation — rejected: a silently-diverged spec could bind handlers inconsistent with the claim. Compare a whole-file hash captured at load — rejected: rejects benign whitespace/comment edits and couples activation to byte-exactness rather than the claim contract.

### D5 — Thread safety
Activation and its cached-version validation run on the thread that calls `AddDevice` — the commissioning / device-service thread, **not** the Matter thread — and take `MQuickJsRuntime::GetMutex()` around the `Activate()`/`Deactivate()` call. Activation and validation are performed as **one** transition under a single JS-mutex hold — if the re-read spec's versions no longer match, the just-done activation is rolled back under the same lock before it is recorded — so any concurrent binder that observes `IsActivated() == true` always sees an already-validated registration rather than one this thread might still reject. The claim-metadata↔registration swap is performed only while the JS mutex is held, and `IsActivated()` checks remain the guard for dispatch entry points (`HandleAttributeReport`, `HandleEvent`, `HandleCommand`, resource ops), which already exist.

Last-device teardown and bind rollback share one serialized primitive, `DeactivateIfIdle()` (caller holds the JS mutex): it deactivates only when the driver is still activated, no bind is in flight (a per-driver in-flight-bind counter is zero), and the device map is empty when rechecked under `devicesMutex`. The last-device-removed hook and every `AddDevice` exit both funnel through it, so a failed bind rolls back only when nothing else relies on the activation (never shedding a device another bind just adopted), and a teardown deferred because a bind was racing is retried when that bind finishes and leaves the driver idle. Because `Deactivate()` frees the dispatch tables that report dispatch reads on the Matter thread, teardown must be serialized with dispatch: the last-device hook already runs on the Matter thread (via `DeviceRemoved`'s `RunOnMatterSync`), and a bind's exit teardown is marshaled there too (`TeardownIfIdle` → `RunOnMatterSync` when the Matter loop is running; run directly otherwise, since with no loop there are no reports). The "is the loop running" test is a non-constructing static check (`Matter::IsStackRunning()`), not `Matter::GetInstance()`, so a bind's `~InFlightBind` destructor can run teardown before the Matter subsystem has been constructed (e.g. in unit tests) without instantiating it — and the destructor swallows any teardown exception so it can never escape a `noexcept` destructor. Lock order is `JS mutex → devicesMutex`: the JS mutex may be held while taking the leaf `devicesMutex` (the emptiness recheck), but `devicesMutex` is never held while acquiring the JS mutex — `DeviceRemoved` releases it before invoking the hook — so no cycle exists, and no lock is held across the bind's Matter-loop work. This protocol is covered deterministically by unit tests (`SbmdLifecycleGuardTest`): with a bind marked in flight `OnLastDeviceRemoved()` defers teardown; once the in-flight count returns to zero `DeactivateIfIdle()` retries it; a bound device makes `DeactivateIfIdle()` a no-op; and `FailedBindDeactivatesOnExit` runs the real `AddDevice` with an unresolvable device so the `InFlightBind` destructor tears the activation down. `FailedActivationAbortsBind` covers the complementary case where the on-demand `Activate()` itself fails (reduced driver, unreadable spec), so the bind aborts without ever activating.

### D6 — Observability
Add metrics, gated by `BARTON_CONFIG_SBMD_METRICS`, following the existing observability helpers:
- Attributed activate/deactivate event counters (`ObservabilityCounter` with a driver-stem attribute) for churn and per-driver attribution.
- A current active-driver count as a synchronous `ObservabilityGauge` **set to the live count on each activate/deactivate transition**, mirroring the existing `registeredDriversGauge` / `RecordRegisteredDriverCount` pattern. The factory also publishes this gauge once after startup registration (counting `IsActivated()` drivers — normally zero), so the gauge reflects true startup state rather than being absent until the first bind.
- A per-activation duration histogram (`sbmd.driver.activation.duration_ms`) recorded around the on-demand `Activate()` call, separate from the startup `sbmd.driver.load.duration_ms` (now load-and-register only).
- An activation-failure counter (`sbmd.driver.activation.failure`) with `driver` and `reason` attributes (`activate_failed` for an unreadable/unparseable spec or claim-identity mismatch, `version_mismatch` for a device-class/profile version or set change), so a device that silently stops binding after a spec drift is visible in telemetry rather than only in logs. The old load-time `reason="activation_failed"` is retired, since activation no longer runs at load.

The active-count gauge and event counters live in the SBMD driver metrics owned by the activation path (`SpecBasedMatterDeviceDriver`); the process-wide active count is maintained as the drivers transition.

*Alternatives considered:* Deriving the current active count by subtracting two monotonic counters (activations − deactivations) — rejected: harder to dashboard, can drift, and the codebase already has a set-on-change gauge idiom.

## Risks / Trade-offs

- **[Re-parse cost on every activation]** → Activation now does a disk read + full parse instead of reusing an in-memory copy. This occurs once per driver until its device count returns to zero, on an already-async commissioning/sync path; acceptable. There is no deactivation hysteresis, so a device that repeatedly flaps off and back on re-parses the spec each cycle; device removal is infrequent and the parse stays on the async path, so this is accepted. Bytecode caching is the documented follow-up if either ever matters.
- **[Spec file changed or removed on disk after startup]** → Activation reads the live file, which could differ from what was loaded at startup. Mitigation (decided, see D4): treat read/parse failure as an activation failure, and validate the re-read spec's claim-identity fields against the claim metadata, rejecting on mismatch (bind fails, logged).
- **[A dispatch or resource path that assumes always-active]** → Any code reading the heavy `GetRegistration()` collections (or `reporting`) while inactive would be unsafe. Mitigation: audit all `GetRegistration()`/dispatch callers; the dispatch entry points already gate on `IsActivated()`, and `GetDesiredSubscriptionIntervalSecs()` falls back to the base default when inactive rather than reading the zeroed `reporting`. The claim-time metadata accessors read only the claim fields, which remain resident on the reduced registration in every state.
- **[Re-entering the Matter loop during last-device teardown]** → `OnLastDeviceRemoved()` runs inside `DeviceRemoved`'s `RunOnMatterSync` block, so anything it calls that re-enters the Matter loop (nested `RunOnMatterSync`/`ConnectAndExecute`) would self-deadlock. Mitigation: the hook is documented as Matter-loop-reentrant-forbidden, and `CancelAllPendingOperations()` only settles promises and releases JS roots (no Matter-loop work).
- **[Concurrent bind/remove racing activation state]** → `AddDevice` activates the driver (on the commissioning thread) before it inserts into the `devices` map, so the empty-map observation that triggers `OnLastDeviceRemoved()` (on the Matter thread) can go stale, a rollback of a failed bind can collide with another bind that adopted the activation, and a teardown deferred during an in-flight bind can be lost. Mitigation: a per-driver in-flight-bind counter (held across the whole of `AddDevice`) plus the single `DeactivateIfIdle()` teardown primitive — shared by the removal hook and every `AddDevice` exit — that deactivates only when the driver is still activated, the counter is zero, **and** the device map is still empty when rechecked under `devicesMutex`. Activation and its version validation are one JS-mutex-held transition, so an active driver is always validated. State swaps stay under the JS mutex and `Activate()`/`Deactivate()` remain idempotent. The counter is a lock-free atomic and the recheck takes only the leaf `devicesMutex`; no lock is held across the Matter-loop-blocking parts of the bind. A failed bind's teardown is additionally marshaled onto the Matter thread (`TeardownIfIdle`) so `Deactivate()` cannot free the dispatch tables while a subscription report is being dispatched into them on that thread; with no Matter loop (hence no subscriptions) it runs directly.
- **[Deactivation while an in-flight deferred operation is outstanding]** → A deferred op holds handler `SafeJSValue` roots and could complete after the last device is removed, dispatching against an inactive driver and blocking root reclamation. Mitigation (implemented): `OnLastDeviceRemoved()` calls `CancelAllPendingOperations()` — completing every outstanding deferred op as a failure and releasing its handler roots — before taking the JS mutex and deactivating.

## Migration Plan

- Pure internal refactor of the Matter SBMD driver layer; no public API, GIR, signal, property, or persistence-format change, so no client migration is required.
- **Spec authors** must ensure numeric claim/version fields fit their ranges: `uint16` for `matter.deviceTypes`/`vendorId`/`productId`, `uint8` for `barton.deviceClassVersion`/endpoint `profileVersion`. A spec with an out-of-range value — previously accepted and silently truncated — now fails to load and must be corrected. The build-time schema (`sbmd-spec-schema.json`) and the `validate_sbmd_specs` target flag these before runtime, and the loader logs `sbmd.driver.load.failure` with an explanatory error.
- Land behind the existing `BCORE_MATTER` build; no new CMake flag.
- Rollback is a straight revert — no on-disk state is written or migrated. If a runtime escape hatch is desired for triaging, an optional config/env toggle could force eager activation at startup (retaining old behavior), but it is not required.

## Open Questions

_Resolved:_
- **Activation validates the re-read spec against the claim metadata and rejects on mismatch** (claim-identity fields only). See D4.
- **Active-driver count is a set-on-change `ObservabilityGauge`** (mirroring `registeredDriversGauge`), alongside attributed activate/deactivate counters. See D6.
- **How the spec-author contract tightening is versioned.** It is signaled via the SBMD schema version, bumped `5.0` → `5.1` and recorded in `schema/CHANGELOG.md`; the Barton project version stays a patch-level `fix(sbmd):` because the public API/ABI (soname) is unchanged and `.sbmd.js` specs are Barton-shipped assets, not client-authored, so no library consumer breaks.

_Open:_
- **Is `uint16`-only the intended long-term contract for `matter.deviceTypes`?** Matter device-type IDs are formally 32-bit (vendor-prefixed, e.g. `0xFFF1_xxxx`), but SBMD has only ever stored them as `uint16`, and the loader now enforces that (see D4). Supporting vendor-prefixed device types would mean widening the stored type and the claim-matching path, not just relaxing the loader — a larger change than this one, deferred until a concrete need arises.
