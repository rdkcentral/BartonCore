## ADDED Requirements

### Requirement: SBMD Transaction object
The runtime SHALL represent every SBMD request — a resource read, write, or execute; a seed; an attribute-report, event, or unsolicited-command handler invocation; and a compensation chain — as exactly one SBMD Transaction (`SbmdTransaction`). An SBMD Transaction spans the initial handler invocation, every parked device operation and deferred callback, and the terminal result. Each SBMD Transaction SHALL own exactly one data session obtained from the internal data-access seam (`SbmdDataAccess`) for its lifetime. The SBMD Transaction SHALL reach exactly one terminal outcome, committed or failed; once it has reached an outcome, no further handler of that transaction SHALL be invoked by a late device callback.

#### Scenario: One SBMD Transaction per resource operation
- **WHEN** a client executes a resource whose handler returns `requestCommand`, and the `onResponse` handler returns `success`
- **THEN** one SBMD Transaction exists for the whole flow, owns one data session, and commits once

#### Scenario: Late callback after completion is ignored
- **WHEN** an SBMD Transaction has failed with `timeout` and the device's response arrives afterwards
- **THEN** no handler is invoked and the transaction's outcome does not change

#### Scenario: Superseded hop callback is ignored
- **WHEN** an `onResponse` handler returns a new `requestCommand`, and a late response or error for the previous command arrives
- **THEN** neither the previous nor the current hop's handlers are invoked by that late callback

### Requirement: Internal data-access seam
All data access by an SBMD Transaction SHALL go through an `SbmdDataAccess` implementation that provides, per session: a batched fetch that returns the step's supplement values and performs any sequence-allocation entries atomically; staging of writes; staging of conditional writes (an expected current value checked atomically at commit); commit; abort; recording of a successful attribute write so later reads in the session return it; and an abort listener the backing store can fire. JavaScript handlers SHALL NOT call the seam. Sequence allocations SHALL NOT be undone when a session aborts. A conditional write whose expected value does not match at commit SHALL fail the commit with reason `conditionFailed` and SHALL apply none of the session's writes to the store holding that key.

#### Scenario: Sequence allocation is atomic and not rolled back
- **WHEN** two concurrent SBMD Transactions for the same device each fetch a sequence entry for key `nextId`, and the first then fails
- **THEN** the two transactions receive different values, and the value allocated to the failed transaction is not reused

#### Scenario: Every implementation passes the contract suite
- **WHEN** an `SbmdDataAccess` implementation is added or changed
- **THEN** it passes the shared `SbmdDataAccess` contract test suite, with any unsupported capability declared and returning an unsupported error

#### Scenario: Conditional write mismatch
- **WHEN** a transaction stages a conditional write to transient key `owner` expecting `"a"`, and another transaction commits `owner = "b"` first
- **THEN** the first transaction's commit fails with reason `conditionFailed`

### Requirement: Legacy data-access implementation
Until a DAL-backed implementation replaces it, the runtime SHALL use `LegacySbmdDataAccess`, which wraps the existing resource, metadata, persistent-data, and transient-data paths. Staged writes SHALL be held by the SBMD Transaction and applied, in staged order, only at commit. Access to the transient store and to the device attribute cache SHALL be performed on the Matter thread by work the orchestrator schedules there; device-service resource and metadata calls SHALL be made from the orchestrator. Sequence allocation and conditional writes SHALL be supported for the Transient namespace and SHALL be reported as an `internal` failure for other namespaces. The legacy commit SHALL report the first failed write as `commitFailed` without rolling back writes already applied.

#### Scenario: Transient access stays on the Matter thread
- **WHEN** a step declares a transient supplement and stages a transient write that later commits
- **THEN** both the read and the write of the transient store execute on the Matter thread, scheduled by the orchestrator

#### Scenario: Staged writes not visible before commit
- **WHEN** a transaction stages `setTransientData("k", "v", 60)` and parks on `requestCommand`
- **THEN** another transaction reading `k` before the first commits does not see `"v"`

### Requirement: Operation log
Each SBMD Transaction SHALL record, in request order, one entry for every device operation it requests: `sendCommand` and `requestCommand` (kind `command`), `readAttribute` (kind `readAttribute`), and `writeAttribute` (kind `writeAttribute`). An entry SHALL be appended before the operation is attempted and SHALL contain `step`, `kind`, the resolved `endpoint` (absent if resolution failed), `cluster`, `command` or `attribute`, the `request` payload as supplied by the driver, and `status` — `notSent` until the operation is attempted, then `succeeded` with the `response` (command response payload or attribute value, if any) or `failed` with the `error` (`type`, `message`, `matterCode`). The log SHALL be retained until the SBMD Transaction ends, including while its failure handler hand-off runs.

#### Scenario: Successful multi-step chain is logged
- **WHEN** a transaction sends `requestCommand` A, its `onResponse` returns `requestCommand` B, and B succeeds
- **THEN** the log contains two `command` entries for A then B, both `succeeded`, each with its response payload

#### Scenario: Pre-send failure is logged as notSent
- **WHEN** a handler returns `requestCommand` for a cluster that no endpoint on the device supports
- **THEN** the log contains a `command` entry with no `endpoint`, status `notSent`, and an `error` of type `internal`

#### Scenario: Device rejection is logged as failed
- **WHEN** a `writeAttribute` is rejected by the device with a Matter status
- **THEN** the log entry for it has status `failed` and `error.matterCode` set to that status

### Requirement: Orchestrator step loop
The SBMD Transaction SHALL be executed by an orchestrator (`SbmdTransactionOrchestrator`) on the thread that owns the request. Each step SHALL: (1) fetch all of the step's supplements, including attribute supplements and sequence entries, in one batched fetch through the data session; (2) invoke the JavaScript handler for the step on the Matter thread with those supplements; (3) execute `log` operations immediately and stage the step's data write operations in the data session; (4) if the terminal is a device operation, start it on the Matter thread and receive its outcome back on the orchestrator; (5) if the terminal is deferred, run the next step with the response or error as its trigger and with supplements fetched for that step; (6) on a successful terminal, commit the data session; on failure, abort it and hand off to the failure handler. Supplements SHALL be fetched freshly for every step and SHALL NOT be reused from an earlier step.

#### Scenario: Deferred callback sees data changed while parked
- **WHEN** a handler declares a transient-data supplement, returns `requestCommand`, another transaction commits a new value for that key while the first is parked, and the response arrives
- **THEN** the `onResponse` handler's `args.supplements` contains the newly committed value

#### Scenario: One fetch per step
- **WHEN** a handler declares two attribute supplements, one resource supplement, and one transient supplement
- **THEN** the orchestrator calls the data session's fetch exactly once for that step

### Requirement: Recovery rule
When a deferred hop fails and its `onError` returns a non-`error` terminal that the runtime honors under "Deferred failure callback delivery", the SBMD Transaction SHALL be treated as recovered: a `success` terminal SHALL commit the transaction, and a continuation terminal SHALL continue the chain. A recovered transaction SHALL NOT be handed to the transaction failure handler unless a later step causes it to terminate in failure. Only an SBMD Transaction that terminates in failure SHALL be handed to the failure handler. The failed operation's log entry SHALL remain `failed` after recovery.

#### Scenario: onError returns success
- **WHEN** a `requestCommand` send is rejected with `transport` and `onError` returns `success("cached")`
- **THEN** the transaction commits, the caller receives `"cached"`, and the failure handler hand-off does not run

#### Scenario: Recovery ignored for timeout
- **WHEN** a `requestCommand` times out and `onError` returns `success()`
- **THEN** the recovery terminal is ignored, the transaction fails with `timeout`, and the failure handler hand-off runs

### Requirement: Deferred callbacks run as orchestrator steps
Device responses, device errors, and per-hop or overall timeouts for a deferred operation SHALL be handed back to the orchestrator. The runtime SHALL NOT invoke `onResponse` or `onError` inline from a Matter SDK callback. The orchestrator SHALL invoke them as the next step, on the Matter thread, as defined by the step loop.

#### Scenario: Response handler invoked via the orchestrator
- **WHEN** the device's response command for a pending `requestCommand` is received on the Matter thread
- **THEN** the Matter thread posts the outcome to the orchestrator and returns, and the orchestrator fetches supplements and then invokes `onResponse`

### Requirement: Wait-direction rule
The orchestrator MAY wait on the device-service main loop and on the Matter thread. Code running on the main loop MAY wait on the Matter thread. Code running on the Matter thread SHALL NOT wait on the main loop or on an orchestrator; it SHALL report device outcomes by posting without blocking. JavaScript handlers SHALL NOT perform data access, and work scheduled on the Matter thread by the orchestrator SHALL NOT wait on any other thread. When the orchestrator is the device-service main loop (`g_main_context_is_owner` on the default context is true), work destined for the main loop SHALL run inline instead of being posted.

#### Scenario: Orchestrator on the main loop
- **WHEN** a resource operation is issued from code running on the device-service main loop
- **THEN** the transaction completes without posting work to the main loop and without deadlock

#### Scenario: Matter thread never waits
- **WHEN** a device response arrives while the orchestrator is busy
- **THEN** the Matter thread posts the outcome and continues processing without waiting

### Requirement: Deadlines on all waits
Every wait performed by an orchestrator or by main-loop code on another thread SHALL be bounded by a deadline. The SBMD Transaction's overall deadline SHALL be the earliest of the request's `timeoutMs` (when set on the current hop), the driver's `matter.defaultTimeoutMs` or the system default of 30 seconds, and the device-service caller's own timeout. The orchestrator SHALL enforce the overall deadline while waiting, not only when a response arrives. A missed deadline SHALL fail the SBMD Transaction with failure type `timeout` and SHALL be logged; work that runs after its waiter's deadline has passed SHALL have no side effects.

#### Scenario: No response before the deadline
- **WHEN** a `requestCommand` is sent and no response or error arrives before the transaction's deadline
- **THEN** the orchestrator stops waiting at the deadline, the current hop's `onError` is invoked with `type: "timeout"`, and the transaction fails if `onError` does not succeed

#### Scenario: Stalled Matter thread during fetch
- **WHEN** the Matter thread does not run a scheduled transient read before the transaction's deadline
- **THEN** the transaction fails with `timeout`, the failure is logged, and the late read, when it eventually runs, performs no work

### Requirement: Commit and abort
On a successful terminal the orchestrator SHALL commit the data session, and SHALL deliver the success value to the caller only after the commit succeeds. If the commit fails, the SBMD Transaction SHALL fail with failure type `commitFailed`, carrying the commit's reason (for example `conditionFailed`). On any failure — a handler `error` terminal not recovered under the recovery rule, a device failure, a timeout, a depth-limit failure, a pre-send failure, `aborted`, or `commitFailed` — the orchestrator SHALL abort the data session, discarding all staged writes, and SHALL then hand off to the transaction failure handler before unblocking the caller. A handler `error` terminal SHALL be reported with the type of the hop error being handled when returned from `onError`, and as `internal` otherwise.

#### Scenario: Staged writes discarded on failure
- **WHEN** a handler stages `updateResource` and `setTransientData`, returns `requestCommand`, and the device returns a failure that `onError` turns into `error`
- **THEN** neither the resource nor the transient value is changed, and the caller receives failure

#### Scenario: Commit failure
- **WHEN** a transaction succeeds but its commit fails with `conditionFailed`
- **THEN** the transaction fails with `commitFailed`, the failure handler hand-off runs, and the caller receives failure

### Requirement: Transaction aborts
The runtime SHALL fail a running SBMD Transaction with failure type `aborted` when its device is removed, when its driver is deactivated or unloaded, when device service shuts down, or when its data session's abort listener fires. An orchestrator waiting for a device outcome SHALL wake promptly on an abort rather than waiting for its deadline. `aborted` SHALL NOT be delivered to a hop's `onError`.

#### Scenario: Device removed while parked
- **WHEN** a transaction is waiting for a `requestCommand` response and the device is removed
- **THEN** the orchestrator wakes, the transaction fails with `aborted`, staged writes are discarded, and `onError` is not invoked

### Requirement: Attribute write overlay
When a `writeAttribute` device operation succeeds, the orchestrator SHALL record the written value in the data session before running the next step. Later attribute reads within the same SBMD Transaction SHALL return the written value. The overlay SHALL be discarded when the transaction ends. The runtime SHALL NOT automatically undo an attribute write when the SBMD Transaction later fails.

#### Scenario: Read your own attribute write
- **WHEN** a handler returns `writeAttribute` for attribute X, the write succeeds, and a later step in the same transaction declares X as a supplement
- **THEN** that step's supplement for X is the written value, even if no subscription report has arrived yet

#### Scenario: No automatic attribute undo
- **WHEN** a transaction writes attribute X successfully and a later step fails
- **THEN** the runtime does not write X back; the failure handler hand-off receives an operation log containing the successful `writeAttribute`

### Requirement: SBMD worker for device-initiated work
SBMD work with no waiting caller — attribute-report handlers, event handlers, and unsolicited command handlers — SHALL run as SBMD Transactions orchestrated by a dedicated `SbmdWorker` thread. The Matter thread SHALL encode the payload and post the work to the worker without waiting. The worker SHALL process work for a device in the order it was posted. Each matching handler invocation SHALL be its own SBMD Transaction.

#### Scenario: Attribute report handled on the worker
- **WHEN** an attribute report arrives for a handler that stages `updateResource`
- **THEN** the Matter callback returns after posting, and the resource update is applied when the worker's transaction commits

#### Scenario: Report order preserved
- **WHEN** two reports for the same device arrive in order A then B
- **THEN** the transaction for A commits before the transaction for B starts

### Requirement: Device operation outcomes are awaited
Every device operation an SBMD Transaction starts — including a `sendCommand` or `writeAttribute` returned from an `onResponse` or `onError` handler — SHALL be awaited by the orchestrator until the device's outcome or the deadline. A device rejection of a non-deferred `sendCommand` or `writeAttribute` SHALL fail the SBMD Transaction with failure type `commandFailed` and SHALL NOT invoke any `onError`.

#### Scenario: Chained sendCommand rejected by the device
- **WHEN** an `onResponse` handler returns `sendCommand` and the device returns a failure status
- **THEN** the transaction fails with `commandFailed` and its staged writes are discarded
