## ADDED Requirements

### Requirement: Every SBMD Transaction step receives fresh supplements
For every step of an SBMD Transaction — the initial handler and each `requestCommand` or `readAttribute` `onResponse` or `onError` callback (including timeout, pre-send failure, and depth-limit errors) — the runtime SHALL populate `args.supplements` by fetching through the SBMD Transaction's data session at that step's time. For a deferred callback, the declaration SHALL be the deferred terminal's `supplements` option if present, otherwise the declaration effective for the step that returned the terminal. The driver-level `onTransactionFailure` handler SHALL receive supplements fetched under the same rules using its own declaration. Each step SHALL observe values committed by other SBMD Transactions and its own SBMD Transaction's staged writes. If no declaration applies, `args.supplements` SHALL be absent. `args.handlerContext` SHALL carry the `context` value unchanged.

#### Scenario: Concurrent change visible after park
- **WHEN** handler A declares `supplements: { transientData: ["sessions"] }`, returns `requestCommand`, and while parked another SBMD Transaction B commits a change to `sessions`
- **THEN** A's `onResponse` SHALL receive B's committed value in `args.supplements.transientData.sessions`

#### Scenario: Error callback receives fresh supplements
- **WHEN** a parked `requestCommand` fails, times out, or fails before sending, and its effective declaration includes supplements
- **THEN** `onError` SHALL receive `args.supplements` read at callback time

#### Scenario: Option overrides inherited declaration
- **WHEN** a `requestCommand` specifies `supplements: { resources: ["1/stream"] }`
- **THEN** its callbacks SHALL receive only the `resources` supplements declared in the option

#### Scenario: Step sees its own staged writes
- **WHEN** step 1 stages `mergeTransientData("sessions", {"2": {"state": "allocating"}}, 3600)` and returns `requestCommand`
- **THEN** step 2's `args.supplements.transientData.sessions` SHALL include member `2` with state `allocating`

---

### Requirement: Sequence supplements
A handler SHALL be able to declare `supplements: { sequence: <name> }` where `<name>` is a transient key name or an array of names. During the step's batched supplement fetch the runtime SHALL atomically increment each named counter and deliver the new value as a number in `args.supplements.sequence[<name>]`. A missing counter SHALL start at 1. Allocation SHALL take effect immediately and SHALL NOT be rolled back when the SBMD Transaction aborts or its commit fails.

#### Scenario: Concurrent allocations are distinct
- **WHEN** two SBMD Transactions on the same device concurrently invoke a handler declaring `supplements: { sequence: "nextId" }`
- **THEN** they SHALL receive different values of `args.supplements.sequence.nextId`

#### Scenario: Allocation survives failure
- **WHEN** a handler receives sequence value 5 and returns `error`
- **THEN** the next allocation of the same counter SHALL return 6

---

### Requirement: Conditional transient write
The result builder SHALL provide `storage.setTransientDataIf(name, expected, value, ttlSecs)`. The write SHALL be staged and SHALL apply at commit only if the current value of `name` equals `expected` (`null` meaning the key is absent). If any condition does not hold at commit, no staged write of the SBMD Transaction SHALL take effect, the SBMD Transaction SHALL fail with failure type `commitFailed` and reason `conditionFailed`, and the driver's `onTransactionFailure` handler SHALL run.

#### Scenario: Condition holds
- **WHEN** `k` is `"a"` and a handler returns `setTransientDataIf("k", "a", "b", 60)` followed by `success`
- **THEN** after commit `k` SHALL be `"b"`

#### Scenario: Condition fails after a concurrent change
- **WHEN** a handler reads `k` as `"a"`, another SBMD Transaction commits `k = "c"`, and the first handler's result contains `setTransientDataIf("k", "a", "b", 60)` and `setTransientData("other", "x", 60)` followed by `success`
- **THEN** `k` SHALL remain `"c"`, `other` SHALL be unchanged, the operation SHALL fail, and `onTransactionFailure` SHALL receive `args.failure.type` `"commitFailed"` with a message identifying `conditionFailed`

---

### Requirement: Per-member transient data merge
The result builder SHALL provide `storage.mergeTransientData(name, patch, ttlSecs)`. The merge SHALL be staged and resolved at commit by applying `patch` to the value of `name` current at commit using RFC 7396 JSON Merge Patch semantics, treating an absent or non-object current value as `{}`, and storing the result with the given TTL. A `null` member SHALL delete that member. Arrays SHALL be replaced. `ttlSecs` of 0 SHALL delete the key. A non-object patch SHALL be rejected by the builder.

#### Scenario: Merge preserves other members
- **WHEN** `sessions` is `{"1":{"state":"streaming"}}` and a handler merges `{"2":{"state":"created"}}`
- **THEN** after commit `sessions` SHALL be `{"1":{"state":"streaming"},"2":{"state":"created"}}`

#### Scenario: Null deletes a member
- **WHEN** a handler merges `{"1":null}` into `sessions`
- **THEN** after commit member `1` SHALL be removed and other members SHALL be preserved

#### Scenario: Merge applies against value current at commit
- **WHEN** a deferred step merges a patch after another SBMD Transaction committed a change to the key during the park
- **THEN** the patch SHALL be applied to the changed value

---

### Requirement: Interleaving guidance
The SBMD driver documentation SHALL state that an unconditional read-compute-write of a key across separate SBMD Transactions can lose an update, and SHALL direct driver authors to sequence supplements for counters, `mergeTransientData` for members of JSON objects, and `setTransientDataIf` for whole-value updates derived from a read.

#### Scenario: Documentation lists the safe primitives
- **WHEN** a driver author reads the data-access section of `docs/SBMD.md`
- **THEN** it SHALL describe the limitation and the three primitives with examples
