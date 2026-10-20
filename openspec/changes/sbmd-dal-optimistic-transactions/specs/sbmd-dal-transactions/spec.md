## ADDED Requirements

### Requirement: Implicit per-operation transactions
The SBMD runtime SHALL execute each resource operation (read, write, execute), each device-initiated handler dispatch, and each seed invocation within a Data Access Layer transaction that it creates and finishes. A transaction SHALL span all deferred hops of an operation. Drivers SHALL NOT declare, see, or control transaction scope.

#### Scenario: Transaction spans a park
- **WHEN** a write handler returns `requestCommand` and its `onResponse` returns `success`
- **THEN** reads and writes from both the handler and `onResponse` SHALL belong to the same transaction

---

### Requirement: Automatic read and write set tracking
The DAL SHALL record the version of each key the first time a transaction reads it, and SHALL stage every data write op (`updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `mergeTransientData`) in the transaction's write set in op order instead of applying it. Reads within a transaction SHALL observe the transaction's own staged writes.

#### Scenario: Read-your-writes
- **WHEN** a handler stages `setTransientData("k", "v", 60)` and a later hop in the same operation reads `k`
- **THEN** the read SHALL return `"v"`

#### Scenario: Writes not visible before commit
- **WHEN** a parked operation has staged a write to `k`
- **THEN** other operations reading `k` SHALL NOT observe the staged value

---

### Requirement: Commit only on success
Staged writes SHALL be committed atomically only when the operation completes successfully. They SHALL be discarded when the operation ends with an `error` terminal, a deferred failure, a timeout, or an unmergeable conflict. `log` ops SHALL execute immediately regardless. Writes returned by an `onError` handler SHALL be committed in a separate compensation transaction.

#### Scenario: Error terminal discards writes
- **WHEN** a handler returns `setTransientData("k", "v", 60)` followed by `error("failed")`
- **THEN** `k` SHALL NOT be modified

#### Scenario: onError writes are committed
- **WHEN** a deferred command fails and `onError` returns `mergeTransientData("sessions", {"3": null}, 3600)` with `error(...)`
- **THEN** member `3` SHALL be removed from `sessions`

---

### Requirement: Commit-time validation and pre-park retry
At commit, and immediately before the first device side effect (sending a command or writing an attribute), the DAL SHALL validate that every key in the read set still has the recorded version. If validation fails before any device side effect, the runtime SHALL discard staged writes and transparently re-run the handler with fresh data, at most 3 times, after which the operation SHALL fail with error type `"conflict"`.

#### Scenario: Pre-park conflict retried
- **WHEN** a handler reads `sessions`, another operation changes `sessions` before the command is sent, and validation fails
- **THEN** the handler SHALL be re-invoked with the new `sessions` and the device command SHALL be sent exactly once

---

### Requirement: Post-park conflicts resolved by merge, never retried
After a device side effect has occurred, the runtime SHALL NOT retry the operation. On a commit-time conflict, the DAL SHALL commit if every conflicting key is read-only in the transaction, written only by merge-patch ops (re-applied to the current value), or written with a value equal to the current value. Otherwise the conflict SHALL be unmergeable: staged writes SHALL be discarded, the operation SHALL fail, and the driver failure path SHALL be invoked exactly once with error type `"conflict"`.

#### Scenario: Merge writes survive concurrent change
- **WHEN** a parked operation stages `mergeTransientData("sessions", {"1": {"videoStreamID": 5}})` and another operation adds session `2` before commit
- **THEN** the commit SHALL succeed and `sessions` SHALL contain both session `1` with `videoStreamID` 5 and session `2`

#### Scenario: Whole-value write over concurrent change fails
- **WHEN** a parked operation read `sessions`, another operation changed `sessions`, and the parked operation stages `setTransientData("sessions", staleJson, 3600)`
- **THEN** the commit SHALL NOT apply `staleJson`, the operation SHALL fail, and `onError` SHALL be invoked once with `args.error.type == "conflict"`

#### Scenario: No replay of device commands
- **WHEN** a post-park conflict occurs
- **THEN** no device command from the operation SHALL be re-sent
