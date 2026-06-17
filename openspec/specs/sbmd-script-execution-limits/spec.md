## MODIFIED Requirements

### Requirement: Script timeout enforcement for handler invocations
The mquickjs interrupt handler SHALL enforce per-invocation timeouts for v4 handler function calls, using the same `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS` configuration as v3 mapper scripts. The deadline SHALL be set before each handler call and cleared immediately after.

#### Scenario: Handler exceeds timeout
- **WHEN** a handler function runs longer than `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS`
- **THEN** the mquickjs interrupt handler terminates execution and the runtime reports the operation as failed

## ADDED Requirements

### Requirement: Overall operation timeout for deferred chains
The runtime SHALL enforce an overall operation deadline for resource operations that involve deferred chains. The deadline SHALL be set when the first deferral occurs (from `matter.defaultTimeoutMs` or a system default) and SHALL NOT reset on subsequent deferrals. Per-hop `timeoutMs` values SHALL be capped at the remaining overall budget.

#### Scenario: Overall timeout prevents runaway chains
- **WHEN** a deferred chain makes multiple successful hops but exceeds the overall deadline
- **THEN** the next deferral attempt triggers `onError` with `type: "timeout"` without sending the command

#### Scenario: Per-hop timeout capped by overall budget
- **WHEN** a deferral specifies `timeoutMs: 30000` but only 5000ms remain in the overall budget
- **THEN** the effective per-hop timeout is 5000ms

### Requirement: Maximum deferral depth
The runtime SHALL enforce a maximum deferral depth (configurable, default 10). When exceeded, the current hop's `onError` handler SHALL be called with an error indicating the depth limit was reached.

#### Scenario: Depth limit exceeded
- **WHEN** a deferred chain reaches the maximum deferral depth
- **THEN** the `onError` handler is called with a message indicating deferral depth exceeded and the parked operation completes with failure
