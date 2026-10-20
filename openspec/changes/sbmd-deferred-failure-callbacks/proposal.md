## Why

Jira: BARTON-412 — "SBMD: synchronous deferred-operation failures never reach the driver's error handler".

When an SBMD driver parks a resource operation on `.device.requestCommand()` or `.device.readAttribute()` it supplies an `onError` callback, and `docs/SBMD.md` §6.2 promises that "timeout or failure routes to `onError`". Today that only holds when the device actually responded with an error or the overall deadline expired (`HandleDeferredCommandError`, the deadline branch of `HandleDeferredCommandResponse`, and the readAttribute cache-miss branch). Every failure that happens *before the command leaves the stack* takes a different path in `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp`:

- `ExecuteRequestCommand` calls `FailOperation(promises)` on endpoint-resolution and base64/TLV decode failure (the driver's `onError` is still sitting in the terminal and is simply released), and calls `CompletePendingOperation(pendingId, false)` when `MatterDevice::SendCommandWithCallbacks` returns `false` — *after* `onError` has already been moved into the `PendingOperation`.
- `ExecuteReadAttribute` calls `FailOperation(promises)` on endpoint-resolution and TLV-copy failure.
- `ContinueDeferredChain` calls `CompletePendingOperation(pendingId, false)` for re-armed `requestCommand`/`readAttribute` hops on endpoint resolution, TLV decode, expired session (`pending.sessionHandle.Get()` empty), device removed (`pending.device.lock()` null), send failure, and max-deferral-depth exceeded — even though the re-armed hop's `onError` is already armed in `pending.onError`. The existing `sbmd-v4-runtime` spec already says max-depth "calls the current hop's `onError`"; the code does not.

All of these unblock the waiting caller with failure but never invoke the driver. A driver that persisted state (via `storage.setTransientData`/`setPersistentData` ops) before parking cannot guarantee that its rollback logic runs. This was observed in the camera driver (PR #285): `allocateThenProvideOffer` persists the session in state `'streaming'` and then parks on `VideoStreamAllocate`; if `SendCommandWithCallbacks` fails, `handleVideoStreamAllocateError` never runs, the session is stranded in `'streaming'`, and `findStreamingSessionIdForDevice` (which matches on state) causes the next `localSdp` to adopt the stranded session and allocate a second video stream, leaking against the camera's encoder budget.

## What Changes

- **Guarantee:** once a driver supplies `onError` for a deferred hop (`requestCommand` or `readAttribute`), the runtime SHALL invoke exactly one of that hop's `onResponse` / `onError`, exactly once, on every path — device response, device error, deadline, and every pre-send / local failure. `onError` SHALL never run after `onResponse` for the same hop, and SHALL never run twice.
- **Single failure choke point** in `SpecBasedMatterDeviceDriver`: a new `FailPendingOperation(pendingId, failure, recovery)` replaces every `CompletePendingOperation(pendingId, false)` call. It disarms the hop, invokes `onError` (with the existing `args.error = { message, type, matterCode }` shape built by `SbmdHandlerInvoker::BuildDeferredErrorArgs`), executes its ops, honors a recovery terminal where allowed, and then resolves the parking promise. `CompletePendingOperation` keeps success-only semantics plus a last-resort safety net that invokes a still-armed `onError` if any future code path reaches it with failure.
- **Register before validate:** `ExecuteRequestCommand` registers the `PendingOperation` (and arms its callbacks) *before* endpoint resolution and TLV decode, so those failures also route through the choke point. The unreachable duplicate-ID failure is removed by allocating IDs with `try_emplace` until success.
- **Synchronous readAttribute:** `ExecuteReadAttribute` routes endpoint-resolution and TLV-copy failures through the same `onError` invocation it already uses for a cache miss.
- **Hop generation:** each armed hop gets a generation number captured by the `SendCommandWithCallbacks` lambdas, so a late or duplicate `OnResponse`/`OnError` from a superseded `CommandSender` can never invoke the current hop's callbacks or fire a callback twice.
- **Failure reason:** pre-send failures use `type: "internal"` (local problem: endpoint, TLV, depth, device gone) or `type: "transport"` (session expired, send rejected), matching the types documented in `docs/SBMD.md` §5.1. `MatterDevice::SendCommandWithCallbacks` returns `CHIP_ERROR` instead of `bool` so the Matter SDK code is surfaced as `matterCode` when available. Existing `"timeout"`, `"commandFailed"`, and `"readFailed"` values are unchanged.
- **Docs:** `docs/SBMD.md` §5.1, §6.2, and the requestCommand/readAttribute reference tables are updated to state the exactly-once guarantee and enumerate every error `type` the runtime emits.
- **Tests:** gtest coverage for every failure class, and a camera-driver regression test for the stranded-session scenario.

### Non-goals

- **Lost-update / stale-snapshot concurrency on driver storage** (BARTON-411). Drivers that read-modify-write `sessions` JSON across hops can still race; that is tracked under the SBMD Data Access Layer epic.
- **SBMD Data Access Layer, staged/transactional writes, and runtime cleanup hooks** (BARTON-420 epic, BARTON-421..425). This change only guarantees the driver *gets the chance* to roll back; it does not make the runtime roll back for it.
- **Non-deferred terminals.** A plain `.device.sendCommand()` / `.device.writeAttribute()` has no `onError`; its failures continue to fail the resource operation only. This includes a fire-and-forget `sendCommand`/`writeAttribute` returned *from* an `onResponse` — that hop's `onResponse` has already run, so its `onError` is not invoked (see design D2).
- **Overall-deadline timer.** The deadline is still only checked when a response arrives; adding an independent timer is a separate concern.
- **Renaming existing error types** (`"commandFailed"`, `"readFailed"`) to the documented `"transport"`/`"internal"` vocabulary. Docs are corrected to list what the runtime actually emits; renaming would break drivers that switch on them.
- **Camera driver rollback logic.** Today `handleVideoStreamAllocateError` only emits `webrtcError`; it does not move the session out of `'streaming'`. This change guarantees the handler runs (with `handlerContext.sessionId`), which is the prerequisite for that rollback; adding the rollback itself belongs to the camera driver work (PR #285) and is not changed here.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `sbmd-v4-runtime`: The "Deferred operations" requirement is tightened (max-depth and pre-send failures invoke `onError`), and a new "Deferred failure callback delivery" requirement defines the exactly-once guarantee, error types, recovery rules, and ordering relative to the waiting caller.

## Impact

- **Affected layers:**
  - Drivers (SBMD runtime, C++): `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp`, `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h`
  - Drivers (Matter base, C++, internal): `core/deviceDrivers/matter/MatterDevice.cpp`, `core/deviceDrivers/matter/MatterDevice.h` (`SendCommandWithCallbacks` return type)
  - Docs: `docs/SBMD.md`
  - Tests: `core/test/src/SbmdDeferredFailureTest.cpp` (new), `core/test/src/SbmdCameraWebrtcTest.cpp`, `core/test/CMakeLists.txt`
  - **Not affected:** public API (`api/c/public/`), GObject types/signals/GIR, core service, subsystems, Zigbee/Thread, SBMD spec files.
- **CMake flags:** `BCORE_MATTER` (required for all touched code); unit tests build under `BUILD_TESTING`. No new flags.
- **Behavior change for drivers:** drivers whose `onError` previously never ran on pre-send failure will now see it run. `onError` must already return a terminal per `docs/SBMD.md`, so well-formed drivers need no change. A driver `onError` that returns a recovery terminal on a pre-send failure will now have that recovery executed (consistent with today's response-error path).
- **No public API / ABI change.** `MatterDevice::SendCommandWithCallbacks` is internal C++ with two callers, both in `SpecBasedMatterDeviceDriver.cpp`.
