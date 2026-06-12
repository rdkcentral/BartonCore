## Why

SBMD mapper scripts execute inside a shared mquickjs context protected by a single mutex. If any script enters an infinite loop or performs a computationally unbounded operation, the mutex is held indefinitely, blocking all SBMD device operations across the entire service with no recovery short of process restart. The mquickjs engine exposes `JS_SetInterruptHandler()` for exactly this purpose, but it is not currently wired in. This is a denial-of-service vulnerability that should be addressed before SBMD specs are accepted from broader sources.

## What Changes

- Add a configurable script execution timeout enforced via the mquickjs interrupt handler mechanism (`JS_SetInterruptHandler`).
- Scripts that exceed the timeout will be terminated and the operation will return failure (existing error path).

## Non-goals

- Protecting `SbmdUtils` or JS builtins from cross-script mutation (separate concern, separate change).
- Output validation of cluster/command IDs returned by scripts (separate concern).
- Spec file integrity or signature verification.
- Per-execution context isolation.
- Changes to the quickjs backend (mquickjs is the go-forward engine).

## Capabilities

### New Capabilities
- `sbmd-script-execution-limits`: Script execution timeout via interrupt handler.

### Modified Capabilities
- `sbmd-system`: The existing SBMD system spec gains new requirements for script execution timeout behavior.

## Impact

- **Core layer**: `MQuickJsRuntime` (interrupt handler setup), `SbmdScriptImpl` (timeout tracking in `ExecuteScript`).
- **Build system**: One new CMake integer option for timeout.
- **CMake flags**: `BCORE_MATTER` (existing), `BCORE_MATTER_SBMD_JS_ENGINE` (existing, mquickjs path only).
- **Testing**: New unit tests in `SbmdScriptTest.cpp` for timeout and size limit behavior.
- **No API changes**: This is entirely internal to the SBMD script execution layer, invisible to the public C API.
