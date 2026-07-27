## Tasks

### 1. Add ttlSecs to setTransientData in JS result builder
- **File:** `core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-result.js`
- **Change:** Add `ttlSecs` (3rd arg) to `setTransientData(key, value, ttlSecs)` method in the storage sub-builder. Emit `{ op: "setTransientData", key: key, value: value, ttlSecs: ttlSecs }`.
- **Spec:** sbmd-storage — Transient data write via result op

### 2. Add ttlSecs to C++ SetTransientData struct and parser
- **Files:** `core/deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h`, `SbmdResultExecutor.cpp`
- **Change:** Add `int ttlSecs` field to `SetTransientData` struct. Extract `ttlSecs` from JSON in `ParseOp`.
- **Spec:** sbmd-storage — Transient data write via result op

### 3. Add persistentData/transientData to SbmdSupplements and supplement loader
- **Files:** `core/deviceDrivers/matter/sbmd/SbmdRegistration.h`, `core/deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.cpp`, `core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h`, `core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.cpp`
- **Change:** Add `std::vector<std::string> persistentData` and `std::vector<std::string> transientData` to `SbmdSupplements`. Parse them in `ExtractSupplements`. Add `PersistentDataFetcher` and `TransientDataFetcher` callback types to `SbmdHandlerInvoker::AddSupplements`. Build `args.supplements.persistentData` and `args.supplements.transientData` JS objects from the fetcher results.
- **Spec:** sbmd-storage — Persistent/Transient data read via supplements

### 4. Implement transient storage on SpecBasedMatterDeviceDriver
- **Files:** `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h`, `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp`
- **Change:** Add `std::unordered_map<std::string, TransientEntry>` member (where `TransientEntry = {std::string value; std::chrono::steady_clock::time_point expiry}`). Add `SetTransientData(key, value, ttlSecs)` and `GetTransientData(key) → optional<string>` methods. On get, check expiry and erase if expired.
- **Spec:** sbmd-storage — Transient data write/read

### 5. Wire setPersistentData executor to deviceServiceSetMetadata
- **Files:** `core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.cpp`
- **Change:** In `ExecuteOps`, replace the `setPersistentData` TODO stub with a call to `deviceServiceSetMetadata` using URI `/devices/{deviceUuid}/metadata/sbmd.{key}` with the op's value. Requires `deviceUuid` from handler context.
- **Spec:** sbmd-storage — Persistent data write via result op

### 6. Wire setTransientData executor to in-memory store
- **Files:** `core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.cpp`
- **Change:** In `ExecuteOps`, replace the `setTransientData` TODO stub with a call to the driver's `SetTransientData(key, value, ttlSecs)`.
- **Spec:** sbmd-storage — Transient data write via result op

### 7. Wire supplement fetchers for storage in call sites
- **Files:** `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp`
- **Change:** At each call to `AddSupplements`, provide persistent data fetcher (wrapping `deviceServiceGetMetadata` with `sbmd.` prefix) and transient data fetcher (wrapping driver's `GetTransientData`).
- **Spec:** sbmd-storage — Persistent/Transient data read via supplements

### 8. Update SBMD.md documentation
- **File:** `docs/SBMD.md`
- **Change:** Add `persistentData` and `transientData` to section 4.12 (Supplements). Add them to section 5.1 (Handler args). Update section 7.3 to remove `Sbmd.getPersistentData()`/`Sbmd.getTransientData()`, fix `setTransientData` to show 3 args with `ttlSecs`.
- **Spec:** sbmd-storage — No standalone getter functions; sbmd-system — modified Sbmd built-in library

### 9. Add unit tests for storage
- **Files:** `core/test/src/SbmdResultExecutorTest.cpp`, new test file or existing
- **Change:** Test parsing `setTransientData` with `ttlSecs`. Test supplement building with `persistentData` and `transientData`. Test transient store expiry behavior. Test persistent data op emission.
- **Spec:** All sbmd-storage requirements

### 10. Build and run all tests
- **Command:** `cmake --build build && ctest --output-on-failure --test-dir build -R "Sbmd|ResultBuilder"`
- **Verify:** All existing + new tests pass. No regressions.

---

## Known Doc-vs-Code Discrepancies (SBMD.md)

The following discrepancies were identified between `docs/SBMD.md` and the
actual implementation. Tasks 1-10 above are complete. The items below are
outstanding work to align the doc with the code.

### 11. Fix `requestCommand` signature in §7.2
- **Doc says:** 4 args `(clusterId, commandId, payload, options)` with all deferred fields in `options`
- **Code does:** 5 args `(clusterId, commandId, deferred, tlvBase64, options)` where `deferred = {responseCommandId, onResponse, onError, timeoutMs}`
- **Fix:** Rewrite §7.2 `requestCommand` to show 5-arg form with separate `deferred` and `options` objects

### 12. Fix `readAttribute` signature in §7.2
- **Doc says:** 3 args `(clusterId, attributeId, options)` with callbacks in `options`
- **Code does:** 4 args `(clusterId, attributeId, deferred, options)` where `deferred = {onResponse, onError, timeoutMs}`
- **Fix:** Rewrite §7.2 `readAttribute` to show 4-arg form with separate `deferred` object

### 13. Fix response callback field name (`handler` → `onResponse`)
- **Doc says:** `handler` is the response callback name (§7.2, §6.2)
- **Code does:** `onResponse` is the actual field name parsed by C++
- **Fix:** Replace all `handler:` references with `onResponse:` in §7.2 and §6.2

### 14. Fix `setMetadata` signature in §7.1
- **Doc says:** 2 args `(name, value)` — "Set arbitrary name/value metadata on the device"
- **Code does:** 4 args `(endpoint, resource, key, value)` — but `resource` is parsed then dropped by C++ executor
- **Fix:** Update doc to show 4-arg form. Decide whether to wire `resource` into the C function or remove from JS API.

### 15. Fix §6.2 example to match actual `requestCommand` API
- **Doc example:** Uses 4-arg form with `handler:` field
- **Fix:** Rewrite to use 5-arg form with `deferred` object and `onResponse:` field

### 16. Document `endpointId` option on device operations
- **Code supports:** `options.endpointId` on `sendCommand`, `writeAttribute`, `requestCommand`, `readAttribute`
- **Doc:** Not listed in any options table
- **Fix:** Add `endpointId` to all four device operation options tables

### 17. Remove unimplemented options from doc or implement them
- `sendCommand`: `timeoutMs` and `successValue` documented but not implemented
- `writeAttribute`: `timeoutMs` documented but not implemented
- `requestCommand`/`readAttribute`: `context` documented but not parsed (and `args.handlerContext` not built)
- `requestCommand`: `passthrough` documented but not implemented
- `args.error.matterCode` documented but not set
- **Decision needed:** Remove from doc (and add back when implemented) or implement now

### 18. Document `Sbmd.Tlv.TYPE` constants
- **Code:** `Sbmd.Tlv.TYPE` exports TLV type constants (SIGNED_INT, UNSIGNED_INT, BOOLEAN, etc.)
- **Doc:** Not mentioned
- **Fix:** Add subsection documenting `Sbmd.Tlv.TYPE` and its constants

### 19. Document or remove `Sbmd.Tlv.decodeStruct()`
- **Code:** Exported as alias for `decode()`
- **Doc:** Not mentioned
- **Fix:** Either document it or remove the export
