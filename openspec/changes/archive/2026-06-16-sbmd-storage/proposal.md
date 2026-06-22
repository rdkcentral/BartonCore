## Why

SBMD handlers need access to per-device persistent and transient key-value storage for
debounce state, last-known values, and operational context that doesn't fit the resource
model. The storage API is partially scaffolded (result builder ops parse but don't execute,
`setTransientData` is missing `ttlSecs`, getter functions don't exist) and needs to be
completed with a correct design: reads through supplements, writes through result ops.

## What Changes

- Add `persistentData` and `transientData` supplement types so handlers can declare
  storage keys to pre-fetch, delivered via `args.supplements.persistentData` and
  `args.supplements.transientData`.
- Fix `storage.setTransientData()` to accept the `ttlSecs` parameter and propagate it
  through the C++ parser and executor.
- Wire `setPersistentData` and `setTransientData` executor to actual Barton storage APIs.
- Remove documented `Sbmd.getPersistentData()` and `Sbmd.getTransientData()` standalone
  accessors from the spec — reads go through supplements only.
- Update `docs/SBMD.md` sections 4.12, 5.1, 7.3 to reflect the correct design.

## Non-goals

- No new storage backends — uses existing Barton device metadata / property APIs.
- No cross-device storage — storage is always scoped to the current device.
- No complex data types — values are always strings.

## Capabilities

### New Capabilities
- `sbmd-storage`: Per-device persistent and transient key-value storage for SBMD handlers,
  with reads via supplements and writes via result ops.

### Modified Capabilities
- `sbmd-system`: Add `persistentData` and `transientData` to the supplements schema.
  Remove `Sbmd.getPersistentData()` and `Sbmd.getTransientData()` standalone accessors.

## Impact

- **SBMD runtime** (`core/deviceDrivers/matter/sbmd/`): SbmdHandlerInvoker executor,
  supplement loading, result op structs.
- **JS bundles** (`scriptCommon/sbmd-result.js`): Fix `setTransientData` signature.
- **Documentation** (`docs/SBMD.md`): Sections 4.12, 5.1, 7.3.
- **Tests** (`core/test/`): Executor tests, supplement tests.
- No CMake flag changes — storage is always available.
