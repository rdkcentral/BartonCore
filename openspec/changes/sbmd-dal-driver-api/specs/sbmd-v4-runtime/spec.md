## MODIFIED Requirements

### Requirement: Result chain execution
After a handler returns, the runtime SHALL execute all operations in the `ops` array in order, then execute the terminal. The runtime SHALL support the following operation types: `updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `mergeTransientData`, `log`. Data write operations SHALL be applied through the SBMD Data Access Layer against the value current at execution time. Unknown operation types SHALL be logged as warnings and skipped.

#### Scenario: Operations execute in order
- **WHEN** a result contains `[updateResource, log, setPersistentData]` followed by `success`
- **THEN** the resource is updated, the message is logged, the data is persisted, and the operation completes successfully — in that order

#### Scenario: Operations execute even on error terminal
- **WHEN** a result contains `[log("diagnostic")]` followed by `error("failed")`
- **THEN** the log message is emitted, then the operation is marked as failed

#### Scenario: mergeTransientData supported
- **WHEN** a result contains `mergeTransientData("sessions", {"2": {"state": "created"}}, 3600)`
- **THEN** the patch is merged into the current `sessions` value

### Requirement: Deferred operations
`requestCommand` and `readAttribute` terminals SHALL park the resource operation and register pending response state. When a matching response arrives, the stored handler function SHALL be called with the response data, the original trigger context, and supplements read from the Data Access Layer at callback time. The handler's result chain SHALL be executed to complete the parked operation.

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
- **THEN** the current hop's `onError` handler is called with an appropriate error

#### Scenario: Deferred handler receives fresh supplements
- **WHEN** a deferred handler is invoked and supplements were declared for it
- **THEN** `args.supplements` reflects the Data Access Layer view at invocation time
