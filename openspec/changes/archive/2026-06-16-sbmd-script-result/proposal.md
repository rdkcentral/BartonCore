## Why

SBMD mapper scripts return JSON objects whose structure is inconsistently defined and validated in two parallel, engine-specific implementations (`quickjs/SbmdScriptImpl.cpp` and `mquickjs/SbmdScriptImpl.cpp`). The three observable outcomes a script result can express — error, suppress, and operation — have no formal contract, leaving script authors without a clear specification and duplicating all validation logic across both engines. This change formalizes the contract via a `ScriptResult` C++ class and a minor JSON schema revision.

## What Changes

- **NEW**: `ScriptResult` C++ class that parses, validates, and provides typed accessors for all SBMD script results, with a single shared `FromJsonValue()` factory method
- **NEW**: `ResourceUpdate` operation type (nested struct in `ScriptResult`) representing a Barton resource string update — the result produced by read, event, seedFrom, and command-response mappers
- **MODIFIED**: `ScriptWriteResult` — `OperationType::None` removed; the `optional<variant>` in `ScriptResult` handles the absent-operation case
- **MODIFIED**: `SbmdScript` virtual interface — all mapper methods (`MapAttributeRead`, `MapWrite`, `MapExecute`, `MapEvent`, `MapCommandExecuteResponse`) return `ScriptResult` by value instead of `bool` + output parameters
- **MODIFIED**: Engine implementations (`quickjs/SbmdScriptImpl.cpp`, `mquickjs/SbmdScriptImpl.cpp`) — all field extraction and validation logic removed; engines become thin wrappers that run the script, catch JS exceptions as error `ScriptResult`s, extract result fields from the JSValue into a `Json::Value`, and call `ScriptResult::FromJsonValue()`
- **BREAKING**: SBMD script JSON schema revision 2.0 → 3.0: `output` key renamed to `value`; `error` string key introduced as a valid return
- **NEW**: `Sbmd.Response.value(v)` and `Sbmd.Response.error(msg)` JavaScript helpers in `sbmd-utils.js`
- **MODIFIED**: All `.sbmd` spec files — `{output: ...}` → `{value: ...}`, `schemaVersion` bumped to `"3.0"`
- **MODIFIED**: `sbmd-script.d.ts` TypeScript interface — `SbmdReadResult`, `SbmdEventResult`, `SbmdCommandResponseResult` updated to use `value`; new `SbmdErrorResult` type added

### Non-goals

- No changes to the SBMD YAML schema structure (aliases, endpoints, resources, reporting, etc.)
- No changes to the `invoke` or `write` JSON shapes returned by write/execute scripts
- No implementation of scriptlet feedback (the "tell the script what Barton did" concept) — deferred
- No new operation types beyond `ResourceUpdate`, `Invoke`, and `Write`

## Capabilities

### New Capabilities

- `sbmd-script-result`: A formal, validated C++ representation of SBMD script results with a unified contract (error, suppress, or operation outcomes), shared parsing logic, and typed accessor API

### Modified Capabilities

- None — the SBMD script result contract has not previously been formally specified; this is a new capability, not a revision to an existing one

## Impact

- **Device drivers layer**: `core/deviceDrivers/matter/sbmd/` — primary site of change
  - `SbmdScript.h` (interface changes)
  - `quickjs/SbmdScriptImpl.cpp`, `mquickjs/SbmdScriptImpl.cpp` (validation removed, thinned)
  - New files: `ScriptResult.h`, `ScriptResult.cpp`
- **Matter device handling**: `core/deviceDrivers/matter/MatterDevice.cpp` and `SpecBasedMatterDeviceDriver.cpp` (if applicable) — call sites updated to use `ScriptResult` accessors
- **JS runtime commons**: `core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-utils.js`, `sbmd-script.d.ts`
- **SBMD specs**: all `core/deviceDrivers/matter/sbmd/specs/*.sbmd` files
- **Tests**: `core/test/src/SbmdScriptTest.cpp` updated; `core/test/src/ScriptResultTest.cpp` (new file) added with unit tests for `ScriptResult::FromJsonValue()` (no JS engine required)
- **CMake flags**: `BCORE_MATTER` — change is scoped entirely to the Matter subsystem
- **Dependencies**: JsonCpp (`<json/json.h>`) — already linked in Matter-enabled core builds; no new dependency
- **Spike required**: Verify that the mquickjs engine can produce a `Json::Value` from a script result (the engine already uses `JS_GetPropertyStr` to extract individual output fields; confirm that those extracted values can be assembled into a `Json::Value` before implementation begins)
