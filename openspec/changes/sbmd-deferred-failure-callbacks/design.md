# Design: SBMD deferred failure callbacks (BARTON-412)

Guarantee that a parked SBMD operation's `onError` runs exactly once on every failure path, including failures that occur before the command leaves the stack.

## Context

The SBMD runtime (`core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp`) executes a handler's `ParsedResult` via `ExecuteTerminal`. Two terminals are *deferred*: `ResultTerminal::RequestCommand` (handled by `ExecuteRequestCommand`) and `ResultTerminal::ReadAttribute` (handled by `ExecuteReadAttribute`). A deferred `requestCommand` pushes a parking promise onto the caller's `std::forward_list<std::promise<bool>>`, records a `PendingOperation` in `pendingOperations` (a `std::map<uint64_t, PendingOperation>` confined to the Matter event-loop thread), moves the terminal's `onResponse`/`onError`/`context` `SafeJSValue`s into it under `MQuickJsRuntime`'s mutex, and calls `MatterDevice::SendCommandWithCallbacks` with lambdas that route to `HandleDeferredCommandResponse` / `HandleDeferredCommandError`. Those handlers invoke the JS callback via `SbmdHandlerInvoker::InvokeHandler`, run its ops, and either continue the chain (`ContinueDeferredChain`) or resolve the parking promise via `CompletePendingOperation(pendingId, success)`, which also releases the held callbacks (`ReleasePendingHandlers`) and erases the record.

`readAttribute` is synchronous: `ExecuteReadAttribute` reads the attribute cache and immediately invokes `onResponse` (hit) or `onError` with `type: "readFailed"` (miss), then recurses into `ExecuteTerminal` with the callback's result. Inside a chain (`ContinueDeferredChain`), a `readAttribute` hop re-arms the pending record's callbacks and does the same.

`onError` arguments are built by `SbmdHandlerInvoker::BuildDeferredErrorArgs(ctx, hctx, errorType, errorMessage, matterCode = -1, handlerContext)` (`core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h`), producing `args.error = { message, type, matterCode }` with `matterCode === null` when `-1`. The runtime currently emits `type` values `"timeout"`, `"commandFailed"` (with `error.AsInteger()`), and `"readFailed"`. `docs/SBMD.md` §5.1 documents `"timeout"`, `"transport"`, `"internal"`.

The defect: there are ~18 failure sites that unblock the caller without calling `onError`.

| Site | Function | Today | Hop armed? |
|---|---|---|---|
| Endpoint resolution | `ExecuteRequestCommand` | `FailOperation(promises)` | callbacks still in terminal |
| Base64 TLV decode | `ExecuteRequestCommand` | `FailOperation(promises)` | callbacks still in terminal |
| Duplicate pending ID | `ExecuteRequestCommand` | `FailOperation(promises)` | callbacks still in terminal |
| `SendCommandWithCallbacks` → `false` | `ExecuteRequestCommand` | `CompletePendingOperation(id,false)` | **yes, in `PendingOperation`** |
| Endpoint resolution | `ExecuteReadAttribute` | `FailOperation(promises)` | callbacks in terminal |
| TLV copy (`EncodeTlvElementToBase64`) | `ExecuteReadAttribute` | `FailOperation(promises)` | callbacks in terminal |
| Max deferral depth | `ContinueDeferredChain` | `CompletePendingOperation(id,false)` | current hop consumed (see D2) |
| Device removed (`pending.device.lock()` null) | `ContinueDeferredChain` | `CompletePendingOperation(id,false)` | depends on next terminal |
| Re-arm requestCommand: endpoint / decode / session expired / send false | `ContinueDeferredChain` | `CompletePendingOperation(id,false)` | **yes, re-armed** |
| Re-arm readAttribute: endpoint / TLV copy | `ContinueDeferredChain` | `CompletePendingOperation(id,false)` | **yes, re-armed** |
| `onResponse` returned no result | `HandleDeferredCommandResponse` | `CompletePendingOperation(id,false)` | consumed by `onResponse` |
| Chained sendCommand/writeAttribute: endpoint / decode / session / send | `ContinueDeferredChain` | `CompletePendingOperation(id,false)` | consumed by `onResponse` |

`MatterDevice::SendCommandWithCallbacks` (`core/deviceDrivers/matter/MatterDevice.cpp`) stores the callbacks in `activeCommandContexts` only after `SendCommandRequest` succeeds, so a `false` return never invokes them — there is no double-fire risk from the device layer on that path. However `MatterDevice::OnResponse` routes a non-success `statusIB` to `deferredOnError`, and the CHIP `CommandSender` can subsequently call `OnError` for the same exchange, so the driver layer must already tolerate a second callback for a hop.

The camera driver (`core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js`) demonstrates the impact: `allocateThenProvideOffer` / `allocateThenSolicitOffer` emit `storage.setTransientData(TD_SESSIONS, …)` with the session already in state `'streaming'` and then park on `VideoStreamAllocate` with `onError: handleVideoStreamAllocateError`. A pre-send failure skips that handler; `findStreamingSessionIdForDevice` later adopts the stranded session.

No `docs/dev/architecture.md` exists in the project.

## References

- `docs/SBMD.md` §5.1 (deferred handler args), §6.2 (requestCommand runtime behavior), §7.2 reference tables — the contract this change enforces.
- `openspec/specs/sbmd-v4-runtime/spec.md` "Deferred operations" — already requires max-depth to call the current hop's `onError`; code diverges.
- `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h` — `PendingOperation`, thread-confinement invariant comment on `transientStore`/`pendingOperations`.
- `core/deviceDrivers/matter/MatterDeviceDriver.h` `FailOperation` — pushes a pre-failed promise to the front so `ConnectAndExecute` stops waiting.
- [Matter SDK `CommandSender::Callback`](https://github.com/project-chip/connectedhomeip/blob/master/src/app/CommandSender.h) — `OnResponse`/`OnError`/`OnDone` ordering; `OnError` may follow a status-bearing response.

## Goals / Non-Goals

**Goals:**

- Every armed deferred hop resolves through exactly one of `onResponse` / `onError`, exactly once.
- All failure transitions of a `PendingOperation` go through one function; resolving with failure while a hop is still armed is impossible to do silently.
- Pre-send failures report `args.error` with the existing shape and a meaningful `type`/`message`/`matterCode`.
- `onError` runs on the Matter thread *before* the waiting caller is unblocked, so the caller observes the driver's rollback.
- Stale or duplicate device callbacks can never fire a newer hop's callbacks.

**Non-Goals:**

- BARTON-411 lost-update concurrency; BARTON-420..425 data access layer, staged writes, cleanup hooks.
- `onError` for non-deferred terminals (`sendCommand`, `writeAttribute`), including those returned from `onResponse`.
- An independent overall-deadline timer.
- Renaming `"commandFailed"`/`"readFailed"`.

## Decisions

### D1: Failure descriptor with stable `type` vocabulary

**Decision:** Introduce an internal `DeferredFailure` value carrying `type`, `message`, and `matterCode`, with named constructors so every site picks from a fixed vocabulary. Pre-send local failures use `"internal"`; failures attributable to the session/transport use `"transport"`; existing sites keep `"timeout"`, `"commandFailed"`, `"readFailed"`.

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h  (inside namespace barton, before PendingOperation)

/**
 * Why a deferred hop failed. Converted to args.error = { message, type, matterCode } by
 * SbmdHandlerInvoker::BuildDeferredErrorArgs. matterCode -1 is surfaced to JS as null.
 */
struct DeferredFailure
{
    std::string type;
    std::string message;
    int32_t matterCode = -1;

    static DeferredFailure Internal(std::string message) { return {"internal", std::move(message), -1}; }

    static DeferredFailure Transport(std::string message, CHIP_ERROR err = CHIP_NO_ERROR)
    {
        return {"transport",
                std::move(message),
                err == CHIP_NO_ERROR ? -1 : static_cast<int32_t>(err.AsInteger())};
    }

    static DeferredFailure Timeout(std::string message) { return {"timeout", std::move(message), -1}; }

    static DeferredFailure CommandFailed(CHIP_ERROR err)
    {
        return {"commandFailed", err.AsString(), static_cast<int32_t>(err.AsInteger())};
    }

    static DeferredFailure ReadFailed(std::string message) { return {"readFailed", std::move(message), -1}; }
};
```

| Failure | `type` | `message` | `matterCode` |
|---|---|---|---|
| Endpoint not found for cluster | `internal` | `"No endpoint for cluster 0x%x"` | null |
| Base64 / TLV decode or copy | `internal` | `"Invalid TLV payload"` | null |
| Max deferral depth | `internal` | `"Max deferral depth (10) exceeded"` | null |
| Device removed mid-chain | `internal` | `"Device removed"` | null |
| Session expired before send | `transport` | `"Session expired"` | null |
| `SendCommandWithCallbacks` failed | `transport` | `"Send failed: <CHIP_ERROR string>"` | CHIP error |
| Overall deadline (existing) | `timeout` | unchanged | null |
| Device error / IM status (existing) | `commandFailed` | unchanged | CHIP error |
| Cache miss (existing) | `readFailed` | unchanged | null |

**Alternative considered:** Reuse `"commandFailed"` for pre-send failures. Rejected — drivers cannot distinguish "device rejected" from "never sent", and the documented vocabulary already has `"transport"`/`"internal"`.

**Rationale / trade-offs:** No new JS shape; drivers that only read `args.error.type` for logging (camera) keep working. Two vocabularies coexist until a future rename.

---

### D2: Hop arming model and the exactly-once rule

**Decision:** A *hop* is armed when its `onResponse`/`onError` are stored (initial `ExecuteRequestCommand`, or a re-arm in `ContinueDeferredChain`/`ExecuteReadAttribute`). A hop is *consumed* the instant the runtime decides to invoke either callback; consumption moves both callbacks out of the record under the JS mutex before invocation, so the other callback can no longer be reached. Each arm increments `PendingOperation::hopGeneration`.

Consequences:

- Pre-send failures of an armed hop → `onError` (new behavior).
- Failures caused by the *result* of a consumed hop (e.g. `onResponse` returns a plain `sendCommand` whose send fails, or returns no result) do **not** invoke that hop's `onError` — doing so would run both callbacks for one hop. They fail the parked operation only. If that result is itself a deferred terminal, that new hop is armed first and its own `onError` covers it.
- Max deferral depth is checked *when arming* a new hop (after its callbacks are stored), so the new hop's `onError` is invoked with recovery disallowed. This satisfies the existing spec scenario ("the current hop's `onError` handler is called").

**Alternative considered:** Invoke the previous hop's `onError` when its `onResponse` result fails. Rejected — violates "never both", and the driver already had control in `onResponse` and chose a fire-and-forget terminal; drivers needing rollback there should use `requestCommand`.

**Trade-off:** A narrow residual gap remains for `onResponse` → plain `sendCommand` pre-send failures. Called out in docs.

---

### D3: Single choke point `FailPendingOperation`

**Decision:** All failure resolution of a registered `PendingOperation` goes through `FailPendingOperation`. `CompletePendingOperation` is renamed `ResolvePendingOperation` and becomes private plumbing; when called with `success == false` and the hop is still armed, it logs an error, increments a metric, and invokes `onError` with recovery disallowed as a safety net — so a future site that forgets the choke point still delivers the callback.

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h  (PendingOperation additions)
        // Incremented every time a hop's callbacks are armed. Device-layer callbacks capture the
        // generation they were issued for; a mismatch means the callback belongs to a superseded hop.
        uint32_t hopGeneration = 0;

        // True while onResponse/onError belong to a hop that has not yet been consumed.
        bool hopArmed = false;
```

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h  (private section)
        enum class Recovery
        {
            Allowed,   // a non-error terminal from onError continues the chain
            Forbidden  // onError ops run, but the operation fails regardless of its terminal
        };

        /** Arm a hop: move callbacks into the record under the JS mutex and bump hopGeneration. */
        void ArmHop(PendingOperation &pending, SafeJSValue &onResponse, SafeJSValue &onError);

        /**
         * Consume the current hop and return its callbacks. After this returns, pending.onResponse and
         * pending.onError are empty and hopArmed is false. Must be called on the Matter thread.
         */
        std::pair<SafeJSValue, SafeJSValue> ConsumeHop(PendingOperation &pending);

        /**
         * The only failure path for a registered pending operation. Invokes the armed hop's onError
         * exactly once (if armed and supplied), executes its ops, then either continues the chain with
         * its recovery terminal (Recovery::Allowed) or resolves the parking promise with false.
         * The PendingOperation may be erased on return; callers must not touch it afterward.
         */
        void FailPendingOperation(uint64_t pendingId, const DeferredFailure &failure, Recovery recovery);

        /** Resolve the parking promise, record metrics, release handlers, erase. */
        void ResolvePendingOperation(uint64_t pendingId, bool success);
```

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp
void SpecBasedMatterDeviceDriver::ArmHop(PendingOperation &pending, SafeJSValue &onResponse, SafeJSValue &onError)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
    pending.onResponse = std::move(onResponse);
    pending.onError = std::move(onError);
    pending.hopGeneration++;
    pending.hopArmed = true;
}

std::pair<SafeJSValue, SafeJSValue> SpecBasedMatterDeviceDriver::ConsumeHop(PendingOperation &pending)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
    std::pair<SafeJSValue, SafeJSValue> callbacks {std::move(pending.onResponse), std::move(pending.onError)};
    pending.hopArmed = false;
    return callbacks;
}

void SpecBasedMatterDeviceDriver::FailPendingOperation(uint64_t pendingId,
                                                       const DeferredFailure &failure,
                                                       Recovery recovery)
{
    auto it = pendingOperations.find(pendingId);

    if (it == pendingOperations.end())
    {
        return;
    }

    PendingOperation &pending = it->second;

    icError("Deferred operation %" PRIu64 " failed (%s): %s", pendingId, failure.type.c_str(), failure.message.c_str());

    if (!pending.hopArmed)
    {
        ResolvePendingOperation(pendingId, false);
        return;
    }

    auto callbacks = ConsumeHop(pending);
    SafeJSValue &onError = callbacks.second;

    std::optional<ParsedResult> errorResult;
    ScopedResultRelease errorResultRelease {errorResult};
    {
        auto lock = MQuickJsRuntime::Instance().AcquireMutex();
        auto *ctx = MQuickJsRuntime::Instance().GetSharedContext();

        if (onError.HasValue())
        {
            SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(ctx,
                                                                          pending.handlerContext,
                                                                          failure.type,
                                                                          failure.message,
                                                                          failure.matterCode,
                                                                          pending.context.Get());
            errorResult = SbmdHandlerInvoker::InvokeHandler(ctx, onError.Get(), args, &pending.operationCtx);
        }

        // Release the consumed callbacks while the mutex is held.
        callbacks = {};
    }

    metrics.RecordDeferredFailureCallback(pending.operationCtx.driverStem.c_str(), failure.type.c_str());

    if (errorResult.has_value())
    {
        SbmdHandlerInvoker::ExecuteOps(
            pending.handlerContext, errorResult->ops, MakeTransientSetter(pending.handlerContext.deviceUuid));

        if (recovery == Recovery::Allowed &&
            !std::holds_alternative<ResultTerminal::Error>(errorResult->terminal.data))
        {
            // May re-arm and re-enter FailPendingOperation; bounded by MAX_DEFERRAL_DEPTH.
            ContinueDeferredChain(pending, *errorResult);
            return;
        }
    }

    ResolvePendingOperation(pendingId, false);
}

void SpecBasedMatterDeviceDriver::ResolvePendingOperation(uint64_t pendingId, bool success)
{
    auto it = pendingOperations.find(pendingId);

    if (it == pendingOperations.end())
    {
        return;
    }

    if (!success && it->second.hopArmed)
    {
        // Safety net: a failure site bypassed FailPendingOperation. Deliver onError anyway.
        icError("Deferred operation %" PRIu64 " failed with an armed hop outside FailPendingOperation", pendingId);
        metrics.RecordDeferredFailureBypass(it->second.operationCtx.driverStem.c_str());
        FailPendingOperation(pendingId, DeferredFailure::Internal("Operation failed"), Recovery::Forbidden);
        return;
    }

    // ... existing CompletePendingOperation body: metrics, parkingPromise->set_value(success),
    // ReleasePendingHandlers(pending), pendingOperations.erase(it).
}
```

`HandleDeferredCommandError` and the deadline branch of `HandleDeferredCommandResponse` are rewritten as one-line delegations: `FailPendingOperation(id, DeferredFailure::CommandFailed(err), Recovery::Allowed)` and `FailPendingOperation(id, DeferredFailure::Timeout("Overall operation deadline exceeded"), Recovery::Forbidden)`. `HandleDeferredCommandResponse` calls `ConsumeHop` before invoking `onResponse`.

**Alternative considered:** RAII scope guard in each executor that fires `onError` on destruction unless dismissed. Rejected — invoking JS and executing ops (which may re-enter the driver and erase the record) from a destructor is hard to reason about, cannot propagate a recovery terminal, and still needs the same lookup logic. The safety net in `ResolvePendingOperation` gives the "cannot bypass" property without destructor side effects.

**Rationale / trade-offs:** One function owns invocation, ops, recovery, and promise resolution, so ordering (D5) and exactly-once (D2) are enforced in one place. Recursion depth on repeated synchronous recovery failures is bounded by `MAX_DEFERRAL_DEPTH` (10).

---

### D4: Register the pending operation before validation; synchronous readAttribute uses the same callback path

**Decision:** `ExecuteRequestCommand` creates the parking promise and `PendingOperation`, arms the hop via `ArmHop`, and only then resolves the endpoint and decodes the TLV. Every subsequent failure calls `FailPendingOperation`. ID allocation uses `try_emplace` in a loop, removing the duplicate-ID failure site. `SendCommandWithCallbacks` returns `CHIP_ERROR`, and its lambdas capture `(pendingId, generation)`.

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp
void SpecBasedMatterDeviceDriver::ExecuteRequestCommand(std::forward_list<std::promise<bool>> &promises,
                                                        MatterDevice &device,
                                                        ResultTerminal::RequestCommand &cmd,
                                                        const HandlerContext &hctx,
                                                        char **readValue,
                                                        char **executeResponse,
                                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                                        const chip::SessionHandle &sessionHandle,
                                                        const OperationContext *opCtx)
{
    promises.emplace_front();
    auto &parkingPromise = promises.front();

    PendingOperation pending;
    pending.parkingPromise = &parkingPromise;
    pending.handlerContext = hctx;
    pending.device = GetDevice(device.GetDeviceId());
    pending.exchangeMgr = &exchangeMgr;
    pending.sessionHandle.Grab(sessionHandle);
    pending.readValue = readValue;
    pending.executeResponse = executeResponse;
    pending.overallDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ResolveOverallTimeoutMs(cmd));
    pending.deferralDepth = 0;
    SeedOperationContext(pending, opCtx);

    uint64_t pendingId;
    std::map<uint64_t, PendingOperation>::iterator it;
    bool inserted = false;

    do
    {
        pendingId = nextPendingId++;
        std::tie(it, inserted) = pendingOperations.try_emplace(pendingId, std::move(pending));
    } while (!inserted);

    PendingOperation &stored = it->second;
    stored.id = pendingId;
    metrics.RecordDeferredStart(static_cast<int64_t>(pendingOperations.size()));

    ArmHop(stored, cmd.onResponse, cmd.onError);
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
        stored.context = std::move(cmd.context);
    }

    SendArmedRequestCommand(stored, device, cmd.clusterId, cmd.commandId, cmd.timedInvokeTimeoutMs,
                            cmd.endpointId, cmd.tlvBase64, sessionHandle);
}

/**
 * Resolve, decode, and send for an armed requestCommand hop. Shared by the initial park and the
 * re-arm in ContinueDeferredChain. On any failure, routes to FailPendingOperation; `stored` may be
 * erased on return.
 */
void SpecBasedMatterDeviceDriver::SendArmedRequestCommand(PendingOperation &stored,
                                                          MatterDevice &device,
                                                          chip::ClusterId clusterId,
                                                          chip::CommandId commandId,
                                                          std::optional<uint16_t> timedInvokeTimeoutMs,
                                                          std::optional<chip::EndpointId> requestedEndpoint,
                                                          const std::string &tlvBase64,
                                                          const chip::SessionHandle &sessionHandle)
{
    const uint64_t pendingId = stored.id;
    const uint32_t generation = stored.hopGeneration;

    auto endpointOpt = ResolveEndpoint(device, clusterId, requestedEndpoint);

    if (!endpointOpt.has_value())
    {
        FailPendingOperation(pendingId,
                             DeferredFailure::Internal(StringPrintf("No endpoint for cluster 0x%x", clusterId)),
                             Recovery::Allowed);
        return;
    }

    auto decodedTlv = DecodeBase64Tlv(tlvBase64);

    if (!decodedTlv.has_value())
    {
        FailPendingOperation(pendingId, DeferredFailure::Internal("Invalid TLV payload"), Recovery::Allowed);
        return;
    }

    CHIP_ERROR err = device.SendCommandWithCallbacks(
        clusterId,
        commandId,
        timedInvokeTimeoutMs,
        *endpointOpt,
        decodedTlv->empty() ? nullptr : decodedTlv->data(),
        decodedTlv->size(),
        *stored.exchangeMgr,
        sessionHandle,
        [this, pendingId, generation](const chip::app::ConcreteCommandPath &path, chip::TLV::TLVReader *data) {
            HandleDeferredCommandResponse(pendingId, generation, path, data);
        },
        [this, pendingId, generation](CHIP_ERROR error) { HandleDeferredCommandError(pendingId, generation, error); });

    if (err != CHIP_NO_ERROR)
    {
        FailPendingOperation(pendingId,
                             DeferredFailure::Transport(std::string("Send failed: ") + err.AsString(), err),
                             Recovery::Allowed);
    }
}
```

`StringPrintf` stands in for whatever formatting helper the file already uses (e.g. a `snprintf` into a local buffer); the implementer should use the existing idiom.

`ContinueDeferredChain`'s `RequestCommand` branch becomes: depth check → `ArmHop` → if depth exceeded, `FailPendingOperation(Internal("Max deferral depth…"), Forbidden)` → session check (`Transport("Session expired")`) → `SendArmedRequestCommand`. The device-removed check moves *after* arming for deferred terminals so it reports to the newly armed hop's `onError`. Its `ReadAttribute` branch arms first and routes endpoint/TLV-copy failures to `FailPendingOperation` (the cache-miss branch already invokes `onError`; it switches to `ConsumeHop` + `ReadFailed`).

`HandleDeferredCommandResponse` / `HandleDeferredCommandError` gain a `uint32_t generation` parameter and return early (debug log) when `pending.hopGeneration != generation || !pending.hopArmed`.

For the initial synchronous `ExecuteReadAttribute` (no `PendingOperation`), endpoint-resolution and TLV-copy failures are folded into the existing cache-miss branch: compute a `std::optional<DeferredFailure>`, and if set, invoke `ra.onError` with it (consuming `ra.onResponse`/`ra.onError`/`ra.context`), then continue with its result via `ExecuteTerminal` exactly as the cache-miss path does today; `FailOperation(promises)` remains only for "no `onError` supplied" and "`onError` returned no result".

`MatterDevice::SendCommandWithCallbacks` changes its return type from `bool` to `CHIP_ERROR`, returning the specific error at each existing `return false` (`CHIP_ERROR_INVALID_ARGUMENT` for TLV structure errors, `CHIP_ERROR_NO_MEMORY` for allocation, and the propagated `err` from `PrepareCommand`/`CopyElement`/`FinishCommand`/`SendCommandRequest`).

**Alternative considered:** Keep validation before registration and invoke `cmd.onError` directly from the terminal on early failure. Rejected — creates a second invocation path with its own recovery/ordering logic, which is exactly the duplication that caused this bug.

**Trade-off:** A pending record (and a `RecordDeferredStart`) is now created even for operations that fail validation; metrics will count them as started-then-failed, which is more accurate.

---

### D5: Ordering — `onError` before caller unblock, all on the Matter thread

**Decision:** `FailPendingOperation` invokes `onError` and executes its ops *before* `ResolvePendingOperation` sets the parking promise. All of this runs on the Matter event-loop thread (the thread that runs `ExecuteTerminal` inside `ConnectAndExecute` and the `CommandSender` callbacks). When the waiting resource operation returns `false` to the core service, the driver's rollback writes (`setTransientData` etc.) are already applied.

Because the parking promise is pushed to the *front* of `promises` (same as today), the pre-send case resolves it before `ExecuteTerminal` returns; no `FailOperation(promises)` is pushed for registered operations, so the list never contains both a pre-failed promise and an unresolved parking promise.

**Alternative considered:** Unblock the caller first, then post `onError` to the event loop. Rejected — a client retrying immediately (camera `localSdp`) would race the rollback, reproducing the stranded-session bug.

## Data Storage

No persistent storage changes. In-memory `PendingOperation` gains two fields:

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h  (struct PendingOperation)
        /** Incremented each time a hop is armed; device-layer callbacks carry the value they were issued for. */
        uint32_t hopGeneration = 0;

        /** True while the current hop's onResponse/onError have not been consumed. */
        bool hopArmed = false;
```

## Data Structures

`DeferredFailure` (D1) is the only new structure. The JS-facing `args.error` shape is unchanged:

```text
args.error = {
  message:    string,        // human-readable reason
  type:       "timeout" | "commandFailed" | "readFailed" | "transport" | "internal",
  matterCode: number | null  // CHIP_ERROR integer when available
}
```

## Interfaces

### SBMD driver contract (`docs/SBMD.md`)

| Callback | When invoked | Guarantee |
|---|---|---|
| `onResponse` | Matching successful response (requestCommand) / cache hit (readAttribute) | At most once per hop; never together with `onError` |
| `onError` | Any failure after the hop is armed: pre-send local error, send rejected, session expired, device removed, depth exceeded, device error, deadline, cache miss | At most once per hop; never together with `onResponse`; always runs before the resource operation's caller is unblocked |

Recovery: a non-`error` terminal returned from `onError` continues the chain for `internal`, `transport`, `commandFailed`, and `readFailed` failures; it is ignored (operation fails) for `timeout` and max-depth failures.

### Internal C++

| Symbol | Change |
|---|---|
| `MatterDevice::SendCommandWithCallbacks` | Returns `CHIP_ERROR` instead of `bool` |
| `SpecBasedMatterDeviceDriver::CompletePendingOperation` | Renamed `ResolvePendingOperation`; failure-with-armed-hop safety net |
| `SpecBasedMatterDeviceDriver::FailPendingOperation` | New choke point |
| `SpecBasedMatterDeviceDriver::ArmHop` / `ConsumeHop` / `SendArmedRequestCommand` | New helpers |
| `HandleDeferredCommandResponse` / `HandleDeferredCommandError` | Gain `uint32_t generation` parameter |
| `SpecBasedMatterDeviceDriverMetrics` | Adds `RecordDeferredFailureCallback(driver, type)` and `RecordDeferredFailureBypass(driver)` |

### Architecture

```text
 Core service (GLib thread)               Matter event loop thread
 ─────────────────────────                ───────────────────────────────────────────────────────────
 deviceService ──execute/read──► MatterDeviceDriver::ConnectAndExecute ──► SpecBasedMatterDeviceDriver
      ▲                                                                      │ ExecuteResource / HandleResourceOp
      │ waits on promises                                                    ▼
      │                                                               ExecuteTerminal
      │                                       ┌──────────────────────────────┼─────────────────────────────┐
      │                                       ▼                              ▼                             ▼
      │                           ExecuteRequestCommand            ExecuteReadAttribute         sendCommand/writeAttribute
      │                             register + ArmHop               (sync; onError on miss,       (no onError; FailOperation)
      │                                   │                          endpoint, TLV copy)
      │                                   ▼
      │                         SendArmedRequestCommand ──CHIP_ERROR──┐
      │                                   │ ok                        │
      │                                   ▼                           │
      │                  MatterDevice::SendCommandWithCallbacks       │
      │                    (pendingId, generation) lambdas            │
      │                      │ OnResponse        │ OnError            │
      │                      ▼                   ▼                    ▼
      │       HandleDeferredCommandResponse  HandleDeferredCommandError     ContinueDeferredChain failures
      │        (gen check, ConsumeHop,         (gen check)                  (depth, device gone, session,
      │         onResponse)                        │                         endpoint, TLV, send)
      │              │ deadline                    │                                │
      │              └──────────────┬──────────────┴────────────────────────────────┘
      │                             ▼
      │                 ┌──────────────────────────────┐
      │                 │  FailPendingOperation        │  ◄── single choke point
      │                 │   ConsumeHop → onError(args) │
      │                 │   ExecuteOps (driver rollback)
      │                 │   recovery? → ContinueDeferredChain
      │                 └──────────────┬───────────────┘
      │                                ▼
      │                 ResolvePendingOperation(id,false)  (safety net if hop still armed)
      └──────────────── parkingPromise.set_value(false)
```

### GObject / public API

None. No signals, properties, GIR types, or `api/c/public/` headers change. Public API backward compatibility is unaffected; the only observable change is that SBMD drivers' `onError` now runs in cases where it previously did not, which is the documented contract.

## Implementation Detail

### Thread safety

`pendingOperations`, `nextPendingId`, and the new `hopGeneration`/`hopArmed` fields are Matter-thread-confined per the existing invariant in `SpecBasedMatterDeviceDriver.h`; every new entry point (`FailPendingOperation`, `ArmHop`, `ConsumeHop`, `SendArmedRequestCommand`) is only called from executors and `CommandSender` callbacks already on that thread, so no new locking is required. `SafeJSValue` moves and destruction continue to happen under `MQuickJsRuntime`'s mutex (`ArmHop`, `ConsumeHop`, and the `callbacks = {}` reset inside the locked block). `onError`'s ops are executed *outside* the JS mutex, matching the existing `HandleDeferredCommandError`, because `ExecuteOps` → `MakeTransientSetter` may re-enter the driver.

Re-entrancy: `onError` may return a deferred terminal. `ContinueDeferredChain` arms a new hop (generation +1) and may synchronously fail again, re-entering `FailPendingOperation`. This is safe because (a) `ConsumeHop` cleared the previous hop before invocation, so it cannot fire twice; (b) `std::map` node references stay valid until `erase`, and the only `erase` is in `ResolvePendingOperation`, after which every caller returns immediately without touching `pending`; (c) recursion is bounded by `MAX_DEFERRAL_DEPTH`, whose violation uses `Recovery::Forbidden`.

Promise double-set: the parking promise is set only in `ResolvePendingOperation`, which erases the record in the same call; a second resolve finds no record. The existing `try/catch std::future_error` stays as defense. For registered operations no `FailOperation(promises)` is pushed, so `ConnectAndExecute` sees a single outcome.

The core-service thread only waits on futures; it never touches pending state.

### Metrics

Add two counters to `SpecBasedMatterDeviceDriverMetrics` following the existing `RecordDeferredTimeout` pattern: `sbmd.deferred.failure_callback` (attributes: driver, type) and `sbmd.deferred.failure_bypass` (attribute: driver). The bypass counter should stay at zero; a non-zero value indicates a new site that skipped the choke point.

## Migrations

Implementation order: (1) `MatterDevice::SendCommandWithCallbacks` return type; (2) `DeferredFailure`, `PendingOperation` fields, `ArmHop`/`ConsumeHop`, `FailPendingOperation`, rename to `ResolvePendingOperation` with safety net; (3) convert `HandleDeferredCommandError`, deadline branch, and generation checks; (4) restructure `ExecuteRequestCommand` + `SendArmedRequestCommand`; (5) `ContinueDeferredChain` re-arm branches and depth check; (6) `ExecuteReadAttribute`; (7) docs. No data migration; no driver version bumps.

## Testing Philosophy

### Unit: failure classes (gtest, `core/test/src/SbmdDeferredFailureTest.cpp`)

Use the `SbmdDriverTestBase` / `TestableMatterDevice` harness with a minimal inline SBMD spec whose `onError` records `args.error` into transient data and whose execute handler persists a marker before parking. For each failure class — endpoint not found, invalid base64/TLV, send rejected (`SendCommandWithCallbacks` returns an error via a test hook), session expired, device removed mid-chain, max deferral depth, readAttribute endpoint missing, readAttribute TLV copy failure, plus existing timeout / `commandFailed` / `readFailed` — assert: `onError` ran exactly once, `args.error.type`/`matterCode` match the D1 table, the operation's future resolved `false`, the rollback write is visible when the future resolves, and `pendingOperations` is empty.

### Unit: exactly-once and stale callbacks

Drive `HandleDeferredCommandResponse` then `HandleDeferredCommandError` for the same generation and assert only `onResponse` ran. Re-arm a hop and deliver an error with the old generation; assert it is ignored. Have `onError` return a recovery `requestCommand` that also fails pre-send; assert each hop's `onError` ran once and recursion terminated. Verify `onError` recovery success resolves `true`.

### Unit: safety net

Expose a test-only call to `ResolvePendingOperation(id, false)` with an armed hop and assert `onError` runs once and the bypass metric increments.

### Unit: camera stranded session (`core/test/src/SbmdCameraWebrtcTest.cpp`)

Load `camera.sbmd.js`, post `localSdp` with the send hook forced to fail, and assert `handleVideoStreamAllocateError` ran exactly once (observable as a `webrtcError` resource update carrying the session id) before the operation returned failure. Note that the current `handleVideoStreamAllocateError` does not reset the session out of `'streaming'`; asserting that a second `localSdp` does not adopt the stranded session requires the camera driver rollback from PR #285, so that assertion is added (and initially skipped with a reference to PR #285) rather than changing the driver here.

### Integration (Docker)

If the matter.js camera mock in `testing/mocks/devices/matterjs` can be induced to make `SendCommandRequest` fail (e.g. by dropping the session between `localSdp` posts), add a pytest reproducing two consecutive `localSdp` posts and asserting only one `VideoStreamAllocate` is observed by the mock. If no reliable trigger exists, the unit test above is the regression gate and the integration test is deferred.

## Documentation Plan

### `docs/SBMD.md`

**Audience:** SBMD driver authors.

In §5.1, replace the `args.error.type` description with the full vocabulary (`timeout`, `commandFailed`, `readFailed`, `transport`, `internal`) and when each occurs. In §6.2 step 5 and the `requestCommand`/`readAttribute` reference tables, state the guarantee: exactly one of `onResponse`/`onError` per deferred hop, on every path including failures before the command is sent, and `onError` completes before the resource operation returns. Note the residual gap for plain `sendCommand`/`writeAttribute` returned from `onResponse`, and when an `onError` recovery terminal is honored.

### `openspec/specs/sbmd-v4-runtime/spec.md`

**Audience:** Maintainers. Updated via this change's delta spec on archive.

## Risks / Trade-offs

### Drivers whose onError now runs unexpectedly

**Risk:** A driver `onError` written assuming it only runs after a device round-trip (e.g. assumes `args.error.matterCode` is non-null) may misbehave on `internal` failures.

**Mitigation:** `matterCode` was already nullable for `timeout`/`readFailed`. Audit all in-tree `*.sbmd.js` `onError` handlers as a task; docs list every type.

### Recovery terminals on pre-send failure loop

**Risk:** An `onError` that unconditionally retries the same `requestCommand` against a missing endpoint recurses synchronously.

**Mitigation:** Each recovery re-arm increments `deferralDepth`; at `MAX_DEFERRAL_DEPTH` the hop fails with recovery forbidden. Stack depth is bounded at ~10 frames of executor recursion.

### Generation check hides legitimate late callbacks

**Risk:** A real device response arriving after a re-arm is dropped.

**Mitigation:** After a re-arm, the previous hop's `CommandSender` has already delivered its outcome (that is what triggered the re-arm); any further callback for it is a duplicate by definition.

### Residual gap for consumed hops

**Risk:** `onResponse` → plain `sendCommand` pre-send failure still does not reach driver code.

**Mitigation:** Documented; drivers needing rollback use `requestCommand`. BARTON-420 cleanup hooks will close it generally.
