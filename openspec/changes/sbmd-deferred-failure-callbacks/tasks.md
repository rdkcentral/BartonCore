## 1. Matter device send error reporting (core/deviceDrivers/matter) — ~2h

- [ ] 1.1 Change `MatterDevice::SendCommandWithCallbacks` in `core/deviceDrivers/matter/MatterDevice.h`/`.cpp` to return `CHIP_ERROR` instead of `bool`, returning the specific error at each existing `return false` (TLV structure → `CHIP_ERROR_INVALID_ARGUMENT`, allocation → `CHIP_ERROR_NO_MEMORY`, propagated `err` from `PrepareCommand`/`CopyElement`/`FinishCommand`/`SendCommandRequest`); update the two callers in `SpecBasedMatterDeviceDriver.cpp` to compare against `CHIP_NO_ERROR` (behavior otherwise unchanged)
- [ ] 1.2 Add a test-only hook on `TestableMatterDevice` (`core/test/src/MatterDeviceTestHelpers.h`) to force `SendCommandWithCallbacks` to return a chosen `CHIP_ERROR`, and a gtest in `core/test/src/MatterDeviceEndpointMapTest.cpp` (or a new `MatterDeviceSendTest.cpp`) asserting an invalid TLV payload returns a non-`CHIP_NO_ERROR` value and never invokes either callback

## 2. Failure choke point (SBMD runtime) — ~4h

- [ ] 2.1 Add `DeferredFailure` (with `Internal`/`Transport`/`Timeout`/`CommandFailed`/`ReadFailed` constructors) and `PendingOperation::hopGeneration`/`hopArmed` in `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h`
- [ ] 2.2 Implement `ArmHop`, `ConsumeHop`, `FailPendingOperation(pendingId, failure, Recovery)`; rename `CompletePendingOperation` → `ResolvePendingOperation` and add the armed-hop safety net
- [ ] 2.3 Add `RecordDeferredFailureCallback(driver, type)` and `RecordDeferredFailureBypass(driver)` to `SpecBasedMatterDeviceDriverMetrics`
- [ ] 2.4 Rewrite `HandleDeferredCommandError` and the deadline branch of `HandleDeferredCommandResponse` to delegate to `FailPendingOperation` (`CommandFailed`/`Allowed`, `Timeout`/`Forbidden`); make `HandleDeferredCommandResponse` call `ConsumeHop` before invoking `onResponse`; add the `generation` parameter and stale-generation early return to both
- [ ] 2.5 Tests (gtest, new `core/test/src/SbmdDeferredFailureTest.cpp`, registered in `core/test/CMakeLists.txt` alongside `SbmdCameraWebrtcTest`): timeout and `commandFailed` still invoke `onError` once with unchanged `args.error`; response-then-error for the same generation invokes only `onResponse`; stale-generation callbacks are ignored; safety net invokes `onError` once and increments the bypass metric; recovery terminal from `commandFailed` continues the chain

## 3. Initial requestCommand pre-send failures — ~3h

- [ ] 3.1 Restructure `ExecuteRequestCommand`: create parking promise, register `PendingOperation` with `try_emplace` ID loop (removing the duplicate-ID `FailOperation`), `ArmHop`, then call new `SendArmedRequestCommand`
- [ ] 3.2 Implement `SendArmedRequestCommand` routing endpoint-resolution (`Internal`), TLV decode (`Internal`), and send rejection (`Transport` with `CHIP_ERROR`) to `FailPendingOperation(…, Recovery::Allowed)`
- [ ] 3.3 Tests in `SbmdDeferredFailureTest.cpp`: for endpoint missing, invalid base64, non-structure TLV, and forced send error — `onError` runs exactly once with the expected `type`/`matterCode`; a rollback `setTransientData` from `onError` is visible when the operation's future resolves `false`; `pendingOperations` is empty afterward; `onError` returning `.success("fallback")` completes the operation with `"fallback"`

## 4. Deferred chain re-arm failures — ~4h

- [ ] 4.1 In `ContinueDeferredChain` `RequestCommand` branch: `ArmHop` first, then depth check (`Internal`, `Recovery::Forbidden`), device-removed (`Internal`), session-expired (`Transport`), then `SendArmedRequestCommand`
- [ ] 4.2 In `ContinueDeferredChain` `ReadAttribute` branch: `ArmHop` first, then depth/device checks, route endpoint and TLV-copy failures to `FailPendingOperation`; switch cache-miss to `ConsumeHop` + `ReadFailed`
- [ ] 4.3 Leave non-deferred chained terminals (`SendCommand`, `WriteAttribute`, `Error`, device-removed before a non-deferred terminal) resolving via `ResolvePendingOperation(id, false)` with the hop already consumed; add a comment referencing D2
- [ ] 4.4 Tests in `SbmdDeferredFailureTest.cpp`: re-arm with expired session, device removed, missing endpoint, send error — only the re-armed hop's `onError` runs; max-depth invokes the new hop's `onError` and ignores its recovery terminal; `onError` that keeps returning a failing `requestCommand` terminates at max depth with each `onError` invoked once; `onResponse` returning a plain `sendCommand` that fails does not invoke any `onError`

## 5. Synchronous readAttribute failures — ~2h

- [ ] 5.1 In `ExecuteReadAttribute`, fold endpoint-resolution (`Internal`) and TLV-copy (`Internal`) failures into the existing cache-miss `onError` invocation path, consuming `ra` callbacks once and continuing via `ExecuteTerminal`; keep `FailOperation(promises)` only for "no `onError`" and "`onError` returned no result"
- [ ] 5.2 Tests in `SbmdDeferredFailureTest.cpp`: endpoint missing and TLV-copy failure each invoke `onError` once with `type: "internal"`; cache miss still reports `"readFailed"`; cache hit never invokes `onError`

## 6. Camera regression and driver audit — ~3h

- [ ] 6.1 Audit every in-tree `onError` in `core/deviceDrivers/matter/sbmd/specs/*.sbmd.js` for assumptions that break on `internal`/`transport` failures (e.g. non-null `matterCode`); record findings in the PR description (no driver changes in this change)
- [ ] 6.2 Add `SbmdCameraWebrtcTest` case: force the `VideoStreamAllocate` send to fail on `localSdp`; assert `handleVideoStreamAllocateError` ran once (a `webrtcError` update with the session id and `reason` `"transport"`) before the operation returned failure
- [ ] 6.3 Add a second camera case asserting a subsequent `localSdp` does not adopt the stranded session; mark it `GTEST_SKIP()` referencing PR #285 (camera rollback), since `handleVideoStreamAllocateError` does not yet reset session state

## 7. Integration test (Docker) — ~3h, optional

- [ ] 7.1 Investigate whether the matter.js camera mock (`testing/mocks/devices/matterjs`) can deterministically cause `SendCommandRequest` to fail (e.g. tearing down the CASE session between posts). Requires the Docker dev container; no hardware
- [ ] 7.2 If feasible, add a pytest reproducing the stranded-session scenario (two `localSdp` posts, assert the mock observes a single `VideoStreamAllocate` once PR #285 rollback lands); otherwise document in the PR that the unit tests in 6.2/6.3 are the regression gate

## 8. Documentation — ~1h

- [ ] 8.1 Update `docs/SBMD.md` §5.1 `args.error.type` vocabulary (`timeout`, `commandFailed`, `readFailed`, `transport`, `internal`) with when each occurs
- [ ] 8.2 Update `docs/SBMD.md` §6.2 step 5 and the `requestCommand`/`readAttribute` reference tables with the exactly-once guarantee, pre-send coverage, ordering before caller unblock, recovery rules, and the residual gap for plain terminals returned from `onResponse`

## 9. Verification — ~1h (Docker)

- [ ] 9.1 Build with `BCORE_MATTER=ON` and `BUILD_TESTING=ON` in the Docker dev container and run the full C++ unit suite (`ctest`) — all SBMD tests green
- [ ] 9.2 Run the existing Matter pytest integration suite in Docker to confirm no regressions in deferred flows (door lock, camera, thermostat)
