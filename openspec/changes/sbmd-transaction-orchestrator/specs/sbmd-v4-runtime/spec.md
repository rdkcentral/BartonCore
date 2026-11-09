## MODIFIED Requirements

### Requirement: Result chain execution
After a handler returns, the runtime SHALL process all operations in the `ops` array in order, then execute the terminal. The runtime SHALL support the following operation types: `updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `log`. `log` operations SHALL execute immediately. Data write operations (`updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`) SHALL be staged, in order, in the data session owned by the handler's SBMD Transaction and SHALL take effect only if the SBMD Transaction commits; if the SBMD Transaction fails for any reason, including an `error` terminal, its staged writes SHALL be discarded. Unknown operation types SHALL be logged as warnings and skipped.

#### Scenario: Operations execute in order
- **WHEN** a result contains `[updateResource, log, setPersistentData]` followed by `success`
- **THEN** the message is logged immediately, and when the SBMD Transaction commits the resource update and the persisted data are applied in that order

#### Scenario: Log executes on error terminal, writes are discarded
- **WHEN** a result contains `[log("diagnostic"), setTransientData("k", "v", 60)]` followed by `error("failed")`
- **THEN** the log message is emitted, `k` is not modified, and the operation is marked as failed

#### Scenario: Writes from an earlier step are discarded when a later step fails
- **WHEN** a handler stages `updateResource` and returns `requestCommand`, and the response handler returns `error`
- **THEN** the resource is not updated and the operation is marked as failed

### Requirement: Deferred operations
`requestCommand` and `readAttribute` terminals SHALL park the resource operation within its SBMD Transaction and arm the transaction's current hop with the supplied `onResponse` and `onError` handlers. When the device outcome for the hop is available, it SHALL be handed back to the SBMD Transaction's orchestrator, which SHALL invoke the hop's handler as the next step with the response data, the original trigger context, and supplements fetched for that step. The handler's result chain SHALL be executed to complete the parked operation. When a deferred operation fails for any reason after its handlers are supplied, the `onError` handler of the current hop SHALL be called as defined by "Deferred failure callback delivery".

#### Scenario: requestCommand parks and completes on response
- **WHEN** a handler returns `requestCommand` with `responseCommandId: 26` and later a command with ID 26 arrives on the matching cluster
- **THEN** the response handler is called by the orchestrator, its result executes, and the parked resource operation completes

#### Scenario: Timeout fires onError
- **WHEN** a `requestCommand` specifies `timeoutMs: 5000` and no matching response arrives within 5 seconds
- **THEN** the `onError` handler is called with `args.error.type` set to `"timeout"`

#### Scenario: Deferred handler returns another deferral
- **WHEN** a deferred response handler returns a new `requestCommand`
- **THEN** the SBMD Transaction's hop is re-armed with the new match criteria, handlers, and deadline without creating nested structures

#### Scenario: Overall operation timeout
- **WHEN** a chain of deferrals exceeds the overall operation deadline (`matter.defaultTimeoutMs`)
- **THEN** the orchestrator stops waiting at the deadline and the `onError` handler of the current hop is called with `type: "timeout"` regardless of per-hop timeouts

#### Scenario: Max deferral depth exceeded
- **WHEN** a chain of deferrals exceeds the maximum deferral depth
- **THEN** the newly armed hop's `onError` handler is called with `args.error.type` `"internal"`, its `log` ops execute, any recovery terminal it returns is ignored, and the parked operation fails

#### Scenario: Deferred handler is not run on the Matter SDK callback
- **WHEN** a response for a parked `requestCommand` arrives
- **THEN** the Matter SDK callback only hands the outcome to the orchestrator, and `onResponse` runs afterwards as an orchestrator step

### Requirement: Deferred failure callback delivery
For every deferred hop (a `requestCommand` or `readAttribute` whose `onResponse`/`onError` handlers have been supplied), the runtime SHALL invoke exactly one of that hop's `onResponse` or `onError` handlers, at most once, on every outcome path except an SBMD Transaction abort. This includes failures that occur before the command is sent: endpoint resolution failure, invalid base64/TLV payload, expired session, send rejected by the Matter stack, and maximum deferral depth. The runtime SHALL NOT invoke `onError` for a hop whose `onResponse` has already been invoked, and SHALL NOT invoke either handler of a hop that has been superseded by a re-arm. Failures caused by a non-deferred terminal (`sendCommand`, `writeAttribute`) — including one returned from `onResponse` — SHALL fail the SBMD Transaction without invoking any `onError`. When the SBMD Transaction is aborted (device removed, driver deactivated, shutdown, or the data session's backing store aborted it), no `onError` SHALL be invoked and the transaction SHALL fail with type `aborted`.

The `onError` handler SHALL receive `args.error = { message, type, matterCode }` where `type` is one of `"timeout"`, `"commandFailed"`, `"readFailed"`, `"transport"`, or `"internal"`, and `matterCode` is the Matter SDK error code or `null`. Pre-send local failures (endpoint, payload, depth) SHALL use `"internal"`; expired session and send rejection SHALL use `"transport"`, with `matterCode` set to the `CHIP_ERROR` value when one is available.

The `onError` handler SHALL be invoked by the SBMD Transaction's orchestrator as a step, on the Matter thread, with supplements fetched for that step, before the parked resource operation's caller is unblocked. Its `log` ops SHALL execute immediately and its data writes SHALL be staged in the SBMD Transaction's data session. If `onError` returns a non-`error` terminal, the runtime SHALL honor it for `"internal"`, `"transport"`, `"commandFailed"`, and `"readFailed"` failures and SHALL ignore it (failing the operation) for `"timeout"` and maximum-deferral-depth failures. An honored terminal means the driver has recovered: `success` completes the SBMD Transaction successfully and a continuation terminal continues the chain, and in neither case is the transaction handed to the transaction failure handler unless it later terminates in failure. If the SBMD Transaction ultimately fails, writes staged by `onError` SHALL be discarded together with the rest of the transaction's staged writes. The parked operation's result SHALL be delivered to the caller exactly once.

#### Scenario: Endpoint resolution failure invokes onError
- **WHEN** a handler returns `requestCommand` for a cluster that no endpoint on the device supports
- **THEN** the `onError` handler is called once with `args.error.type` `"internal"` and `args.error.matterCode` `null`, its ops are processed, and the parked operation fails

#### Scenario: Invalid payload invokes onError
- **WHEN** a handler returns `requestCommand` whose TLV payload is not valid base64 or not a TLV structure
- **THEN** the `onError` handler is called once with `args.error.type` `"internal"` and the parked operation fails

#### Scenario: Send rejected by the Matter stack invokes onError
- **WHEN** a handler returns `requestCommand` and the Matter stack rejects the command before it is transmitted
- **THEN** the `onError` handler is called once with `args.error.type` `"transport"` and `args.error.matterCode` set to the Matter SDK error code, and the parked operation fails

#### Scenario: Expired session on re-arm invokes the re-armed hop's onError
- **WHEN** an `onResponse` handler returns a new `requestCommand` and the device session has expired before it can be sent
- **THEN** the new `requestCommand`'s `onError` handler is called once with `args.error.type` `"transport"`, and the previous hop's `onError` is not called

#### Scenario: Device removed during chain aborts the transaction
- **WHEN** an SBMD Transaction is parked on a `requestCommand` and the device is removed
- **THEN** no `onError` is called, the transaction fails with `aborted`, and the parked operation fails

#### Scenario: Synchronous readAttribute endpoint failure invokes onError
- **WHEN** a handler returns `readAttribute` for a cluster that no endpoint on the device supports
- **THEN** the `onError` handler is called once with `args.error.type` `"internal"`

#### Scenario: onError writes are discarded when the transaction fails
- **WHEN** a handler returns `requestCommand`, the send fails, and `onError` stages `setTransientData("k", "rollback", 60)` and returns `error`
- **THEN** `k` is not modified, and the resource operation returns failure to its caller after the transaction failure hand-off has run

#### Scenario: onResponse and onError never both run
- **WHEN** a deferred command's response is delivered to `onResponse` and the Matter stack subsequently reports an error for the same exchange
- **THEN** `onError` is not called for that hop and the parked operation's result is delivered once

#### Scenario: Stale callback from superseded hop is ignored
- **WHEN** a hop has been re-armed with a new `requestCommand` and a late response or error arrives for the previous command
- **THEN** neither the previous nor the current hop's handlers are invoked by that late callback

#### Scenario: onError recovery terminal after pre-send failure
- **WHEN** a `requestCommand` send is rejected and its `onError` returns `.success("fallback")`
- **THEN** the SBMD Transaction commits, the parked operation completes successfully with `"fallback"`, and the transaction is not handed to the failure handler

#### Scenario: onError recovery that fails again is bounded
- **WHEN** `onError` repeatedly returns a new `requestCommand` that also fails before sending
- **THEN** each re-armed hop's `onError` is called at most once, and once maximum deferral depth is reached the operation fails without further recovery
