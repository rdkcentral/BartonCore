## Why

The SBMD factory eagerly activates every `.sbmd.js` driver at startup and never deactivates any of them, so each spec on disk costs heap for the process lifetime — even device types that are never discovered. The existing specs (`sbmd-v4-runtime`, `sbmd-system`) already describe a lazy lifecycle (activate on claim, deactivate when the last device is removed, metadata available while inactive), but the implementation never conformed. As the SBMD spec catalog grows to cover more Matter device types, the wasted heap for unused drivers scales linearly.

## What Changes

- Stop activating drivers at startup. `SbmdFactory` loads each `.sbmd.js` file, extracts the claim metadata (name, device types, vendor/product IDs, device class, file path), and registers the driver in an **inactive** state.
- After a driver is loaded and its claim metadata is captured, release the retained spec source text and the heavy parsed registration collections (endpoints, resources, aliases, handler references), keeping only the lightweight **claim metadata** resident.
- Activate a driver on demand: when a device first claims it (via `SpecBasedMatterDeviceDriver::AddDevice`, the single funnel used by both fresh commissioning and post-restart re-synchronization), re-read the spec file **from disk**, re-parse, root the handler JSValues, and build the dispatch tables.
- Deactivate a driver when its **per-driver device count** reaches zero (last device removed), releasing handler GC roots, dispatch tables, source text, and heavy registration back down to its claim metadata.
- Add observability for activation/deactivation events and the current active-driver count.
- Bring the implementation into conformance with the already-specified lazy claiming lifecycle, and extend the lifecycle spec with the memory-reclamation (claim-metadata) and disk-reread semantics.

The memory-reclamation change is **not** a breaking change to the consumer contract: the public API, the claim path (`MatterDriverFactory::GetDriver` / `ClaimDevice`), the `DeviceDriver` C struct, and the driver-manager registration all remain in place and behave identically; only the runtime memory footprint of idle drivers changes. A companion hardening, however, does tighten a **spec-author-facing contract**. To keep the disk re-read validation sound (see design D4), the loader now **rejects** a `.sbmd.js` spec whose numeric claim/version fields fall outside their storage ranges — `matter.deviceTypes`/`vendorId`/`productId` must fit `uint16`, and `barton.deviceClassVersion`/endpoint `profileVersion` must fit `uint8`. Such a spec previously loaded with the value silently wrapped/narrowed; it now fails to load. No correct spec relied on that truncation and all in-repo specs still load, but this is an observable change to which `.sbmd.js` files are accepted. It does **not** break a library consumer (e.g. Zilker): the public API/ABI (soname) is unchanged, and `.sbmd.js` specs are assets Barton ships rather than something clients author — only a hand-authored out-of-range *custom* spec would now fail to load. The Barton project version is therefore a patch-level `fix(sbmd):`, not a `!`/`BREAKING CHANGE:` major bump (which would falsely signal ABI incompatibility). The spec-author contract change is instead signaled where SBMD versions its authoring format: the **SBMD schema version is bumped `5.0` → `5.1`** (`sbmd-spec-schema.json` `const`, enforced by `validate_sbmd_specs`), with the tightening recorded in `core/deviceDrivers/matter/sbmd/schema/CHANGELOG.md`.

## Capabilities

### New Capabilities

_None._

### Modified Capabilities

- `sbmd-v4-runtime`: Extend the "Driver lifecycle — activate and deactivate" requirement so that deactivation additionally releases the retained spec source text and heavy registration collections down to the minimal claim metadata, and activation re-reads the spec file from disk rather than from an in-memory copy.
- `sbmd-system`: Clarify that the SBMD factory registers drivers in the inactive (claim-metadata-only) state at startup rather than activating them eagerly, and that a driver is activated on first claim and deactivated when its last device is removed.

## Impact

- **Affected layers**: Matter device drivers (`core/deviceDrivers/matter/sbmd/` — `SbmdFactory`, `SbmdDriver`, `SpecBasedMatterDeviceDriver`), the Matter driver base (`core/deviceDrivers/matter/MatterDeviceDriver` for the last-device-removed hook), and the Matter subsystem class (`core/src/subsystems/matter/Matter` — a non-constructing `IsStackRunning()` static that lets a driver's teardown decide where to run without instantiating the subsystem). No changes to the public API, core services, or persistence.
- **Threading**: Activation and deactivation take `MQuickJsRuntime::GetMutex()`. Activation runs on the thread that calls `AddDevice` (commissioning / device-service), not the Matter thread; last-device and failed-bind teardown is serialized onto the Matter thread so it cannot free the dispatch tables while a report is being dispatched. The one exception is the version-mismatch rollback of a just-activated driver before it has been recorded: it runs the `Deactivate()` under the held JS mutex on the binding thread rather than marshaling to the Matter thread. This is safe for the common case because that driver has no bound device of its own and therefore no live subscription; the only residual reader is a previously-rejected bind's `MatterDevice` whose callbacks hit the `IsActivated()` guard, a race that requires two overlapping binds on the same driver and is otherwise serialized by the JS mutex. The claim path continues to use only C++ metadata and never triggers JS evaluation.
- **I/O**: One spec-file disk read per driver activation (once per driver until its device count returns to zero). Full re-read/re-parse is accepted for now; caching compiled bytecode is a deliberate follow-up (see Non-goals).
- **Observability**: New metrics for activate/deactivate events, active-driver count, per-activation duration (`sbmd.driver.activation.duration_ms`), and activation failures (`sbmd.driver.activation.failure{driver,reason}`).
- **CMake flags**: Relevant only when `BCORE_MATTER` (and the mquickjs runtime, `BCORE_USE_MQUICKJS`) are enabled; SBMD specs directory is configured via `BARTON_CONFIG_MATTER_SBMD_SPECS_DIR`. Builds without Matter are unaffected.
- **Tests**: Unit coverage in `SbmdDriverTest` (reduce-to-claim-metadata, activate-from-disk round trips, numeric-field range validation) and `MatterDeviceEndpointMapTest` (`SbmdLifecycleGuardTest` for the in-flight-bind guard, failed-bind rollback, and failed-activation abort); integration coverage in `testing/test/sbmd_lazy_activation_test.py` (idle-at-startup, activation on commissioning, deactivation on last-device removal, and re-read spec rejection).

## Non-goals

- Caching compiled bytecode (or any parse result) to avoid re-parsing on activation. The first cut re-reads and re-parses from disk; a bytecode/parse cache is a potential future optimization to be evaluated separately.
- Changing the driver-to-device matching semantics, the two-pass (vendor-specific then generic) claim ordering, or the `DeviceDriver` C struct / driver-manager registration model (the "Tier 3" fully-lazy instantiation approach is explicitly out of scope).
- Any change to non-SBMD drivers (native Zigbee, philipsHue) or to non-Matter subsystems.
