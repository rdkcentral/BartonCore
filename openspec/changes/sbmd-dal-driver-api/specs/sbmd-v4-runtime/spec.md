## MODIFIED Requirements

<!-- Archive order for these shared requirements: BARTON-423 (sbmd-transaction-orchestrator) → BARTON-422 (this change) → BARTON-424. "Result chain execution" and "Deferred operations" restate the text as BARTON-423 leaves it, plus this change's additions. "Supplements pre-loading" is not modified by BARTON-423 and restates the baseline plus additions. "Deferred failure callback delivery" is not changed by this change. -->

### Requirement: Supplements pre-loading
When a handler declares `supplements`, the runtime SHALL resolve alias names to cluster+attribute IDs, read attribute values from the device data cache, read resource values from the Barton resource store, read transient data, and deliver them in `args.supplements` before calling the handler. Supplements SHALL be fetched through the SBMD Transaction's data session in one batched fetch per step, at that step's time. When the declaration includes `sequence` (a transient key name or array of names), the runtime SHALL atomically increment each named counter during that fetch and deliver the new value as a number in `args.supplements.sequence`; sequence allocations SHALL NOT be rolled back if the SBMD Transaction fails.

#### Scenario: Attribute supplement loaded from cache
- **WHEN** a `seed` handler declares `supplements: { attributes: ["onOff"] }` and the device data cache has a value for the `onOff` alias
- **THEN** `args.supplements.attributes.onOff` contains the decoded attribute value

#### Scenario: Resource supplement loaded
- **WHEN** a handler declares `supplements: { resources: ["1/isOn"] }`
- **THEN** `args.supplements.resources["1/isOn"]` contains the current Barton resource value

#### Scenario: Sequence supplement allocated
- **WHEN** a handler declares `supplements: { sequence: "nextId" }` and the counter `nextId` currently holds `"4"`
- **THEN** `args.supplements.sequence.nextId` is `5` and `nextId` holds `"5"` even if the SBMD Transaction later fails

### Requirement: Result chain execution
After a handler returns, the runtime SHALL process all operations in the `ops` array in order, then execute the terminal. The runtime SHALL support the following operation types: `updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `setTransientDataIf`, `mergeTransientData`, `log`. `log` operations SHALL execute immediately. Data write operations (`updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `setTransientDataIf`, `mergeTransientData`) SHALL be staged, in order, in the data session owned by the handler's SBMD Transaction and SHALL take effect only if the SBMD Transaction commits; if the SBMD Transaction fails for any reason, including an `error` terminal, its staged writes SHALL be discarded. `setTransientDataIf` SHALL be staged as a conditional write; if its expected value does not match at commit, the commit SHALL fail and the SBMD Transaction SHALL fail with type `commitFailed` and reason `conditionFailed`. `mergeTransientData` SHALL be resolved at commit against the value current at commit using RFC 7396 JSON Merge Patch semantics. Unknown operation types SHALL be logged as warnings and skipped.

#### Scenario: Operations execute in order
- **WHEN** a result contains `[updateResource, log, setPersistentData]` followed by `success`
- **THEN** the message is logged immediately, and when the SBMD Transaction commits the resource update and the persisted data are applied in that order

#### Scenario: Log executes on error terminal, writes are discarded
- **WHEN** a result contains `[log("diagnostic"), setTransientData("k", "v", 60)]` followed by `error("failed")`
- **THEN** the log message is emitted, `k` is not modified, and the operation is marked as failed

#### Scenario: Writes from an earlier step are discarded when a later step fails
- **WHEN** a handler stages `updateResource` and returns `requestCommand`, and the response handler returns `error`
- **THEN** the resource is not updated and the operation is marked as failed

#### Scenario: mergeTransientData supported
- **WHEN** a result contains `mergeTransientData("sessions", {"2": {"state": "created"}}, 3600)` followed by `success`
- **THEN** the patch is merged into the `sessions` value current at commit

#### Scenario: Conditional write mismatch fails the commit
- **WHEN** a result contains `setTransientDataIf("k", "a", "b", 60)` followed by `success` and `k` is `"c"` at commit
- **THEN** no staged write of the SBMD Transaction takes effect and the operation fails with type `commitFailed` and reason `conditionFailed`

### Requirement: Deferred operations
`requestCommand` and `readAttribute` terminals SHALL park the resource operation within its SBMD Transaction and arm the transaction's current hop with the supplied `onResponse` and `onError` handlers. When the device outcome for the hop is available, it SHALL be handed back to the SBMD Transaction's orchestrator, which SHALL invoke the hop's handler as the next step with the response data, the original trigger context, and supplements fetched for that step. The supplement declaration for the hop SHALL be the terminal's `supplements` option if present, otherwise the declaration effective for the step that returned the terminal. The handler's result chain SHALL be executed to complete the parked operation. When a deferred operation fails for any reason after its handlers are supplied, the `onError` handler of the current hop SHALL be called as defined by "Deferred failure callback delivery".

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

#### Scenario: Deferred handler inherits supplements
- **WHEN** a handler declaring `supplements: { transientData: ["sessions"] }` returns `requestCommand` without a `supplements` option, and another SBMD Transaction commits a change to `sessions` while it is parked
- **THEN** `onResponse` receives the committed value in `args.supplements.transientData.sessions`

#### Scenario: Deferred supplements override
- **WHEN** a `requestCommand` specifies `supplements: { resources: ["1/stream"] }`
- **THEN** its `onResponse` and `onError` receive only the `resources` supplements declared in the option
