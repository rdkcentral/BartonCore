## ADDED Requirements

### Requirement: Driver-level transaction failure handler declaration
An SBMD driver MAY declare at most one transaction failure handler as the optional top-level `onTransactionFailure` property of its `SbmdDriver({...})` registration object. The property SHALL accept either a function, or an object `{ handler: function, supplements?: {...} }` whose `supplements` use the same format as other handler supplements. A registration whose `onTransactionFailure` is present but is neither form SHALL be rejected. The handler SHALL be held alive while the driver is activated and released when it is deactivated, like other handlers. The SBMD registration schema SHALL accept the property, and drivers that omit it SHALL behave as if no failure handler exists.

#### Scenario: Bare function form
- **WHEN** a driver declares `onTransactionFailure: handleFailure` where `handleFailure` is a function
- **THEN** the driver loads and `handleFailure` is the driver's transaction failure handler with no supplements

#### Scenario: Object form with supplements
- **WHEN** a driver declares `onTransactionFailure: { supplements: { transientData: ["sessions"] }, handler: handleFailure }`
- **THEN** the driver loads and `handleFailure` receives `args.supplements.transientData.sessions` when invoked

#### Scenario: Invalid declaration rejected
- **WHEN** a driver declares `onTransactionFailure: { supplements: {} }` with no `handler` function
- **THEN** the driver file is rejected with an error

### Requirement: Exactly-once invocation for every failed SBMD Transaction
For every SBMD Transaction that terminates in failure, the runtime SHALL invoke the driver's `onTransactionFailure` handler exactly once, when the driver declares one and the transaction has failure handling enabled. This SHALL apply to every failure type: `timeout`, `commandFailed`, `readFailed`, `transport`, `internal`, `aborted`, and `commitFailed`, and to every failure origin: a handler `error` terminal, a device failure, a per-hop or overall timeout, the maximum deferral depth, a pre-send failure, an abort, and a commit failure (including reason `conditionFailed` when a conditional write's expected value no longer matches). The runtime SHALL NOT invoke the handler for an SBMD Transaction that commits. The runtime SHALL NOT invoke the handler for a transaction that is itself running a compensation chain.

#### Scenario: Device failure at a later step
- **WHEN** an SBMD Transaction's first two device commands succeed and its third is rejected by the device
- **THEN** `onTransactionFailure` is invoked once with `args.failure.type` `"commandFailed"` and `args.failure.step` identifying the failing step

#### Scenario: Handler error terminal before any device operation
- **WHEN** a resource handler returns `error("invalid input")` without performing a device operation
- **THEN** `onTransactionFailure` is invoked once with `args.failure.type` `"internal"`, `args.failure.message` `"invalid input"`, and an empty `args.operations`

#### Scenario: Commit failure
- **WHEN** an SBMD Transaction reaches `success` but its commit fails
- **THEN** `onTransactionFailure` is invoked once with `args.failure.type` `"commitFailed"`

#### Scenario: Conditional write mismatch
- **WHEN** an SBMD Transaction staged a conditional write expecting `"a"` and another transaction committed `"b"` to that key first
- **THEN** `onTransactionFailure` is invoked once with `args.failure.type` `"commitFailed"` and `args.failure.message` beginning with `conditionFailed`

#### Scenario: Abort on device removal
- **WHEN** a transaction is parked on a `requestCommand` and the device is removed
- **THEN** `onTransactionFailure` is invoked once with `args.failure.type` `"aborted"` and a message naming device removal

#### Scenario: Not invoked on success
- **WHEN** an SBMD Transaction commits
- **THEN** `onTransactionFailure` is not invoked

#### Scenario: Driver without a handler
- **WHEN** a driver that does not declare `onTransactionFailure` has a transaction fail
- **THEN** the failure is logged and no handler is invoked

### Requirement: Recovery suppresses the failure handler
If a step's `onError` returns `success()` or another non-`error` terminal that the runtime honors under "Deferred failure callback delivery", the driver has recovered: the SBMD Transaction SHALL continue or succeed, and the runtime SHALL NOT invoke `onTransactionFailure` for that hop's failure. `onTransactionFailure` SHALL run only when the SBMD Transaction terminates in failure. If a recovered transaction later terminates in failure, `onTransactionFailure` SHALL be invoked exactly once for that later failure. When the runtime ignores a recovery terminal (`timeout` and maximum-deferral-depth failures), the transaction terminates in failure and `onTransactionFailure` SHALL be invoked.

#### Scenario: onError returns success
- **WHEN** a `requestCommand` send is rejected with `transport` and its `onError` returns `success()`
- **THEN** the SBMD Transaction commits and `onTransactionFailure` is not invoked

#### Scenario: onError continuation succeeds
- **WHEN** a `requestCommand` fails with `commandFailed` and its `onError` returns a new `requestCommand` that succeeds and whose `onResponse` returns `success()`
- **THEN** the SBMD Transaction commits and `onTransactionFailure` is not invoked, although `args.operations` would contain a `failed` entry

#### Scenario: Recovered transaction fails later
- **WHEN** an `onError` returns a continuation that is later rejected by the device and that hop's `onError` returns `error()`
- **THEN** `onTransactionFailure` is invoked exactly once, with `args.failure` describing the later failure

#### Scenario: Recovery ignored for timeout
- **WHEN** a `requestCommand` times out and its `onError` returns `success()`
- **THEN** the recovery terminal is ignored, the transaction fails with `timeout`, and `onTransactionFailure` is invoked once

### Requirement: Failure handler ordering
The runtime SHALL invoke `onTransactionFailure` only after the failing step's `onError` handler, if the failure was delivered to an armed deferred hop, has completed, and only after the failed SBMD Transaction's data session (its DAL Transaction) has been aborted and its staged writes discarded. The failed SBMD Transaction's caller, if any, SHALL be unblocked with the original failure after the failure handler has been scheduled; the caller SHALL NOT wait for the failure handler or its compensation chain to complete.

#### Scenario: onError runs first
- **WHEN** a deferred `requestCommand` times out and its `onError` returns `error`
- **THEN** `onError` completes before `onTransactionFailure` is invoked

#### Scenario: Handler does not see discarded writes
- **WHEN** a transaction stages `setTransientData("k", "pending", 60)`, then fails
- **THEN** a transient-data supplement for `k` delivered to `onTransactionFailure` does not contain `"pending"`

### Requirement: Failure handler arguments
The failure handler SHALL receive `args` containing the common handler fields (`deviceUuid`, `endpointId`, `clusterFeatureMaps`), `args.resource` when the failed transaction originated from a resource operation or seed, and:
- `args.operations`: the ordered operation log of the failed SBMD Transaction. Each entry SHALL be `{ step, kind, endpoint, cluster, command | attribute, request, status, response?, error? }` where `kind` is `"command"`, `"readAttribute"`, or `"writeAttribute"`; `endpoint` is the resolved endpoint or `null`; `request` is the base64 TLV payload supplied by the driver or `null`; `status` is `"succeeded"`, `"failed"`, or `"notSent"`; `response` is the base64 TLV command response or attribute value for a succeeded operation; `error` is `{ type, message, matterCode }` for a failed or not-sent operation.
- `args.failure`: `{ step, type, message, matterCode? }` where `type` is one of `"timeout"`, `"commandFailed"`, `"readFailed"`, `"transport"`, `"internal"`, `"aborted"`, `"commitFailed"`.
- `args.context`: the driver context of the failed transaction — the `context` value supplied on its first deferred request — or `null` if none was supplied.
- `args.supplements`: the handler's declared supplements, fetched when the handler runs.

When the failure is a handler `error` terminal returned by `onError`, `args.failure.type` SHALL be the type of the error that `onError` was handling; otherwise an `error` terminal SHALL be reported as `"internal"` with the driver's message.

#### Scenario: Operation log contents
- **WHEN** a transaction sends `SetUser` (succeeds), `SetCredential` (succeeds with a response), and `SetWeekDaySchedule` (rejected)
- **THEN** `args.operations` has three `command` entries in that order with statuses `succeeded`, `succeeded`, `failed`; the second has a `response` payload; the third has an `error`

#### Scenario: Not-sent operation
- **WHEN** a transaction fails because a `requestCommand` targets a cluster no endpoint supports
- **THEN** the last `args.operations` entry has `status` `"notSent"`, `endpoint` `null`, and `error.type` `"internal"`

#### Scenario: Context forwarded
- **WHEN** the failed transaction's first `requestCommand` supplied `context: { sessionId: 7 }`
- **THEN** `args.context.sessionId` is `7`

#### Scenario: Fresh supplements
- **WHEN** another transaction commits a new value for a key declared in the handler's supplements after the failed transaction's last step
- **THEN** the handler's `args.supplements` contains the new value

### Requirement: Compensation runs as a new SBMD Transaction without failure handling
The failure handler SHALL run as the first step of a new SBMD Transaction that has failure handling disabled. The handler's result chain — `log` operations, data writes, and any terminal including device operations and deferred chains — SHALL execute in that new transaction with the normal SBMD Transaction semantics: data writes are committed only if it succeeds, and device operations are recorded in its own operation log. If the compensation transaction fails, including when the handler throws or returns no result, the runtime SHALL log the failure and its operation log and record a metric, and SHALL NOT invoke `onTransactionFailure` again. Compensation SHALL run on the SBMD worker thread.

#### Scenario: Compensating command
- **WHEN** `onTransactionFailure` returns `Sbmd.result().device.sendCommand(CL_DOOR_LOCK, CMD_CLEAR_USER, payload)`
- **THEN** a new SBMD Transaction sends `ClearUser` and commits when the device accepts it

#### Scenario: Compensation failure is not recursive
- **WHEN** the compensating `ClearUser` is rejected by the device
- **THEN** the compensation transaction fails, the failure is logged, and `onTransactionFailure` is not invoked again

#### Scenario: Compensation data writes
- **WHEN** `onTransactionFailure` returns `Sbmd.result().storage.setTransientData("sessions", "{}", 3600).success()`
- **THEN** `sessions` is set to `"{}"` when the compensation transaction commits

### Requirement: Door lock provisioning compensation
A driver SHALL be able to compensate a partially applied multi-step device change using only `args.operations`. In the door lock case — `SetUser` (cluster 0x0101, command 0x1A) succeeds, `SetCredential` (0x22, response 0x23) succeeds, and `SetWeekDaySchedule` (0x0B) fails — the handler SHALL be able to read the user index from the logged `SetUser` request and send `ClearUser` (0x1D), which removes the user together with its credentials and schedules.

#### Scenario: Door lock user removed after schedule failure
- **WHEN** steps 1 and 2 of an add-user transaction for user index 5 succeed and step 3 `SetWeekDaySchedule` fails
- **THEN** `onTransactionFailure` decodes `UserIndex` 5 from the logged `SetUser` request and exactly one `ClearUser` with `UserIndex` 5 is sent in a compensation transaction
