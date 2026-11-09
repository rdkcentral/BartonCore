# Design: SBMD Transaction Orchestrator (BARTON-423)

Introduce `SbmdTransaction`, `SbmdTransactionOrchestrator`, and `SbmdWorker` so that every SBMD request is a first-class object that owns one data-access session (through the new internal `SbmdDataAccess` seam), records every device operation in an operation log, runs each step with freshly fetched supplements, obeys a single wait-direction rule across the orchestrator, the device-service main loop, and the Matter thread, and ends in exactly one commit or one abort plus failure hand-off.

## Context

The epic-wide decisions (D1–D10) and glossary are recorded in `openspec/changes/sbmd-dal-core/design.md`. The terms used here are:

- **SBMD Transaction** — the driver-level execution of one request, from the initial handler invocation through every parked device operation and deferred callback, to the terminal result and, on failure, the failure handler. Code: `SbmdTransaction`.
- **DAL Transaction** — the caller-agnostic data-access unit: per-key reads routed to plugins, staged writes, commit or abort. Code: `DalTransaction` (BARTON-421).
- One SBMD Transaction owns exactly one data-access session. This change lands **first** in the epic (delivery order 412 fix → **423** → 421 → 426 → 422 → 424 → 425, epic D13), before any DAL exists, so the session is an `SbmdDataSession` obtained from the internal seam `SbmdDataAccess` (D11). This change ships `LegacySbmdDataAccess`, which wraps today's fetchers, setters, and `transientStore`. BARTON-421 + 426 later supply `DalSbmdDataAccess`, whose session wraps exactly one `DalTransaction`; from then on "one SBMD Transaction owns one DAL Transaction" holds literally. Nothing in the orchestrator changes when the implementation is swapped.

### Current execution path

A device-service resource call (`readResource`, `writeResource`, `executeResource` in `core/deviceDrivers/matter/MatterDeviceDriver.cpp`) calls `MatterDeviceDriver::ConnectAndExecute` on the calling thread. `ConnectAndExecute` uses `RunOnMatterSync` to start `GetConnectedDevice` on the Matter thread, waits on `connectPromise` until `abortAtTime` (`MATTER_ASYNC_DEVICE_TIMEOUT_SECS` = 15 s), then waits on every promise the work lambda pushed into `promises`. The work lambda runs on the Matter thread from the connection callback.

For SBMD drivers the work lambda reaches `SpecBasedMatterDeviceDriver::HandleResourceOp`, which on the Matter thread:

1. Prefetches supplements (`SbmdHandlerInvoker::PrefetchSupplements` with `MakeAttrFetcher` → device data cache, `MakeResFetcher` → `deviceServiceGetResourceById`, `MakePersistFetcher` → `deviceServiceGetMetadata`, `MakeTransientFetcher` → Matter-thread-confined `transientStore`).
2. Invokes the JS handler under the `MQuickJsRuntime` mutex.
3. Applies ops immediately with `SbmdHandlerInvoker::ExecuteOps` (`updateResource`, `setMetadata`, `deviceServiceSetMetadata`, `SetTransientData`) — even when the terminal is `error`.
4. Executes the terminal in `ExecuteTerminal`: `success`/`error` resolve synchronously; `sendCommand`/`writeAttribute` push a promise that `MatterDevice` resolves on completion; `requestCommand` registers a `PendingOperation` in `pendingOperations`, pushes a *parking promise*, and sends with `SendCommandWithCallbacks`; `readAttribute` reads the **device data cache** synchronously (no device round trip) and calls `onResponse`/`onError` inline.

Device responses arrive on the Matter thread in `HandleDeferredCommandResponse`/`HandleDeferredCommandError`, which call the JS callback inline, apply its ops, and `ContinueDeferredChain`. A chained `sendCommand`/`writeAttribute` from a deferred callback is dispatched with a throw-away `tempPromises` list and the parked operation is completed **successfully without waiting for the device status**. The overall deadline (`PendingOperation::DEFAULT_OVERALL_TIMEOUT_MS` = 30 s, or `timeoutMs`, or `matter.defaultTimeoutMs`) is checked only when a response arrives; there is no timer, and the caller's own 15 s `abortAtTime` usually fires first, after which nothing invokes `onError`.

Device-initiated handlers (`HandleAttributeReport`, `HandleEvent`, `HandleCommand` → `DispatchToHandlers`) run inline on the Matter thread from `CacheCallback`/incoming command handlers. They prefetch supplements, run JS, apply ops, and ignore any outbound terminal with a warning.

Seeds run from `SeedInitialResourceValues`, reached from `DoSynchronizeDevice` (inside a `ConnectAndExecute` work lambda) and from `DoRegisterDriverResources` (inside `RunOnMatterSync`).

BARTON-412 (`sbmd-deferred-failure-callbacks`) adds `FailPendingOperation(pendingId, failure, recovery)`, registers the pending record before validation, adds hop generations to reject stale callbacks, and guarantees exactly one of `onResponse`/`onError` per hop. This change keeps those guarantees and moves them into `SbmdTransaction`.

### Threads

| Thread | Identity | Owns today |
| --- | --- | --- |
| Caller | Any device-service API thread (IPC, client, or the main loop itself) | Blocks in `ConnectAndExecute` |
| Main loop | Default `GMainContext`, run on `glibEvt` (`startMainLoop`, `core/src/deviceService.c`) | Device-service state; after BARTON-421, all DAL calls (via `DalSbmdDataAccess`) |
| Matter | CHIP event loop | Matter stack, device data cache, JS invocation, `transientStore` (until BARTON-426 replaces it; `LegacySbmdDataAccess` reaches it only by scheduling onto this thread) |

## References

- `openspec/changes/sbmd-dal-core/design.md` — Epic architecture, glossary (D1–D13), the later `DalSbmdDataAccess` implementation of this change's seam, `DalTransaction` (`Read`, `Stage`, `StageConditional`, `AllocateSequence`, `SetOverlay`, `Commit`, `Abort`). Not a dependency of this change.
- `openspec/changes/sbmd-dal-source-plugins/` — Plugins (transient plugin replaces `transientStore`), attribute plugin overlay. Not a dependency of this change.
- Branch `BARTON-412-deferred-failure-callbacks`, `openspec/changes/sbmd-deferred-failure-callbacks/` — Exactly-once `onError`, failure types `transport`/`internal`, hop generations.
- [GLib `g_main_context_invoke_full`](https://docs.gtk.org/glib/method.MainContext.invoke_full.html) — Runs a function in a context, inline when the caller already owns it; used for main-loop hops.
- [GLib `g_main_context_is_owner`](https://docs.gtk.org/glib/method.MainContext.is_owner.html) — Determines whether the orchestrator is the main loop and DAL calls can run inline.
- [`std::future::wait_until`](https://en.cppreference.com/w/cpp/thread/future/wait_until) — Deadline-bounded waits used for every hop.
- [Matter SDK `PlatformManager::ScheduleWork`](https://github.com/project-chip/connectedhomeip/blob/master/src/include/platform/PlatformManager.h) — Posting work to the Matter thread without blocking it.
- `docs/SBMD.md` — Driver-facing contract for result chains, deferred handlers, and error types.
- `docs/barton-device-service-architecture.md` — Project architecture document (there is no `docs/dev/architecture.md`).

## Goals / Non-Goals

**Goals:**

- `SbmdTransaction` replaces `PendingOperation`; one object per SBMD request, owning one `SbmdDataSession` and an operation log.
- Internal seam `SbmdDataAccess` (batched supplement fetch including sequence-allocation entries, stage, stage-conditional, commit, abort, attribute-write overlay hook, abort listener) with `LegacySbmdDataAccess` over today's data paths; no dependency on BARTON-421/426.
- `SbmdTransactionOrchestrator` drives every step from the caller thread with one batched data-access fetch per step.
- `onResponse`/`onError` are no longer run inline on the Matter thread from SDK callbacks; outcomes are handed back to the orchestrator.
- `SbmdWorker` orchestrates attribute/event/command handler dispatch.
- Wait-direction rule enforced by construction; main-loop work runs inline when the orchestrator is the main loop.
- Deadlines on every wait; overall transaction deadline enforced by the orchestrator.
- Commit on success; abort, staged-write discard, and failure hand-off on failure; `commitFailed` and `aborted` failure types.
- Every device operation's outcome is awaited and logged, including those returned from deferred callbacks.
- Successful `writeAttribute` values recorded through the seam's attribute-write overlay hook.
- Recovery rule (epic D12): an `onError` whose non-`error` terminal is honored recovers the transaction.

**Non-Goals:**

- DAL core, plugins, cross-plugin commit rollback, `DalSbmdDataAccess` (BARTON-421/426, delivered after this change).
- Exposing sequence allocation or conditional writes to JavaScript (BARTON-422). The seam carries them so later changes need no orchestrator change.
- `mergeTransientData`, `supplements` override option (BARTON-422).
- `onTransactionFailure` (BARTON-424); only the hand-off point is defined here.
- Optimistic concurrency, versions, retries, merges (removed by epic D3/D4).
- New result-builder terminals.
- Public API or GObject changes.

## Decisions

### D1: `SbmdTransaction` is the single owner of request state

**Decision:** Every SBMD request is represented by one heap-allocated `SbmdTransaction` held by `std::shared_ptr`. It replaces `PendingOperation`, `pendingOperations`, and `nextPendingId`. It owns the `DalTransaction`, the operation log, the current hop (JS callbacks and context as `SafeJSValue`, hop generation), deferral depth, the overall deadline, output slots (`readValue`/`executeResponse`), and an exactly-once completion latch. SDK callbacks capture a `std::weak_ptr<SbmdTransaction>` plus the hop generation; a callback for a superseded hop or a finished transaction is dropped. This absorbs BARTON-412's `FailPendingOperation` semantics: the latch guarantees that exactly one of commit or fail happens, and the hop-generation check guarantees that exactly one of `onResponse`/`onError` runs per hop.

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdTransaction.h
#pragma once

#include "SbmdOperationLog.h"
#include "../SafeJSValue.h"
#include "../mquickjs/SbmdHandlerInvoker.h"
#include "SbmdDataAccess.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace barton
{
    class MatterDevice;

    /** Failure classification surfaced to onError (per hop) and onTransactionFailure (per transaction). */
    enum class SbmdFailureType
    {
        Timeout,       // per-hop timeoutMs, overall deadline, or a missed cross-thread wait deadline
        CommandFailed, // device returned a failure status for a command
        ReadFailed,    // attribute not available for readAttribute
        Transport,     // session expired, send rejected by the stack (BARTON-412)
        Internal,      // endpoint resolution, TLV decode, depth limit, handler returned no result (BARTON-412)
        Aborted,       // bound plugin unregistered, device removed, driver deactivated, shutdown
        CommitFailed   // SbmdDataSession::Commit failed (reason e.g. conditionFailed, pluginCommitFailed)
    };

    const char *ToString(SbmdFailureType type);

    struct SbmdFailure
    {
        uint32_t step = 0;
        SbmdFailureType type = SbmdFailureType::Internal;
        std::string message;
        std::optional<int32_t> matterCode;
    };

    enum class SbmdTransactionOrigin
    {
        ResourceRead,
        ResourceWrite,
        ResourceExecute,
        Seed,
        AttributeReport,
        Event,
        UnsolicitedCommand,
        Compensation // BARTON-424: chain returned by onTransactionFailure; failure handling disabled
    };

    enum class SbmdTransactionState
    {
        Running,
        AwaitingDevice,
        Committing,
        Failing,
        Committed,
        Failed
    };

    /** The JS callbacks armed for the current deferred hop. */
    struct SbmdHop
    {
        uint64_t generation = 0;
        SafeJSValue onResponse;
        SafeJSValue onError;
        std::optional<SbmdSupplements> supplementsOverride; // BARTON-422 option; empty = inherit
    };

    class SbmdTransaction : public std::enable_shared_from_this<SbmdTransaction>
    {
    public:
        static constexpr uint32_t kMaxDeferralDepth = 10;
        static constexpr std::chrono::milliseconds kDefaultOverallTimeout {30000};

        SbmdTransaction(uint64_t id,
                        SbmdTransactionOrigin origin,
                        HandlerContext handlerContext,
                        std::unique_ptr<SbmdDataSession> data,
                        std::chrono::steady_clock::time_point deadline,
                        bool failureHandlingEnabled);

        uint64_t Id() const { return id; }
        SbmdTransactionOrigin Origin() const { return origin; }
        const HandlerContext &Context() const { return handlerContext; }
        SbmdDataSession &Data() { return *data; }
        SbmdOperationLog &Log() { return log; }
        const SbmdOperationLog &Log() const { return log; }
        std::chrono::steady_clock::time_point Deadline() const { return deadline; }
        bool FailureHandlingEnabled() const { return failureHandlingEnabled; }
        uint32_t Step() const { return step; }
        uint32_t NextStep() { return ++step; }
        uint32_t DeferralDepth() const { return deferralDepth; }

        /** Arm a new hop. Returns its generation; previous hop callbacks are released. Caller holds the JS mutex. */
        uint64_t ArmHop(SafeJSValue &&onResponse,
                        SafeJSValue &&onError,
                        std::optional<SbmdSupplements> supplementsOverride);

        /** True when generation matches the armed hop and the transaction is not finished. */
        bool IsCurrentHop(uint64_t generation) const;

        /** Disarm the current hop so no later callback can reach it. Returns the callbacks for one-time use. */
        SbmdHop TakeHop();

        /** The opaque driver context established when the chain first parks; constant for the transaction. */
        SafeJSValue &DriverContext() { return driverContext; }

        /**
         * Exactly-once terminal latch. Returns true for the first caller only; every later call returns false.
         * Absorbs BARTON-412 FailPendingOperation/CompletePendingOperation exactly-once semantics.
         */
        bool TryFinish(SbmdTransactionState terminalState);

        SbmdTransactionState State() const { return state.load(); }
        void SetState(SbmdTransactionState s) { state.store(s); }

        /** Set by an abort source (plugin unregister, device removal, driver deactivate, shutdown). */
        void RequestAbort(std::string reason);
        std::optional<std::string> AbortRequested() const;

        // Output slots for resource operations (non-owning, valid while the caller is blocked).
        char **readValue = nullptr;
        char **executeResponse = nullptr;
        std::optional<std::string> successValue;

        std::weak_ptr<MatterDevice> device;
        OperationContext operationCtx;
        std::optional<SbmdFailure> failure;

    private:
        const uint64_t id;
        const SbmdTransactionOrigin origin;
        const HandlerContext handlerContext;
        std::unique_ptr<SbmdDataSession> data; // LegacySbmdDataSession holds staged writes here
        SbmdOperationLog log;
        SbmdHop hop;
        SafeJSValue driverContext;
        const std::chrono::steady_clock::time_point deadline;
        const bool failureHandlingEnabled;
        uint32_t step = 0;
        uint32_t deferralDepth = 0;
        std::atomic<SbmdTransactionState> state {SbmdTransactionState::Running};
        std::atomic<bool> finished {false};
        mutable std::mutex abortMtx;
        std::optional<std::string> abortReason;
    };
} // namespace barton
```

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdTransaction.cpp (excerpt)
uint64_t SbmdTransaction::ArmHop(SafeJSValue &&onResponse,
                                 SafeJSValue &&onError,
                                 std::optional<SbmdSupplements> supplementsOverride)
{
    hop.onResponse = std::move(onResponse);
    hop.onError = std::move(onError);
    hop.supplementsOverride = std::move(supplementsOverride);
    hop.generation++;
    deferralDepth++;
    return hop.generation;
}

bool SbmdTransaction::IsCurrentHop(uint64_t generation) const
{
    return !finished.load() && hop.generation == generation && (hop.onResponse.HasValue() || hop.onError.HasValue());
}

SbmdHop SbmdTransaction::TakeHop()
{
    SbmdHop taken;
    taken.generation = hop.generation;
    taken.onResponse = std::move(hop.onResponse);
    taken.onError = std::move(hop.onError);
    taken.supplementsOverride = std::move(hop.supplementsOverride);
    return taken;
}

bool SbmdTransaction::TryFinish(SbmdTransactionState terminalState)
{
    bool expected = false;

    if (!finished.compare_exchange_strong(expected, true))
    {
        return false;
    }

    state.store(terminalState);
    return true;
}
```

The hop fields are touched only by the orchestrator thread; the `finished` latch and abort request are the only cross-thread members, which is why they are atomic or mutex-protected.

**Alternative considered:** Keep `PendingOperation` and add a data-session pointer and log vector to it. Rejected — the record would still be created only on park, would not exist for synchronous `success`/`sendCommand` terminals or device-initiated dispatch, and would still be mutated from SDK callbacks on the Matter thread.

**Trade-offs:** `shared_ptr` plus `weak_ptr` captures replace numeric ids; the lifetime is slightly harder to reason about, but stale callbacks become impossible to dereference.

---

### D2: The operation log

**Decision:** The SBMD Transaction appends one entry for every device-level operation it requests, in request order, before the operation is attempted. Entries are later updated to `succeeded`, `failed`, or left `notSent`. The shape matches what BARTON-424 exposes as `args.operations`. `readAttribute` is logged with kind `readAttribute` even though the runtime satisfies it from the device data cache, because it is a device-state observation the driver requested.

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdOperationLog.h
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace barton
{
    enum class SbmdOperationKind
    {
        Command,      // sendCommand or requestCommand
        ReadAttribute,
        WriteAttribute
    };

    enum class SbmdOperationStatus
    {
        NotSent,
        Succeeded,
        Failed
    };

    struct SbmdOperationError
    {
        std::string type; // "timeout" | "commandFailed" | "readFailed" | "transport" | "internal" | "aborted"
        std::string message;
        std::optional<int32_t> matterCode;
    };

    struct SbmdOperationRecord
    {
        uint32_t step = 0;
        SbmdOperationKind kind = SbmdOperationKind::Command;
        std::optional<uint16_t> endpoint; // resolved endpoint; empty if resolution failed
        uint32_t cluster = 0;
        std::optional<uint32_t> command;   // set for Command
        std::optional<uint32_t> attribute; // set for ReadAttribute / WriteAttribute
        std::string requestTlvBase64;      // payload as supplied by the driver ("" if none)
        SbmdOperationStatus status = SbmdOperationStatus::NotSent;
        std::optional<std::string> responseTlvBase64; // command response or attribute value
        std::optional<SbmdOperationError> error;
    };

    class SbmdOperationLog
    {
    public:
        /** Append a NotSent record and return its index. */
        size_t Append(SbmdOperationRecord record);

        void MarkSucceeded(size_t index, std::optional<std::string> responseTlvBase64);
        void MarkFailed(size_t index, SbmdOperationError error);

        const std::vector<SbmdOperationRecord> &Records() const { return records; }
        bool Empty() const { return records.empty(); }

    private:
        std::vector<SbmdOperationRecord> records;
    };
} // namespace barton
```

**Alternative considered:** Log only operations that reached the device. Rejected — a compensating driver needs to know that step 3 was attempted and never sent (for example endpoint resolution failed) to decide that nothing needs undoing for it.

**Trade-offs:** Payloads are kept as base64 TLV for the life of the transaction; this bounds memory to the transaction's own requests and is released at transaction end.

---

### D3: The orchestrator runs on the calling thread and owns the step loop

**Decision:** `SbmdTransactionOrchestrator::Run(txn, firstStep)` executes on whichever thread owns the transaction: the device-service caller for resource operations and seeds, or `SbmdWorker` for device-initiated work. Each step is:

1. **Plan and fetch.** Build one `SbmdFetchRequest` for the step's supplements (the handler's declared supplements, or the hop's override), including any sequence-allocation entries, and call `SbmdDataSession::Fetch` once. The session performs the work with the correct affinity (D11): with `LegacySbmdDataAccess`, device-service reads run on the orchestrator and `transientStore`/attribute-cache reads are one scheduled hop onto the Matter thread; with `DalSbmdDataAccess`, one hop to the main loop, where the attribute plugin hops to the Matter thread (main loop → Matter is allowed).
2. **Invoke JS on the Matter thread.** Post one closure that builds `args`, attaches supplements, calls the handler (or `onResponse`/`onError`), and parses the result into a `ParsedResult`. The orchestrator waits with the step deadline.
3. **Stage ops.** Execute `log` ops immediately on the orchestrator. Convert data ops (`updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, and later BARTON-422's `mergeTransientData`/conditional writes) into `SbmdWrite`s and call `SbmdDataSession::Stage` (or `StageConditional`). The legacy session only appends to the transaction-held staged list; nothing is applied before commit.
4. **Terminal.** `success` → commit. `error` → fail; the failure type is the type of the hop error being handled when the terminal came from `onError` (for example `commandFailed`), otherwise `internal`, and the message is the driver's error message. `sendCommand`/`writeAttribute`/`requestCommand`/`readAttribute` → append an operation-log record, run the device operation on the Matter thread (D5), and wait for the outcome posted back to the orchestrator. `sendCommand`/`writeAttribute` success → commit; `requestCommand`/`readAttribute` → arm the hop and loop to step 1 with the response or error as the trigger.
5. **Finish.** `SbmdDataSession::Commit` on success, or `Abort` and hand off on failure (D7). D12 recovery is applied before step 5 (D12).

Because fetch, JS, and commit are separate hops, two SBMD Transactions on the same device can interleave before any park (epic D11). Interleaving safety comes from fresh reads plus the atomic primitives carried by the seam (sequence allocation in the fetch, conditional writes at commit); there is no per-device serialization, no version check, and no automatic retry (epic D3 as restated by D11).

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdTransactionOrchestrator.h
#pragma once

#include "SbmdTransaction.h"
#include "ThreadHop.h"

#include <functional>
#include <memory>

namespace barton
{
    class SpecBasedMatterDeviceDriver;

    /** What starts a step: the initial handler, or a device outcome feeding onResponse/onError. */
    struct SbmdStepTrigger
    {
        enum class Kind
        {
            Initial,
            CommandResponse,
            AttributeValue,
            Error
        } kind = Kind::Initial;

        const SbmdHandler *handler = nullptr;           // Initial only
        std::function<SafeJSValue(JSContext *)> buildArgs; // Initial only; builds the trigger args
        uint32_t clusterId = 0;
        uint32_t elementId = 0;                         // commandId or attributeId
        std::string tlvBase64;
        SbmdFailure error;                              // Error only
        bool deliverToOnError = false;                  // Error only: true when an armed hop's onError runs next
    };

    struct SbmdTransactionResult
    {
        bool success = false;
        std::optional<SbmdFailure> failure;
    };

    class SbmdTransactionOrchestrator
    {
    public:
        SbmdTransactionOrchestrator(SpecBasedMatterDeviceDriver &driver, ThreadHop &hop);

        /**
         * Run the SBMD Transaction to completion on the calling thread. Blocks; every internal wait is
         * bounded by txn->Deadline(). Never call from the Matter thread.
         */
        SbmdTransactionResult Run(const std::shared_ptr<SbmdTransaction> &txn, SbmdStepTrigger first);

    private:
        std::optional<ParsedResult> InvokeStep(SbmdTransaction &txn,
                                               const SbmdStepTrigger &trigger,
                                               const FetchedSupplements &supplements);
        bool StageOps(SbmdTransaction &txn, const std::vector<ResultOp> &ops);
        std::optional<SbmdStepTrigger> ExecuteDeviceTerminal(SbmdTransaction &txn, ResultTerminal &terminal);
        SbmdTransactionResult Commit(SbmdTransaction &txn);
        SbmdTransactionResult Fail(SbmdTransaction &txn, SbmdFailure failure);

        SpecBasedMatterDeviceDriver &driver;
        ThreadHop &hop;
    };
} // namespace barton
```

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdTransactionOrchestrator.cpp (step loop)
SbmdTransactionResult SbmdTransactionOrchestrator::Run(const std::shared_ptr<SbmdTransaction> &txn,
                                                       SbmdStepTrigger trigger)
{
    assert(!hop.IsMatterThread());

    for (;;)
    {
        uint32_t step = txn->NextStep();

        if (auto reason = txn->AbortRequested())
        {
            return Fail(*txn, {step, SbmdFailureType::Aborted, *reason, std::nullopt});
        }

        if (std::chrono::steady_clock::now() >= txn->Deadline())
        {
            return Fail(*txn, {step, SbmdFailureType::Timeout, "Overall transaction deadline exceeded", std::nullopt});
        }

        // (1) One batched fetch (reads + sequence allocations) for this step's supplements.
        const SbmdSupplements &declared = driver.SupplementsForStep(*txn, trigger);
        SbmdFetchRequest request = SbmdHandlerInvoker::PlanFetch(declared, txn->Context(), driver.Aliases());
        auto fetched = txn->Data().Fetch(request, txn->Deadline());

        if (auto *err = std::get_if<SbmdDataError>(&fetched))
        {
            return Fail(*txn, {step, ToFailureType(*err), err->message, std::nullopt});
        }

        // (2) JS on the Matter thread.
        auto supplements = SbmdHandlerInvoker::ToFetchedSupplements(declared, std::get<SbmdFetchResult>(fetched));
        auto parsed = hop.OnMatter(txn->Deadline(), [&] { return InvokeStep(*txn, trigger, supplements); });

        if (!parsed.has_value())
        {
            return Fail(*txn, {step, SbmdFailureType::Timeout, "Handler invocation missed deadline", std::nullopt});
        }

        if (!parsed->has_value())
        {
            return Fail(*txn, {step, SbmdFailureType::Internal, "Handler returned no result", std::nullopt});
        }

        ScopedResultRelease release {*parsed};
        ParsedResult &result = parsed->value();

        // (3) Log now, stage data writes in the data session.
        if (auto err = StageOps(*txn, result.ops))
        {
            return Fail(*txn, {step, ToFailureType(*err), err->message, std::nullopt});
        }

        // (4) Terminal.
        if (auto *ok = std::get_if<ResultTerminal::Success>(&result.terminal.data))
        {
            if (!ok->value.empty())
            {
                txn->successValue = ok->value;
            }

            return Commit(*txn);
        }

        if (auto *err = std::get_if<ResultTerminal::Error>(&result.terminal.data))
        {
            // From onError: keep the hop's failure type (e.g. commandFailed). Otherwise: internal.
            SbmdFailureType type = trigger.kind == SbmdStepTrigger::Kind::Error ? trigger.error.type
                                                                               : SbmdFailureType::Internal;
            std::optional<int32_t> code =
                trigger.kind == SbmdStepTrigger::Kind::Error ? trigger.error.matterCode : std::nullopt;
            return Fail(*txn, {step, type, err->message, code});
        }

        auto next = ExecuteDeviceTerminal(*txn, result.terminal);

        if (!next.has_value())
        {
            // sendCommand / writeAttribute succeeded: nothing to continue.
            return Commit(*txn);
        }

        if (next->kind == SbmdStepTrigger::Kind::Error && !next->deliverToOnError)
        {
            // Non-deferred sendCommand/writeAttribute failure, or a failure that BARTON-412 does not route to
            // onError: no onError exists for this hop.
            return Fail(*txn, next->error);
        }

        trigger = std::move(*next); // (5) loop with fresh supplements for onResponse / onError
    }
}
```

`ExecuteDeviceTerminal` returns `std::nullopt` for a successful `sendCommand`/`writeAttribute`, an `Error` trigger for any failure of a non-deferred terminal (which `Run` turns directly into a transaction failure, preserving BARTON-412's rule that non-deferred terminals have no `onError`), and a `CommandResponse`/`AttributeValue`/`Error` trigger for deferred terminals whose hop was armed. When the trigger is `Error` for an armed hop, the next iteration invokes `onError`; a non-`error` terminal from `onError` continues the chain for `internal`, `transport`, `commandFailed`, and `readFailed`, and is ignored for `timeout` and depth-limit failures, exactly as BARTON-412 specifies.

**Alternative considered:** Run the whole loop on the Matter thread and post only data-access calls asynchronously. Rejected — every data-access result would need a continuation callback on the Matter thread, recreating the callback spaghetti this change removes, and the Matter thread would still need to wait for data-access results whenever a handler needed them synchronously.

**Alternative considered:** Run the loop on the main loop. Rejected — the caller of a resource operation would then block on the main loop, and main-loop code that itself issues resource operations would deadlock.

**Trade-offs:** Two to three cross-thread hops per step instead of zero. Hop latency is measured; batching keeps it to one hop per kind per step.

---

### D4: Thread hops and the wait-direction rule

**Decision:** All cross-thread execution goes through `ThreadHop`, which enforces the rule *orchestrator → main loop → Matter is allowed; the Matter thread never waits on the main loop* by construction. `ThreadHop` is the canonical cross-thread executor for the whole epic: BARTON-421's DAL executor (plugin affinity hops, main-loop confinement) is built on it rather than introducing a second mechanism. Its operations:

- `OnMainLoop(deadline, fn)` runs `fn` inline when `g_main_context_is_owner(g_main_context_default())` is true; otherwise it posts with `g_main_context_invoke_full` and waits on a `std::future` with `wait_until(deadline)`. It asserts it is never called on the Matter thread.
- `OnMatter(deadline, fn)` posts with `chip::DeviceLayer::PlatformMgr().ScheduleWork` (via the existing `RunOnMatter`-style wrapper that avoids the `LambdaBridge` capture limit) and waits with `wait_until(deadline)`. It may be called from the orchestrator or from main-loop code (allowed direction), and runs inline if already on the Matter thread only when called from plugin affinity on behalf of the main loop — never from the orchestrator, which is never the Matter thread.
- `PostToOrchestrator(txn, outcome)` is the only API the Matter thread uses to report device outcomes; it enqueues into the transaction's outcome slot and signals a condition variable. It never blocks.

On deadline expiry the waiting side stops waiting and marks the shared state abandoned; when the late closure runs it observes the abandoned flag and performs no side effects (data-access closures check that the session is still open; JS closures skip invocation). The transaction then fails with `timeout`.

```cpp
// core/deviceDrivers/matter/sbmd/txn/ThreadHop.h
#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>

namespace barton
{
    class ThreadHop
    {
    public:
        using Deadline = std::chrono::steady_clock::time_point;

        virtual ~ThreadHop() = default;

        virtual bool IsMatterThread() const = 0;
        virtual bool IsMainLoopOwner() const = 0;

        /** Run fn on the device-service main loop (inline if already owner). Empty result on deadline miss. */
        template<typename F>
        auto OnMainLoop(Deadline deadline, F &&fn) -> std::optional<decltype(fn())>;

        /** Run fn on the Matter thread. Empty result on deadline miss. Never called from the Matter thread. */
        template<typename F>
        auto OnMatter(Deadline deadline, F &&fn) -> std::optional<decltype(fn())>;

        /** Fire-and-forget post to the main loop. Safe from any thread, including the Matter thread, because nothing waits. Used by 421 (DAL unregister/purge from the Matter thread). */
        void PostMainLoopAsync(std::function<void()> fn) { PostMainLoop(std::move(fn)); }

    protected:
        virtual void PostMainLoop(std::function<void()> fn) = 0;
        virtual void PostMatter(std::function<void()> fn) = 0;
    };

    /** Production implementation: GLib default context + CHIP PlatformManager. */
    class DefaultThreadHop final : public ThreadHop
    {
    public:
        bool IsMatterThread() const override;
        bool IsMainLoopOwner() const override;

    protected:
        void PostMainLoop(std::function<void()> fn) override;
        void PostMatter(std::function<void()> fn) override;
    };

    template<typename F>
    auto ThreadHop::OnMainLoop(Deadline deadline, F &&fn) -> std::optional<decltype(fn())>
    {
        using R = decltype(fn());

        if (IsMainLoopOwner())
        {
            return fn();
        }

        assert(!IsMatterThread() && "Matter thread must never wait on the main loop");

        struct Shared
        {
            std::promise<R> promise;
            std::atomic<bool> abandoned {false};
        };

        auto shared = std::make_shared<Shared>();
        auto future = shared->promise.get_future();

        PostMainLoop([shared, work = std::function<R()>(std::forward<F>(fn))]() mutable {
            if (shared->abandoned.load())
            {
                return;
            }

            shared->promise.set_value(work());
        });

        if (future.wait_until(deadline) != std::future_status::ready)
        {
            shared->abandoned.store(true);
            return std::nullopt;
        }

        return future.get();
    }
} // namespace barton
```

`OnMatter` has the same shape with `PostMatter` and the assertion `!IsMatterThread()`. Captured references in `fn` are only valid while the waiter waits; implementations therefore capture by value or through the `shared_ptr<SbmdTransaction>`, and the abandoned check prevents a late closure from touching a stack that has unwound.

**Alternative considered:** A lock hierarchy with `LockChipStack()` held across data-access calls. Rejected — the Matter stack lock is not reentrant across threads and main-loop code already takes it, so lock ordering would be global and fragile.

**Trade-offs:** Today some main-loop code blocks on the Matter thread (`RunOnMatterSync`, `LockChipStack`). That is the allowed direction and stays legal, but it adds latency to data-access calls made while the Matter thread is busy. An audit is listed as an open question.

---

### D5: Device operations report outcomes back to the orchestrator

**Decision:** Device operations are started on the Matter thread and complete asynchronously. `MatterDevice` gains outcome-callback variants that never run JS:

```cpp
// core/deviceDrivers/matter/MatterDevice.h (additions)
struct DeviceOpOutcome
{
    bool success = false;
    CHIP_ERROR error = CHIP_NO_ERROR;     // transport / IM error when !success
    std::optional<uint16_t> imStatus;     // cluster or IM status when the device rejected the request
    std::optional<std::string> responseTlvBase64;
    std::optional<chip::app::ConcreteCommandPath> responsePath;
};

using DeviceOpOutcomeCallback = std::function<void(DeviceOpOutcome &&)>;

/** Invoke a command; outcomeCb runs exactly once on the Matter thread. Returns the send error, if any. */
CHIP_ERROR InvokeCommand(chip::ClusterId clusterId,
                         chip::CommandId commandId,
                         std::optional<uint16_t> timedInvokeTimeoutMs,
                         chip::EndpointId endpointId,
                         const uint8_t *tlv,
                         size_t tlvLength,
                         chip::Messaging::ExchangeManager &exchangeMgr,
                         const chip::SessionHandle &sessionHandle,
                         DeviceOpOutcomeCallback outcomeCb);

/** Write an attribute; outcomeCb runs exactly once on the Matter thread. */
CHIP_ERROR WriteAttribute(chip::EndpointId endpointId,
                          chip::ClusterId clusterId,
                          chip::AttributeId attributeId,
                          const uint8_t *tlv,
                          uint16_t tlvLength,
                          chip::Messaging::ExchangeManager &exchangeMgr,
                          const chip::SessionHandle &sessionHandle,
                          DeviceOpOutcomeCallback outcomeCb);
```

`ExecuteDeviceTerminal` appends the log record, then calls `hop.OnMatter` with a closure that resolves the endpoint, decodes TLV, obtains the session, and calls `InvokeCommand`/`WriteAttribute` with an outcome callback that captures `weak_ptr<SbmdTransaction>`, the hop generation, and the log index, and calls `PostToOrchestrator`. Pre-send failures (endpoint, TLV, session expired, stack rejection) return synchronously from the closure as `internal`/`transport` errors. The orchestrator then waits on the transaction's outcome slot until `min(per-hop timeoutMs deadline, transaction deadline)`; expiry is a `timeout` error for that hop.

For `requestCommand`, a response command matching `responseCommandId` that arrives through the incoming-command path (`HandleCommand`) is matched against armed transactions for that device before unsolicited dispatch, and is posted to the orchestrator as a `CommandResponse` trigger. For `readAttribute` the closure reads the device data cache on the Matter thread and returns the value or `readFailed` synchronously.

The orchestrator's response path updates the log (`MarkSucceeded` with the response TLV, or `MarkFailed`) before invoking `onResponse`/`onError`.

**Alternative considered:** Keep promises in `ConnectAndExecute`'s `promises` list. Rejected — the list is fixed when the work lambda returns, cannot express "the next step depends on this result", and the deferred chain's `tempPromises` show the problem: chained operations are never awaited.

**Trade-offs:** `MatterDevice` keeps its promise-based APIs for non-SBMD drivers; the new APIs are additive.

---

### D6: Commit and the attribute-write overlay

**Decision:** On success the orchestrator calls `SbmdDataSession::Commit(deadline)`. If commit returns an error, the transaction fails with `commitFailed` and the error's reason (`conditionFailed` for a conditional-write mismatch, epic D11; `pluginCommitFailed`/`writeFailed` otherwise). With `LegacySbmdDataAccess`, commit applies the staged writes in order with their thread affinity and has no cross-store rollback (D11); with `DalSbmdDataAccess`, commit is `DalTransaction::Commit` with epic D8 light rollback. Output slots (`readValue`, `executeResponse`) are written only after a successful commit.

When a `writeAttribute` device operation succeeds, the orchestrator calls `SbmdDataSession::RecordAttributeWrite(endpoint, cluster, attribute, tlvBase64)` before continuing. The legacy session keeps the value in a per-transaction overlay map consulted when it reads attribute supplements; the DAL session forwards it to `DalTransaction::SetOverlay` on the device's attribute-plugin binding (BARTON-426). Either way, later reads of that attribute within the same SBMD Transaction return the written value. The overlay is discarded at transaction end; subscription reports remain the source of truth for the device data cache. There is no automatic undo of an attribute write.

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdTransactionOrchestrator.cpp
SbmdTransactionResult SbmdTransactionOrchestrator::Commit(SbmdTransaction &txn)
{
    if (!txn.TryFinish(SbmdTransactionState::Committing))
    {
        return {false, txn.failure};
    }

    std::optional<SbmdDataError> err = txn.Data().Commit(txn.Deadline());

    if (err.has_value())
    {
        txn.SetState(SbmdTransactionState::Failing);
        SbmdFailure failure {txn.Step(),
                             err->kind == SbmdDataError::Kind::Timeout ? SbmdFailureType::Timeout
                                                                        : SbmdFailureType::CommitFailed,
                             err->Describe(), // e.g. "conditionFailed: transient/sessionOwner"
                             std::nullopt};
        // The latch is already held; FailAfterLatch performs D7's abort + hand-off without re-latching.
        return FailAfterLatch(txn, std::move(failure));
    }

    if (txn.successValue.has_value())
    {
        char **slot = txn.executeResponse != nullptr ? txn.executeResponse : txn.readValue;

        if (slot != nullptr)
        {
            *slot = strdup(txn.successValue->c_str());
        }
    }

    txn.SetState(SbmdTransactionState::Committed);
    metrics.RecordTransactionCommitted(txn.operationCtx, txn.Step(), txn.Log().Records().size());
    return {true, std::nullopt};
}
```

**Alternative considered:** Apply attribute writes to the device data cache directly. Rejected — device data cache plugins are read-only (epic D10) and a later subscription report would race with the injected value.

**Trade-offs:** `sendCommand`'s `successValue` is no longer written optimistically before the device answers; it is written only after the command succeeds and the session commits.

---

### D7: Failure, abort, and the failure-handler hand-off

**Decision:** `Fail(txn, failure)` latches the transaction (`TryFinish(Failing)`), records the failure, calls `SbmdDataSession::Abort` (discarding staged writes and the attribute overlay; allocated sequence values are not returned), records metrics, then calls `driver.OnTransactionFailed(txn)`. In this change `OnTransactionFailed` is a hook that logs the failure and the operation log at `info`. BARTON-424 replaces its body to invoke the driver's `onTransactionFailure`. The ordering is fixed: the failing step's `onError` (if the failure was delivered to an armed hop) has already run as a step of the loop, then the data session is aborted, then the hand-off runs, then the caller is unblocked with failure.

Abort sources call `SbmdTransaction::RequestAbort(reason)` from any thread:

- The data session's abort listener (`SbmdDataSession::SetAbortListener`). `LegacySbmdDataAccess` never fires it on its own (no plugins exist); `DalSbmdDataAccess` fires it when `DalPluginRegistry::Unregister` aborts the bound `DalTransaction` (BARTON-421).
- Device removal (`MatterDeviceDriver::DeviceRemoved` → `SpecBasedMatterDeviceDriver`) aborts every live transaction for that device.
- Driver deactivation and `Shutdown()` abort all live transactions for the driver.

`RequestAbort` also signals the transaction's outcome condition variable so an orchestrator waiting for a device outcome wakes immediately. The orchestrator turns it into `Fail` with type `aborted`. Live transactions are tracked in a per-driver `std::unordered_map<uint64_t, std::weak_ptr<SbmdTransaction>>` guarded by a mutex.

**Alternative considered:** Run `onError` of the current hop on abort. Rejected for `aborted` — the device or driver is going away; BARTON-424's driver-level handler is the single place to react and receives `failure.type == "aborted"`.

**Trade-offs:** Shutdown waits for in-flight orchestrators to observe the abort; bounded by their wait deadlines.

---

### D8: Resource operations, seeds, and `ConnectAndExecute`

**Decision:** SBMD resource operations keep using `ConnectAndExecute` for connection establishment, but the work lambda no longer runs handlers. `MatterDeviceDriver` gains `ConnectAndOrchestrate`, which establishes the session exactly as `ConnectAndExecute` does (same `RunOnMatterSync(GetConnectedDevice)`, same connect deadline), captures the `ExchangeManager`/`SessionHolder` into the transaction, and then runs `SbmdTransactionOrchestrator::Run` on the calling thread with the deadline `min(caller abortAtTime, transaction overall deadline)`. `SpecBasedMatterDeviceDriver` overrides `readResource`/`writeResource`/`executeResource` dispatch to use it. Non-SBMD Matter drivers are unchanged.

The transaction's overall deadline is the earlier of `timeoutMs`/`matter.defaultTimeoutMs`/30 s and the caller's `MATTER_ASYNC_DEVICE_TIMEOUT_SECS`. Today these disagree (15 s caller vs 30 s chain); after this change the caller can never give up while a chain is still running unobserved, because the orchestrator *is* the caller.

Seeds run as SBMD Transactions with origin `Seed`. From `DoSynchronizeDevice` they run on the orchestrator (the synchronize caller). From `DoRegisterDriverResources`, which today runs inside `RunOnMatterSync`, seed invocation moves to after `RunOnMatterSync` returns, on the registering thread.

**Alternative considered:** Replace `ConnectAndExecute` entirely. Rejected — non-SBMD drivers depend on its promise contract.

**Trade-offs:** Two connection paths in `MatterDeviceDriver` until non-SBMD drivers are retired.

---

### D9: `SbmdWorker` orchestrates device-initiated work

**Decision:** `HandleAttributeReport`, `HandleEvent`, and `HandleCommand` (unsolicited) encode their payloads to base64 on the Matter thread (as today), build an SBMD Transaction with the matching origin, and enqueue it to `SbmdWorker`. The worker is a single dedicated thread with a FIFO queue, which preserves per-device report order. It runs `SbmdTransactionOrchestrator::Run` for each matching handler in dispatch order (specific before multi before wildcard), one transaction per handler invocation. Outbound terminals remain unsupported from device-initiated handlers and are logged and ignored, so these transactions never perform device operations; an `error` terminal fails the transaction (staged writes discarded) and the failure hand-off runs.

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdWorker.h
#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace barton
{
    class SbmdWorker
    {
    public:
        static SbmdWorker &Instance();

        void Start();

        /** Stop accepting work, abort queued items, join. Safe to call twice. */
        void Stop();

        /** Enqueue from any thread (including the Matter thread). Never blocks beyond the queue mutex. */
        bool Post(std::function<void()> work);

        bool IsWorkerThread() const;

    private:
        void Run();

        std::mutex mtx;
        std::condition_variable cv;
        std::deque<std::function<void()>> queue;
        bool stopping = false;
        std::thread thread;
    };
} // namespace barton
```

**Alternative considered:** A GLib thread pool. Rejected for now — it loses per-device ordering unless keyed queues are added. Recorded as an open question.

**Alternative considered:** Run device-initiated handlers on the main loop. Rejected — the main loop would then wait on the Matter thread for every JS invocation, adding latency to all of device service; the dedicated worker isolates SBMD.

**Trade-offs:** A slow handler delays every later report for every device. Mitigated by the step deadlines and a queue-depth metric.

---

### D10: Ops become staged writes

**Decision:** `SbmdHandlerInvoker::ExecuteOps` is replaced by `SbmdHandlerInvoker::ToSbmdWrites(hctx, ops)` (pure conversion, no side effects) plus `RunLogOps(ops)`. `updateResource` → `SbmdDataKey{Resource, endpoint/resource}` (with optional metadata); `setMetadata` → `Metadata`; `setPersistentData` → `Metadata("sbmd." + key)`; `setTransientData` → `Transient` with TTL; later, `mergeTransientData` and conditional writes (BARTON-422). Unknown op types are logged as warnings and skipped, as today. Writes are staged in order and applied in order at commit.

**Alternative considered:** Stage only storage writes and keep `updateResource` immediate. Rejected — a resource update emitted for an operation that later fails is exactly the inconsistency the epic removes.

**Trade-offs:** Clients observe resource updates from a resource operation only after the whole SBMD Transaction succeeds; for multi-hop operations that is later than today.

---

### D11: Internal data-access seam `SbmdDataAccess` with `LegacySbmdDataAccess` (epic brief D13)

This design's D11 implements epic brief D13; it is unrelated to the epic-level D11 (atomic DAL operations) defined in `sbmd-dal-core`, which this seam merely carries.

**Decision:** Because this change lands before the DAL (epic D13), the orchestrator talks to data only through an internal seam. `SbmdDataAccess` is a per-driver factory; `Begin()` returns an `SbmdDataSession` owned by the `SbmdTransaction`. The seam carries every operation the DAL will later need — batched fetch including sequence-allocation entries, stage, stage-conditional, commit, abort, the attribute-write overlay hook, and an abort listener — so swapping the implementation does not touch the orchestrator. Each session method performs its own thread hops through `ThreadHop` and is called only from the orchestrator.

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdDataAccess.h
#pragma once

#include "ThreadHop.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace barton
{
    enum class SbmdDataNamespace
    {
        Attribute, // path "<endpoint>/<clusterId>/<attributeId>" (read-only; overlay applies)
        Resource,  // path "<endpoint>/<resource>" or "<resource>"
        Metadata,  // path "<name>" (persistent data uses "sbmd.<key>")
        Property,  // device-service property; reserved for later supplements
        Transient  // path "<key>"
    };

    struct SbmdDataKey
    {
        SbmdDataNamespace ns;
        std::string path;
        bool operator<(const SbmdDataKey &o) const { return std::tie(ns, path) < std::tie(o.ns, o.path); }
    };

    struct SbmdDataValue
    {
        std::string value;
        std::optional<std::chrono::seconds> ttl; // Transient writes only
        std::optional<std::string> resourceMetadataJson; // updateResource metadata only
    };

    struct SbmdWrite
    {
        SbmdDataKey key;
        std::optional<SbmdDataValue> value; // nullopt = delete
    };

    struct SbmdConditionalWrite
    {
        SbmdWrite write;
        std::optional<std::string> expected; // nullopt = key must be absent
    };

    struct SbmdFetchRequest
    {
        std::vector<SbmdDataKey> reads;
        std::vector<SbmdDataKey> sequences; // epic D11: atomically incremented during the fetch; never rolled back
    };

    struct SbmdFetchResult
    {
        std::map<SbmdDataKey, std::optional<SbmdDataValue>> values;
        std::map<SbmdDataKey, uint64_t> sequences;
    };

    struct SbmdDataError
    {
        enum class Kind
        {
            Aborted,
            Timeout,
            CommitFailed,
            Unsupported,
            Internal
        } kind;
        std::string reason;  // e.g. "conditionFailed", "pluginCommitFailed", "writeFailed"
        std::string message;
        std::string Describe() const { return reason.empty() ? message : reason + ": " + message; }
    };

    class SbmdDataSession
    {
    public:
        using Deadline = ThreadHop::Deadline;
        virtual ~SbmdDataSession() = default;

        virtual std::variant<SbmdFetchResult, SbmdDataError> Fetch(const SbmdFetchRequest &request, Deadline deadline) = 0;
        virtual std::optional<SbmdDataError> Stage(std::vector<SbmdWrite> writes) = 0;
        virtual std::optional<SbmdDataError> StageConditional(SbmdConditionalWrite write) = 0;
        /** Record a successful writeAttribute so later Attribute reads in this session return it. */
        virtual void RecordAttributeWrite(uint16_t endpoint, uint32_t cluster, uint32_t attribute, std::string tlvBase64) = 0;
        /** Applies staged writes. Ends the session. Mismatched conditional write -> CommitFailed/"conditionFailed". */
        virtual std::optional<SbmdDataError> Commit(Deadline deadline) = 0;
        /** Discards staged writes and overlay. Idempotent. Sequence allocations are not returned. */
        virtual void Abort(const std::string &reason) = 0;
        /** Called (any thread) when the backing store aborts the session, e.g. a DAL plugin unregistered. */
        virtual void SetAbortListener(std::function<void(std::string reason)> listener) = 0;
    };

    class SbmdDataAccess
    {
    public:
        virtual ~SbmdDataAccess() = default;
        virtual std::unique_ptr<SbmdDataSession> Begin(const std::string &deviceUuid) = 0;
    };
} // namespace barton
```

`LegacySbmdDataAccess` wraps today's paths with the affinity required by epic D13:

| Operation | Legacy behavior | Thread |
| --- | --- | --- |
| Fetch: `Resource` | `deviceServiceGetResourceById` (today's `MakeResFetcher`) | Orchestrator (inline) |
| Fetch: `Metadata` | `deviceServiceGetMetadata` (today's `MakePersistFetcher`) | Orchestrator (inline) |
| Fetch: `Transient`, `Attribute`, sequences | One `ThreadHop::OnMatter` closure reading `transientStore` (TTL-checked), the device data cache (overlay first), and incrementing transient sequence keys | Matter (scheduled by the orchestrator) |
| Stage / StageConditional | Append to the session's staged list, held by the `SbmdTransaction` | Orchestrator (no hop) |
| Commit | Single `OnMatter` closure: check every Transient conditional and apply Transient writes atomically (Matter-confined, so atomic by confinement); on mismatch nothing is applied and `conditionFailed` is returned. Then apply `updateResource` / `setMetadata` / `deviceServiceSetMetadata` in staged order on the orchestrator; the first failure stops and returns `writeFailed` (no cross-store rollback) | Matter, then orchestrator |
| Conditional/sequence on non-Transient keys | Return `Unsupported` (surfaced as `internal`); no driver can request them until BARTON-422 | — |
| Abort | Clear staged list and overlay | Orchestrator |
| Abort listener | Stored, never fired (no plugins); device removal, driver deactivation, and shutdown are signalled by the driver (D7) | — |

This is legal under the wait-direction rule because only the orchestrator waits: the Matter-thread closures never wait on anything. It also needs no mutex on `transientStore`, which stays Matter-confined until BARTON-426 deletes it.

BARTON-421 + 426 later provide `DalSbmdDataAccess`, whose session owns one `DalTransaction` and maps `Fetch` → `DalTransaction::Read` + `AllocateSequence`, `Stage` → `Stage`, `StageConditional` → `StageConditional`, `RecordAttributeWrite` → `SetOverlay`, `Commit`/`Abort` → `Commit`/`Abort`, each through `ThreadHop::OnMainLoop` (inline when the orchestrator owns the main loop), and fires the abort listener from plugin unregistration. After the switch to `DalSbmdDataAccess`, BARTON-426 deletes `LegacySbmdDataAccess` together with `transientStore`; there is no runtime fallback or feature flag selecting the legacy implementation.

**Alternative considered:** Land the DAL first (the previous delivery order). Rejected by epic D13 — the orchestrator would have to coexist with a DAL used from the Matter thread, which required interim locking of the transient store.

**Alternative considered:** Keep calling the fetchers and setters directly from the orchestrator. Rejected — staging, conditional writes, sequences, and the overlay would each need a second implementation when the DAL arrives.

**Trade-offs:** The legacy commit has no cross-store rollback; documented under Risks.

---

### D12: Recovery rule

**Decision:** Epic D12 is applied in the step loop. When an armed hop's `onError` returns a non-`error` terminal and BARTON-412 honors it (failure types `internal`, `transport`, `commandFailed`, `readFailed`), the driver has recovered: `success()` commits the transaction, and a continuation terminal continues the chain. The transaction succeeds unless a later step fails, and `OnTransactionFailed` (and therefore BARTON-424's `onTransactionFailure`) is not called. Only a transaction that terminates in failure reaches the hand-off. For `timeout` and depth-limit failures the recovery terminal is ignored (BARTON-412 rule, kept as a decided exception to epic D12): the transaction fails and the hand-off runs. Rationale: a timeout means the transaction's deadline budget is exhausted and a depth-limit failure means the chain is runaway, so continuing is impossible or unsafe; and treating `success()` there as recovery would hide a device that never answered — the case where a driver most needs compensation. The operation log keeps the failed operation's `failed` record even when the transaction recovers.

**Alternative considered:** Always run the failure handler when any device operation failed. Rejected by epic D12 — a driver that handled the error locally would be compensated twice.

**Trade-offs:** A driver that wants compensation after a hop failure must return `error()` from `onError`; the camera migration (BARTON-425) changes its handlers accordingly.

## Data Structures

### Inputs

`SbmdStepTrigger` (D3) is the input to each step. `SbmdFetchRequest`, `SbmdWrite`, and `SbmdConditionalWrite` (D11) are the inputs to the data seam.

### Outputs

`SbmdTransactionResult` (D3) is returned to the caller. `SbmdOperationLog::Records()` (D2) and `SbmdTransaction::failure` are the inputs to BARTON-424's `onTransactionFailure`.

### JS-visible changes

`args` for `onResponse`/`onError` keeps its current shape (`args.response`, `args.attribute`, `args.error`, `args.handlerContext`, `args.resource`). `args.supplements` is now present on deferred handlers and is fetched at step time (default contents are BARTON-422's decision). `args.error.type` values delivered to `onError` are unchanged from BARTON-412 (`timeout`, `commandFailed`, `readFailed`, `transport`, `internal`); `aborted` and `commitFailed` are never delivered to `onError` (D7), only to the transaction failure hand-off.

## Interfaces

No public C API, GObject, signal, property, or GIR changes. Internal C++ interfaces:

| Interface | Location | Purpose |
| --- | --- | --- |
| `SbmdTransaction` | `sbmd/txn/SbmdTransaction.h` | Per-request state, operation log, data-session ownership |
| `SbmdDataAccess` / `SbmdDataSession` | `sbmd/txn/SbmdDataAccess.h` | Internal data seam (D11) |
| `LegacySbmdDataAccess` | `sbmd/txn/LegacySbmdDataAccess.{h,cpp}` | Seam over today's fetchers/setters and `transientStore` |
| `SbmdOperationLog` | `sbmd/txn/SbmdOperationLog.h` | Ordered device-operation records |
| `SbmdTransactionOrchestrator::Run` | `sbmd/txn/SbmdTransactionOrchestrator.h` | Step loop on the owning thread |
| `ThreadHop` | `sbmd/txn/ThreadHop.h` | Deadline-bounded main-loop/Matter hops |
| `SbmdWorker` | `sbmd/txn/SbmdWorker.h` | Orchestrator thread for device-initiated work |
| `MatterDevice::InvokeCommand` / `WriteAttribute` | `matter/MatterDevice.h` | Outcome-callback device operations |
| `MatterDeviceDriver::ConnectAndOrchestrate` | `matter/MatterDeviceDriver.h` | Session establishment + orchestrated run |
| `SpecBasedMatterDeviceDriver::OnTransactionFailed` | `sbmd/SpecBasedMatterDeviceDriver.h` | Failure hand-off hook (BARTON-424 fills in) |

### Thread-safety summary

```text
  With LegacySbmdDataAccess (this change):

  device-service caller / SbmdWorker                                               Matter thread
  (orchestrator; owns SbmdTransaction + staged writes)                             (stack, cache, JS, transientStore)
  ----------------------------------------------------                             ----------------------------------
  step N:
    Fetch: device-service reads (resources, metadata) inline on orchestrator
           OnMatter(transient + attribute-cache reads, sequence allocs) -------->  transientStore / DeviceDataCache
           <-------------------------------------------------------------------
    OnMatter(InvokeStep) ------------------------------------------------------>  JS handler (runtime mutex)
    <-------------------------------------------------------------------------  ParsedResult
    Stage: append to SbmdTransaction (no hop)
    OnMatter(start device op) ------------------------------------------------->  InvokeCommand(...)
    wait outcome slot (deadline) <--------------------------------- PostToOrchestrator (never blocks)
  finish:
    Commit: device-service writes inline; OnMatter(transient writes + CAS) ---->  transientStore
    Abort:  drop staged list (no hop)

  With DalSbmdDataAccess (after BARTON-421/426): Fetch/Stage/Commit/Abort become OnMainLoop hops to the
  DalTransaction; the attribute and transient plugins hop main loop -> Matter by affinity.

  Allowed waits:  orchestrator -> main loop,  orchestrator -> Matter,  main loop -> Matter
  Forbidden:      Matter -> main loop,  Matter -> orchestrator
```

Only the orchestrator waits on another thread in the legacy arrangement, which is why it may schedule `transientStore` and attribute-cache access onto the Matter thread without risking the forbidden Matter → main loop wait. GObject implications: none; no new signals or properties. Resource-updated events are emitted when the staged `updateResource` is applied at commit, through the same `updateResource` call used today.

## Implementation Detail

### `SpecBasedMatterDeviceDriver` changes

- Delete `PendingOperation`, `pendingOperations`, `nextPendingId`, `ExecuteRequestCommand`, `ExecuteReadAttribute`, `HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`, `CompletePendingOperation`, `ReleasePendingHandlers`, and BARTON-412's `FailPendingOperation`. Their behavior is covered by D1, D3, D5, D7.
- `HandleResourceOp` builds the `SbmdTransaction` and `SbmdStepTrigger::Initial`; the orchestrator runs it.
- `DispatchToHandlers` posts to `SbmdWorker`.
- `HandleCommand` first offers the command to armed `requestCommand` hops for the device (match on cluster + `responseCommandId`), then falls through to unsolicited dispatch.
- `MakeAttrFetcher`/`MakeResFetcher`/`MakePersistFetcher`/`MakeTransientFetcher`/`MakeTransientSetter`, `SetTransientData`, `GetTransientData`, and `transientStore` move behind `LegacySbmdDataAccess` and are no longer called from handler paths directly. BARTON-421/426 delete them when `DalSbmdDataAccess` replaces the legacy implementation.
- Live-transaction registry, abort on `DeviceRemoved`, driver deactivation, and `Shutdown`.

### Metrics

Extend `SpecBasedMatterDeviceDriverMetrics` with: `sbmd.txn.started{driver,origin}`, `sbmd.txn.committed{driver,origin}`, `sbmd.txn.failed{driver,origin,type}`, `sbmd.txn.steps` (histogram), `sbmd.txn.hop_latency_ms{target}` (histogram, `target` = `main`|`matter`), `sbmd.txn.deadline_missed{driver,phase}`, `sbmd.worker.queue_depth` (gauge). The existing deferred metrics (`RecordDeferredStart/Complete/Timeout/DepthExceeded`) are mapped onto these and kept under their current names for one release.

## Migrations

Delivery order across the epic (D13): BARTON-412 fix → **423** → 421 → 426 → 422 → 424 → 425. This change depends only on the 412 fix. Within this change:

1. Introduce `ThreadHop`, `SbmdOperationLog`, `SbmdTransaction`, `SbmdDataAccess`, and `LegacySbmdDataAccess` with tests (no behavior change).
2. Add `MatterDevice::InvokeCommand`/`WriteAttribute` outcome APIs.
3. Introduce the orchestrator and route resource operations through `ConnectAndOrchestrate`.
4. Port deferred handling and BARTON-412 guarantees; delete `PendingOperation`.
5. Introduce `SbmdWorker`; route device-initiated dispatch.
6. Move seeds.
7. Replace `ExecuteOps` with staging; commit/abort; overlay; aborts.

No persisted data format changes. Driver JS files need no changes for this change to function; BARTON-425 migrates drivers to take advantage of it.

## Testing Philosophy

### SbmdTransaction and operation log

Unit-test the exactly-once latch under concurrent `TryFinish` from multiple threads, hop generation rejection of superseded and post-finish callbacks, abort request visibility, and the log's append/mark ordering. These tests use no Matter stack and no JS.

### Orchestrator step loop

Use a fake `ThreadHop` that runs closures on two test threads (fake main loop, fake Matter) and a fake `SbmdDataAccess` that records fetches, staged and conditional writes, commit, and abort calls. Cover: synchronous `success` commits staged writes in order; `error` aborts and runs only `log` ops; `requestCommand` → response → `onResponse` sees supplements read after the response; `onError` recovery terminal continues for `transport` and is ignored for `timeout`; depth limit; overall deadline with no response; missed hop deadline turns into `timeout` and the late closure performs no side effects; commit failure produces `commitFailed` and the hand-off; abort during a device wait wakes the orchestrator and produces `aborted`.

### Wait-direction rule

Assert in debug builds that `OnMainLoop` is never entered on the Matter thread. A test drives the orchestrator from the fake main-loop thread and verifies main-loop work runs inline (no post). A stress test runs 1,000 concurrent transactions across the worker and caller threads with randomized hop delays and asserts no deadlock within the deadlines.

### Device operations and operation log

With the existing `MatterDevice` test doubles, verify every terminal kind appends a record with correct endpoint/cluster/command-or-attribute/request; status transitions; `notSent` for pre-send failures; chained `sendCommand` after `onResponse` waits for the device status and fails the transaction on device rejection; successful `writeAttribute` calls `RecordAttributeWrite` and a later step's attribute supplement returns the written value.

### `SbmdDataAccess` contract test suite

423 owns a reusable contract suite, `core/test/src/SbmdDataAccessContractTest.h`, written as a GoogleTest typed test (`TYPED_TEST_SUITE_P` with `REGISTER_TYPED_TEST_SUITE_P`) over a fixture trait that creates an implementation, seeds data, and exposes the fake `ThreadHop`. Every `SbmdDataAccess` implementation must pass it unchanged. It covers: one fetch returns all requested keys with absent keys as empty; staged writes invisible to other sessions until commit and applied in staged order; abort discards staged writes and overlay and is idempotent; `RecordAttributeWrite` visible to later fetches in the same session only; sequence allocation unique under concurrent sessions and not rolled back on abort; conditional-write mismatch fails commit with reason `conditionFailed`; operations on an ended session return `Aborted`; missed deadlines return `Timeout` with no late side effects; the abort listener wakes the owner when the backing store aborts. Capabilities an implementation does not support (legacy: non-Transient sequences/conditionals) are declared by the trait and asserted to return `Unsupported` instead. 423 instantiates it for `LegacySbmdDataAccess`; BARTON-421/426 instantiate it for `DalSbmdDataAccess`, which must declare no unsupported capabilities.

### LegacySbmdDataAccess

Verify thread affinity (transient and attribute-cache work only on the fake Matter thread; device-service calls on the orchestrator), staged writes held until commit and applied in order, abort discards them, overlay visible to later fetches, transient sequence allocation atomic under concurrent transactions and not rolled back on abort, transient conditional write mismatch → `conditionFailed`, and unsupported namespaces for sequence/conditional return `internal`.

### Recovery rule (D12)

An `onError` returning `success()` after a `transport` failure commits the transaction and does not call `OnTransactionFailed`; an `onError` returning a continuation that later succeeds also commits; an `onError` returning `success()` after a `timeout` is ignored (BARTON-412) and the transaction fails.

### BARTON-412 regression

Port every scenario in `SbmdDeferredFailureTest.cpp` to the orchestrator and keep it passing unchanged in expectation, except the scenario "Rollback is visible when the caller is unblocked", which now asserts that writes staged by `onError` are discarded when the transaction fails (the driver's compensation moves to BARTON-424).

### SbmdWorker

Verify FIFO order per device, that a slow handler cannot exceed its deadline, that `Stop()` aborts queued and in-flight transactions with `aborted`, and that attribute-report ops are applied at commit, not inline.

### Integration

Existing pytest suites (`testing/test/door_lock_test.py`, `light_test.py`, `contact_sensor_test.py`, `water_leak_detector_test.py`, camera tests) must pass unchanged in Docker. These require Docker but not hardware.

## Documentation Plan

### `docs/SBMD.md`

**Audience:** SBMD driver authors.

Update §2.3 (data flow) to describe an SBMD Transaction and its steps. Update §5.1 "Deferred handler context" to state that deferred handlers receive supplements read when the handler runs. Update §6.2 "Runtime behavior" to describe that responses, errors, and timeouts are delivered as steps of the transaction and that the transaction's data writes are committed only when it succeeds. Document the recovery rule (an `onError` whose non-`error` terminal is honored recovers the operation). Update §7 to state that data-writing ops are staged and committed only on success, while `log` runs immediately, and that `sendCommand`/`writeAttribute` returned from a deferred handler now fail the operation if the device rejects them. Add a glossary box defining SBMD Transaction and DAL Transaction.

### `docs/barton-device-service-architecture.md`

**Audience:** Barton maintainers.

There is no `docs/dev/architecture.md` in this project; the architecture document is `docs/barton-device-service-architecture.md`. Add a short "SBMD execution model" subsection under the Matter driver section containing the thread-safety diagram from this design and the wait-direction rule, and note that `SbmdWorker` is a new long-lived thread started with the Matter subsystem.

## Risks / Trade-offs

### Hop latency

**Risk:** Each step now crosses threads two to four times. On a busy main loop, resource operations get slower, and device-initiated updates are applied later than today.

**Mitigation:** One batched read per step; inline when already on the main loop; histograms for hop latency; combine stage-and-next-read into one hop if measurements justify it.

### Main loop blocked on the Matter thread

**Risk:** Existing main-loop code that calls `RunOnMatterSync` or holds `LockChipStack` delays scheduled work, and Matter-thread data reads (transient, attribute cache; later the attribute plugin) can queue behind long Matter work.

**Mitigation:** The direction is legal, so this is latency, not deadlock. Audit listed as an open question; deadlines convert pathological cases into logged failures.

### Behavior change: writes discarded on failure

**Risk:** Drivers that rely on a write in an `error` path or in `onError` persisting (for example the camera driver's session bookkeeping) lose that write.

**Mitigation:** Called out in the proposal and docs; BARTON-424 provides the supported compensation path and BARTON-425 migrates the camera driver. Tests capture the new contract.

### Interleaving before park

**Risk:** With fetch, JS, and commit on separate hops, two transactions on the same device can both read a value and both write a derived value (lost update, epic D11), even before any device operation.

**Mitigation:** The seam carries sequence allocation and conditional writes; BARTON-421/426 implement them across stores and BARTON-422 exposes them to drivers. Until then the legacy implementation offers them only for the Transient namespace and drivers keep today's behavior. Documented limitation: an unconditional read-compute-write of one key can still lose an update.

### No cross-store rollback in the legacy commit

**Risk:** A legacy commit that writes a resource and then fails a metadata write leaves the resource updated.

**Mitigation:** Device-service writes rarely fail; the failure is reported as `commitFailed` and handed off. `DalSbmdDataAccess` brings epic D8 rollback.

### Ordering of device-initiated updates

**Risk:** Moving report handlers off the Matter thread could reorder updates relative to resource operations that read the same resources.

**Mitigation:** Single FIFO worker preserves report order; each step reads current data; resource operations and report transactions are independent SBMD Transactions with independent data sessions, as designed by the epic.

### Late closures after deadline

**Risk:** A closure scheduled before a deadline expired runs afterwards and touches freed state.

**Mitigation:** All closures capture `shared_ptr`/`weak_ptr` and check the abandoned flag; covered by tests with injected delays.

## Open Questions

- `SbmdWorker`: single thread vs GLib thread pool; per-device ordering if a pool is used.
- Audit of main-loop code that blocks on the Matter thread (`LockChipStack`, `RunOnMatterSync`) for latency.
- Thread-safety contract of `DeviceDataCache::GetAttributeData` (assumed Matter-thread-only; the attribute plugin's affinity relies on it).
- Status-only command continuation: `requestCommand` requires `responseCommandId`, so a driver cannot continue a chain after a command whose only response is a status (for example Door Lock `SetUser`, `SetWeekDaySchedule`). Multi-command flows such as the BARTON-424 door-lock example need either an optional `responseCommandId` (continue on success status) or a new terminal. Not in this change's scope; needs a team decision.
- Seed invocation from `DoRegisterDriverResources` currently runs inside `RunOnMatterSync`; confirm that moving it after `RunOnMatterSync` does not change commissioning order expectations.
- Failure type for a handler `error` terminal: this design uses the type of the hop error being handled when the terminal comes from `onError`, otherwise `internal`. Confirm, or add a dedicated type.
