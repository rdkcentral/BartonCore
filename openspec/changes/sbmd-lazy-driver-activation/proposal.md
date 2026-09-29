## Why

The SBMD factory eagerly activates every `.sbmd.js` driver at startup and never deactivates any of them, so each spec on disk costs heap for the process lifetime — even device types that are never discovered. The existing specs (`sbmd-v4-runtime`, `sbmd-system`) already describe a lazy lifecycle (activate on claim, deactivate when the last device is removed, metadata available while inactive), but the implementation never conformed. As the SBMD spec catalog grows to cover more Matter device types, the wasted heap for unused drivers scales linearly.

## What Changes

- Stop activating drivers at startup. `SbmdFactory` loads each `.sbmd.js` file, extracts the claim metadata (name, device types, vendor/product IDs, device class, file path), and registers the driver in an **inactive** state.
- After a driver is loaded and its claim metadata is captured, release the retained spec source text and the heavy parsed registration collections (endpoints, resources, aliases, handler references), keeping only a lightweight **claim stub** resident.
- Activate a driver on demand: when a device first claims it (via `SpecBasedMatterDeviceDriver::AddDevice`, the single funnel used by both fresh commissioning and post-restart re-synchronization), re-read the spec file **from disk**, re-parse, root the handler JSValues, and build the dispatch tables.
- Deactivate a driver when its **per-driver device count** reaches zero (last device removed), releasing handler GC roots, dispatch tables, source text, and heavy registration back down to the claim stub.
- Add observability for activation/deactivation events and the current active-driver count.
- Bring the implementation into conformance with the already-specified lazy claiming lifecycle, and extend the lifecycle spec with the memory-reclamation (stub) and disk-reread semantics.

This is not a **BREAKING** change: the public API, the claim path (`MatterDriverFactory::GetDriver` / `ClaimDevice`), the `DeviceDriver` C struct, and the driver-manager registration all remain in place and behave identically. Only the runtime memory footprint of idle drivers changes.

## Capabilities

### New Capabilities

_None._

### Modified Capabilities

- `sbmd-v4-runtime`: Extend the "Driver lifecycle — activate and deactivate" requirement so that deactivation additionally releases the retained spec source text and heavy registration collections down to a minimal claim stub, and activation re-reads the spec file from disk rather than from an in-memory copy.
- `sbmd-system`: Clarify that the SBMD factory registers drivers in the inactive (claim-metadata-only) state at startup rather than activating them eagerly, and that a driver is activated on first claim and deactivated when its last device is removed.

## Impact

- **Affected layers**: Matter device drivers (`core/deviceDrivers/matter/sbmd/` — `SbmdFactory`, `SbmdDriver`, `SpecBasedMatterDeviceDriver`) and the Matter driver base (`core/deviceDrivers/matter/MatterDeviceDriver` for the last-device-removed hook). No changes to the public API, core services, or other subsystems.
- **Threading**: Activation/deactivation run on the Matter thread and require `MQuickJsRuntime::GetMutex()`; the claim path continues to use only C++ metadata and never triggers JS evaluation.
- **I/O**: One spec-file disk read per driver activation (once per driver until its device count returns to zero). Full re-read/re-parse is accepted for now; caching compiled bytecode is a deliberate follow-up (see Non-goals).
- **Observability**: New metrics for activate/deactivate events and active-driver count.
- **CMake flags**: Relevant only when `BCORE_MATTER` (and the mquickjs runtime, `BCORE_USE_MQUICKJS`) are enabled; SBMD specs directory is configured via `BARTON_CONFIG_MATTER_SBMD_SPECS_DIR`. Builds without Matter are unaffected.
- **Tests**: Unit tests `SbmdDriverTest`, `SbmdFactoryTest`, `SbmdDispatchTest`; integration `testing/test/sbmd_load_metrics_test.py`. New coverage for lazy activation on claim, deactivation on last-device removal, and stub-only residency while inactive.

## Non-goals

- Caching compiled bytecode (or any parse result) to avoid re-parsing on activation. The first cut re-reads and re-parses from disk; a bytecode/parse cache is a potential future optimization to be evaluated separately.
- Changing the driver-to-device matching semantics, the two-pass (vendor-specific then generic) claim ordering, or the `DeviceDriver` C struct / driver-manager registration model (the "Tier 3" fully-lazy instantiation approach is explicitly out of scope).
- Any change to non-SBMD drivers (native Zigbee, philipsHue) or to non-Matter subsystems.
