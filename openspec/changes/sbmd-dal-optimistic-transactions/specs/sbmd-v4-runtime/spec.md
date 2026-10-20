## MODIFIED Requirements

### Requirement: Result chain execution
After a handler returns, the runtime SHALL process all operations in the `ops` array in order, then execute the terminal. The runtime SHALL support the following operation types: `updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `mergeTransientData`, `log`. `log` operations SHALL execute immediately. Data write operations SHALL be staged in the operation's Data Access Layer transaction and committed only if the operation completes successfully. Unknown operation types SHALL be logged as warnings and skipped.

#### Scenario: Operations execute in order
- **WHEN** a result contains `[updateResource, log, setPersistentData]` followed by `success`
- **THEN** the message is logged, and on success the resource update and persisted data are committed in that order

#### Scenario: Log executes on error terminal, writes are discarded
- **WHEN** a result contains `[log("diagnostic"), setTransientData("k", "v", 60)]` followed by `error("failed")`
- **THEN** the log message is emitted, `k` is not modified, and the operation is marked as failed
