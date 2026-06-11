## 1. Spike: mquickjs Json::Value extraction

- [x] 1.1 Read `mquickjs/SbmdScriptImpl.cpp` and `MQuickJsRuntime.h` to verify that `JS_GetPropertyStr` and `JS_ToCString`/`JS_ToUint32` are available on the mquickjs engine's JSContext
- [x] 1.2 Confirm that a `Json::Value` object can be populated from a script result JSValue using those APIs (same field-extraction approach used in `quickjs/SbmdScriptImpl.cpp`)
- [x] 1.3 If `Json::Value` building is not feasible, evaluate JS-level `JSON.stringify` evaluation as fallback and document the chosen path in `design.md` OQ1

## 2. ScriptResult class — header and implementation

- [x] 2.1 Create `core/deviceDrivers/matter/sbmd/ScriptResult.h` — define `ScriptResult` class with `ResourceUpdate` nested struct, `optional<string> error`, `optional<variant<ResourceUpdate, ScriptWriteResult>> operation`, and accessor methods (`IsError()`, `SkipsResourceUpdate()`, `HasOperation()`, `ErrorMessage()`, `Operation()`)
- [x] 2.2 Create `core/deviceDrivers/matter/sbmd/ScriptResult.cpp` — implement `ScriptResult::FromJsonValue(const Json::Value&)` handling all five JSON shapes: empty object (suppress), `value` key (ResourceUpdate), `invoke` key (Invoke ScriptWriteResult), `write` key (Write ScriptWriteResult), `error` key (error)
- [x] 2.3 Implement ambiguous-JSON detection in `FromJsonValue()` — return error ScriptResult when more than one of `value`/`invoke`/`write`/`error` is present
- [x] 2.4 Implement base64 TLV decoding inside `FromJsonValue()` for `invoke.tlvBase64` and `write.tlvBase64` fields (move from engine impls)
- [x] 2.5 Add `ScriptResult.cpp` to the CMakeLists.txt build target for the SBMD library

## 3. ScriptResult unit tests (no JS engine required)

- [x] 3.1 Create `core/test/src/ScriptResultTest.cpp` with unit tests for `ScriptResult::FromJsonValue()` covering: suppress (empty object), value result, invoke result, write result, error result, ambiguous JSON (multiple keys), missing required `invoke` fields (`clusterId`, `commandId`), missing required `write` fields (`clusterId`, `attributeId`, `tlvBase64`), non-object input; register the file in the test CMakeLists.txt
- [x] 3.2 Run unit tests and confirm all new `ScriptResult` tests pass: `ctest --output-on-failure --test-dir build -R ScriptResult`

## 4. Update SbmdScript virtual interface

- [x] 4.1 Update `SbmdScript.h` — change `MapAttributeRead`, `MapCommandExecuteResponse`, `MapWrite`, `MapExecute`, and `MapEvent` signatures to return `ScriptResult` by value, removing `bool` return and output parameters
- [x] 4.2 Update all docstrings in `SbmdScript.h` to describe the new `ScriptResult` contract (error / suppress / operation states)

## 5. Update quickjs engine implementation

- [x] 5.1 Update `quickjs/SbmdScriptImpl.cpp` — replace `MapAttributeRead` implementation: after `ExecuteScript`, build a `Json::Value` from the JSValue result and call `ScriptResult::FromJsonValue()`; remove `ExtractScriptOutputAsString` usage
- [x] 5.2 Update `quickjs/SbmdScriptImpl.cpp` — replace `MapCommandExecuteResponse` implementation with same `Json::Value` → `ScriptResult::FromJsonValue()` pattern
- [x] 5.3 Update `quickjs/SbmdScriptImpl.cpp` — replace `MapWrite` implementation: build `Json::Value` from JSValue and call `ScriptResult::FromJsonValue()`; remove the `ParseWriteResult`-style field extraction
- [x] 5.4 Update `quickjs/SbmdScriptImpl.cpp` — replace `MapExecute` implementation with same pattern
- [x] 5.5 Update `quickjs/SbmdScriptImpl.cpp` — replace `MapEvent` implementation: build `Json::Value` from JSValue and call `ScriptResult::FromJsonValue()`; remove tri-state logic
- [x] 5.6 Update `quickjs/SbmdScriptImpl.cpp` — convert JS exception handling to synthesize an error `ScriptResult` (rather than returning `false`)
- [x] 5.7 Delete dead helper functions from `quickjs/SbmdScriptImpl.cpp` (`ExtractScriptOutputAsString`, and any other helpers fully replaced by `ScriptResult`)

## 6. Update mquickjs engine implementation

- [x] 6.1 Update `mquickjs/SbmdScriptImpl.cpp` — replace `MapAttributeRead` with `Json::Value` → `ScriptResult::FromJsonValue()` pattern (informed by spike in task 1)
- [x] 6.2 Update `mquickjs/SbmdScriptImpl.cpp` — replace `MapCommandExecuteResponse` implementation
- [x] 6.3 Update `mquickjs/SbmdScriptImpl.cpp` — replace `MapWrite` implementation
- [x] 6.4 Update `mquickjs/SbmdScriptImpl.cpp` — replace `MapExecute` implementation
- [x] 6.5 Update `mquickjs/SbmdScriptImpl.cpp` — replace `MapEvent` implementation
- [x] 6.6 Update `mquickjs/SbmdScriptImpl.cpp` — convert JS exception handling to synthesize an error `ScriptResult`
- [x] 6.7 Delete dead helper functions from `mquickjs/SbmdScriptImpl.cpp`

## 7. Update MatterDevice.cpp call sites

- [x] 7.1 Update all `MapAttributeRead` call sites in `MatterDevice.cpp` to use `ScriptResult` accessors (`HasOperation()`, `Operation()`, `IsError()`, `SkipsResourceUpdate()`)
- [x] 7.2 Update all `MapEvent` call sites in `MatterDevice.cpp` — replace empty-string suppress check with `SkipsResourceUpdate()`
- [x] 7.3 Update all `MapWrite` and `MapExecute` call sites in `MatterDevice.cpp` — replace `ScriptWriteResult` out-param pattern with `ScriptResult` operation accessor
- [x] 7.4 Update `MapCommandExecuteResponse` call sites in `MatterDevice.cpp`
- [x] 7.5 Update any `SpecBasedMatterDeviceDriver.cpp` call sites if applicable
- [x] 7.6 Remove `OperationType::None` from `ScriptWriteResult` in `SbmdScript.h` — safe now that all engine and call-site references have been replaced in tasks 5–7

## 8. JSON schema v3.0 — JS helpers and TypeScript types

- [x] 8.1 Add `SbmdUtils.Response.value(v)` helper to `sbmd-utils.js` (returns `{ value: String(v) }`)
- [x] 8.2 Add `SbmdUtils.Response.error(msg)` helper to `sbmd-utils.js` (returns `{ error: msg }`)
- [x] 8.3 Update `sbmd-script.d.ts` — rename `output` to `value` in `SbmdReadResult`, `SbmdEventResult`, `SbmdCommandResponseResult`
- [x] 8.4 Add `SbmdErrorResult` interface to `sbmd-script.d.ts` (`{ error: string }`)
- [x] 8.5 Update JSDoc examples in `sbmd-script.d.ts` to use `SbmdUtils.Response.value()` for read/event results

## 9. Migrate bundled SBMD spec files to schema v3.0

- [x] 9.1 Update all `core/deviceDrivers/matter/sbmd/specs/*.sbmd` files — change `schemaVersion: "2.0"` → `schemaVersion: "3.0"` and rename all `{output:` → `{value:` in script bodies (affects: `door-lock.sbmd`, `light.sbmd`, `thermostat.sbmd`, `contact-sensor.sbmd`, `occupancy-sensor.sbmd`, `temperature-sensor.sbmd`, `humidity-sensor.sbmd`, `air-quality-sensor.sbmd`, `water-leak-detector.sbmd`, `ikea-timmerflotte.sbmd`)
- [x] 9.2 Verify no `output:` key remains in any bundled spec script body after migration

## 10. Update existing tests

- [x] 10.1 Update `SbmdScriptTest.cpp` test fixtures — replace all `{output: ...}` JSON strings with `{value: ...}`
- [x] 10.2 Update `SbmdScriptTest.cpp` — replace `bool + outValue` result checks with `ScriptResult` accessor assertions
- [x] 10.3 Add a behavior test verifying that when `MapEvent()` returns a `ScriptResult` where `SkipsResourceUpdate()` is true, the `MatterDevice` layer does NOT call `updateResource` — add to `MatterDeviceTest.cpp` (create if it doesn't exist) since this tests `MatterDevice.cpp` behavior, not script parsing
- [x] 10.4 Run full unit test suite and confirm all tests pass: `ctest --output-on-failure --test-dir build`; note whether any integration tests cover the door-lock suppress path and verify they still pass

## 11. Documentation update

- [x] 11.1 Update `docs/SBMD.md` — document the v3.0 script return contract: `value`, `invoke`, `write`, `error`, and empty-object suppress; document `SbmdUtils.Response.*` helpers; note schema version history
