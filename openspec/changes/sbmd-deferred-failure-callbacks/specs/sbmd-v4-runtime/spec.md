## MODIFIED Requirements

### Requirement: Deferred operations
`requestCommand` and `readAttribute` terminals SHALL park the resource operation and register pending response state. When a matching response arrives, the stored handler function SHALL be called with the response data and the original trigger context. The handler's result chain SHALL be executed to complete the parked operation. When a deferred operation fails for any reason after its handlers are supplied, the `onError` handler of the current hop SHALL be called as defined by "Deferred failure callback delivery".

#### Scenario: requestCommand parks and completes on response
- **WHEN** a handler returns `requestCommand` with `responseCommandId: 26` and later a command with ID 26 arrives on the matching cluster
- **THEN** the response handler is called, its result executes, and the parked resource operation completes

#### Scenario: Timeout fires onError
- **WHEN** a `requestCommand` specifies `timeoutMs: 5000` and no matching response arrives within 5 seconds
- **THEN** the `onError` handler is called with `args.error.type` set to `"timeout"`

#### Scenario: Deferred handler returns another deferral
- **WHEN** a deferred response handler returns a new `requestCommand`
- **THEN** the pending state is re-armed with the new match criteria, handlers, and timer without creating nested structures

#### Scenario: Overall operation timeout
- **WHEN** a chain of deferrals exceeds the overall operation deadline (`matter.defaultTimeoutMs`)
- **THEN** the `onError` handler of the current hop is called with `type: "timeout"` regardless of per-hop timeouts

#### Scenario: Max deferral depth exceeded
- **WHEN** a chain of deferrals exceeds the maximum deferral depth
- **THEN** the newly armed hop's `onError` handler is called with `args.error.type` `"internal"`, its ops execute, any recovery terminal it returns is ignored, and the parked operation fails

## ADDED Requirements

### Requirement: Deferred failure callback delivery
For every deferred hop (a `requestCommand` or `readAttribute` whose `onResponse`/`onError` handlers have been supplied), the runtime SHALL invoke exactly one of that hop's `onResponse` or `onError` handlers, at most once, on every outcome path. This includes failures that occur before the command is sent: endpoint resolution failure, invalid base64/TLV payload, expired session, device removed, send rejected by the Matter stack, and maximum deferral depth. The runtime SHALL NOT invoke `onError` for a hop whose `onResponse` has already been invoked, and SHALL NOT invoke either handler of a hop that has been superseded by a re-arm. Failures caused by a non-deferred terminal (`sendCommand`, `writeAttribute`) — including one returned from `onResponse` — SHALL fail the parked operation without invoking any `onError`.

The `onError` handler SHALL receive `args.error = { message, type, matterCode }` where `type` is one of `"timeout"`, `"commandFailed"`, `"readFailed"`, `"transport"`, or `"internal"`, and `matterCode` is the Matter SDK error code or `null`. Pre-send local failures (endpoint, payload, depth, device removed) SHALL use `"internal"`; expired session and send rejection SHALL use `"transport"`, with `matterCode` set to the `CHIP_ERROR` value when one is available.

The `onError` handler and its ops SHALL execute on the Matter event-loop thread before the parked resource operation's caller is unblocked. If `onError` returns a non-`error` terminal, the runtime SHALL continue the chain with that terminal for `"internal"`, `"transport"`, `"commandFailed"`, and `"readFailed"` failures, and SHALL ignore it (failing the operation) for `"timeout"` and maximum-deferral-depth failures. The parked operation's result SHALL be delivered to the caller exactly once.

#### Scenario: Endpoint resolution failure invokes onError
- **WHEN** a handler returns `requestCommand` for a cluster that no endpoint on the device supports
- **THEN** the `onError` handler is called once with `args.error.type` `"internal"` and `args.error.matterCode` `null`, its ops execute, and the parked operation fails

#### Scenario: Invalid payload invokes onError
- **WHEN** a handler returns `requestCommand` whose TLV payload is not valid base64 or not a TLV structure
- **THEN** the `onError` handler is called once with `args.error.type` `"internal"` and the parked operation fails

#### Scenario: Send rejected by the Matter stack invokes onError
- **WHEN** a handler returns `requestCommand` and the Matter stack rejects the command before it is transmitted
- **THEN** the `onError` handler is called once with `args.error.type` `"transport"` and `args.error.matterCode` set to the Matter SDK error code, and the parked operation fails

#### Scenario: Expired session on re-arm invokes the re-armed hop's onError
- **WHEN** an `onResponse` handler returns a new `requestCommand` and the device session has expired before it can be sent
- **THEN** the new `requestCommand`'s `onError` handler is called once with `args.error.type` `"transport"`, and the previous hop's `onError` is not called

#### Scenario: Device removed during chain invokes onError
- **WHEN** an `onResponse` handler returns a new `requestCommand` after the device has been removed
- **THEN** the new `requestCommand`'s `onError` handler is called once with `args.error.type` `"internal"`

#### Scenario: Synchronous readAttribute endpoint failure invokes onError
- **WHEN** a handler returns `readAttribute` for a cluster that no endpoint on the device supports
- **THEN** the `onError` handler is called once with `args.error.type` `"internal"`

#### Scenario: Rollback is visible when the caller is unblocked
- **WHEN** a handler persists state with `storage.setTransientData`, returns `requestCommand`, the send fails, and `onError` writes a rollback value with `storage.setTransientData`
- **THEN** the rollback value is stored before the resource operation returns failure to its caller

#### Scenario: onResponse and onError never both run
- **WHEN** a deferred command's response is delivered to `onResponse` and the Matter stack subsequently reports an error for the same exchange
- **THEN** `onError` is not called for that hop and the parked operation's result is delivered once

#### Scenario: Stale callback from superseded hop is ignored
- **WHEN** a hop has been re-armed with a new `requestCommand` and a late response or error arrives for the previous command
- **THEN** neither the previous nor the current hop's handlers are invoked by that late callback

#### Scenario: onError recovery terminal after pre-send failure
- **WHEN** a `requestCommand` send is rejected and its `onError` returns `.success("fallback")`
- **THEN** the parked operation completes successfully with `"fallback"`

#### Scenario: onError recovery that fails again is bounded
- **WHEN** `onError` repeatedly returns a new `requestCommand` that also fails before sending
- **THEN** each re-armed hop's `onError` is called at most once, and once maximum deferral depth is reached the operation fails without further recovery

#### Scenario: Camera send failure does not strand a streaming session
- **WHEN** the camera driver posts `localSdp`, persists the session, and the `VideoStreamAllocate` `requestCommand` is rejected before sending
- **THEN** `handleVideoStreamAllocateError` is called exactly once with `args.error.type` `"transport"` and `args.handlerContext.sessionId` identifying the persisted session, before the `localSdp` operation returns failure
