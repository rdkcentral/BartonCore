## ADDED Requirements

### Requirement: DataKey identifies data by namespace, device, and path
The DAL SHALL identify every datum with a `DataKey` consisting of a namespace (`Attribute`, `Resource`, `Metadata`, `Property`, or `Transient`), a device ID (empty for device-independent namespaces), and a namespace-specific path. `DataKey` SHALL be hashable and totally ordered.

#### Scenario: Equal keys compare equal
- **WHEN** two `DataKey` values have the same namespace, device ID, and path
- **THEN** they SHALL compare equal and hash to the same value

#### Scenario: Keys differing only in namespace are distinct
- **WHEN** two `DataKey` values share device ID and path but differ in namespace
- **THEN** they SHALL compare unequal

### Requirement: The DAL holds no data store
The DAL SHALL NOT retain data values outside an open DAL Transaction. It SHALL hold only the plugin registry and, per open DAL Transaction, its plugin bindings, staged writes, and overlay values, and SHALL clear all of them when the transaction commits or aborts. Every read not satisfied by the transaction's own staged writes or overlay SHALL be served by a source plugin.

#### Scenario: No values retained after commit
- **WHEN** a DAL Transaction reads and writes keys and then commits
- **THEN** the DAL SHALL retain no value for any of those keys

#### Scenario: Reads go to the plugin
- **WHEN** a DAL Transaction reads a key it has not staged or overlaid
- **THEN** the DAL SHALL call the owning plugin binding's `Read` and return its result

### Requirement: Plugin registry
The DAL SHALL provide a `DalPluginRegistry` confined to the device-service main loop. `Register` and `Unregister` SHALL be callable from any thread and SHALL be marshaled to the main loop. Each plugin SHALL declare a scope (namespace and optional device ID), capabilities (readable, writable), and an executor affinity (`MainLoop` or `MatterThread`). The registry SHALL reject a plugin whose scope exactly overlaps a registered plugin's scope. Key resolution SHALL prefer a device-scoped plugin over a namespace-wide plugin. The DAL SHALL NOT unregister plugins on its own; the component that registered a plugin is responsible for unregistering it.

#### Scenario: Device-scoped plugin wins
- **WHEN** a namespace-wide plugin and a plugin scoped to device `D` are both registered for `Attribute`, and a key for device `D` is resolved
- **THEN** the device-scoped plugin SHALL serve the key

#### Scenario: Duplicate scope rejected
- **WHEN** a plugin is registered with the same namespace and device ID as an existing plugin
- **THEN** registration SHALL fail and be logged, and the existing plugin SHALL remain registered

#### Scenario: Register from a non-main-loop thread
- **WHEN** `Register` is called from a thread other than the main loop
- **THEN** the registration SHALL be performed on the main loop

#### Scenario: No plugin for a key
- **WHEN** a DAL Transaction reads a key no registered plugin serves
- **THEN** the read SHALL fail with failure type `NoPlugin`

### Requirement: Unregistering a plugin aborts bound transactions
When a plugin is unregistered, the DAL SHALL abort every open DAL Transaction bound to that plugin with failure type `aborted`, SHALL call `Release()` exactly once on each of that plugin's bindings, and SHALL then remove the plugin. Unregistering an unknown or already-unregistered plugin SHALL be a no-op.

#### Scenario: Bound transaction aborted
- **WHEN** a DAL Transaction has read from plugin `P` and `P` is unregistered before the transaction ends
- **THEN** the transaction SHALL be aborted with failure type `aborted` and any later operation on it SHALL fail with `aborted`

#### Scenario: Unbound transaction unaffected
- **WHEN** a DAL Transaction has never touched plugin `P` and `P` is unregistered
- **THEN** the transaction SHALL remain open

### Requirement: Lazy per-plugin binding
A DAL Transaction SHALL bind to a plugin (calling `Bind(DalTransactionId)`) on its first read or write that resolves to that plugin, SHALL reuse that binding for the rest of the transaction, and SHALL release every binding exactly once when the transaction ends.

#### Scenario: Single bind per plugin
- **WHEN** a DAL Transaction reads three keys served by plugin `P` across two batched reads
- **THEN** `P.Bind` SHALL be called once and `Release` SHALL be called once at transaction end

### Requirement: Batched routed reads
A DAL Transaction read SHALL accept a batch of keys, group them by owning plugin, and issue exactly one `Read` call per plugin per batch on that plugin's affinity executor. The result SHALL contain every requested key, with absent values reported as absent. Each key SHALL resolve in order: the transaction's staged write, then its overlay value, then the plugin.

#### Scenario: One call per plugin
- **WHEN** a batch contains two `Resource` keys and one `Transient` key
- **THEN** the resource plugin binding SHALL receive one `Read` with two keys and the transient plugin binding SHALL receive one `Read` with one key

#### Scenario: Read your own staged write
- **WHEN** a DAL Transaction stages a write for key `K` and then reads `K`
- **THEN** the staged value SHALL be returned without calling the plugin for `K`

#### Scenario: Overlay value returned
- **WHEN** an overlay value is set for key `K` and `K` is read in the same DAL Transaction
- **THEN** the overlay value SHALL be returned

### Requirement: Staged writes
A DAL Transaction SHALL stage writes without applying them. Staging a write for a key served by a read-only plugin SHALL fail with `NotWritable`. Multiple writes to the same key SHALL keep only the last. A staged write MAY carry a TTL, which SHALL be passed to the plugin at commit.

#### Scenario: Write to read-only plugin rejected
- **WHEN** a DAL Transaction stages a write for an `Attribute` key
- **THEN** staging SHALL fail with `NotWritable`

#### Scenario: Last write wins
- **WHEN** a DAL Transaction stages `K = "a"` then `K = "b"` and commits
- **THEN** the plugin SHALL receive only `K = "b"`

### Requirement: Commit with prior-value rollback
On commit, the DAL SHALL read the current value of every staged key through the owning plugin, apply staged writes plugin by plugin in deterministic order (namespace, then device ID, then plugin name), and, if a plugin's commit fails, SHALL write the captured prior values back to every already-committed plugin in reverse order (deleting keys that were previously absent). A failed commit SHALL end the transaction with failure type `commitFailed`. A rollback failure SHALL be logged. All commits SHALL run on the device-service main loop. The DAL SHALL NOT implement two-phase commit.

#### Scenario: Successful commit
- **WHEN** a DAL Transaction with writes for two writable plugins commits and both plugin commits succeed
- **THEN** both plugins SHALL hold the new values and the transaction SHALL be committed

#### Scenario: Later plugin fails, earlier plugin rolled back
- **WHEN** the first plugin in commit order commits successfully and the second plugin's commit fails
- **THEN** the DAL SHALL write the first plugin's prior values back and the commit SHALL fail with `commitFailed`

#### Scenario: Previously absent key rolled back to absent
- **WHEN** a committed key had no prior value and rollback runs
- **THEN** the DAL SHALL delete that key in the plugin

### Requirement: Abort discards staged state
Aborting a DAL Transaction SHALL discard its staged writes and overlay values without calling any plugin `Commit`, SHALL release all bindings, and SHALL be idempotent. Any operation on an ended transaction SHALL fail with the recorded end reason.

#### Scenario: Abort discards writes
- **WHEN** a DAL Transaction stages writes and is aborted
- **THEN** no plugin `Commit` SHALL be called

### Requirement: Main-loop confinement and executor affinity
All DAL state SHALL be accessed only on the device-service GLib main loop. DAL entry points called from another thread SHALL be marshaled to the main loop and SHALL wait with a caller-supplied deadline. When the caller owns the main-loop context (`g_main_context_is_owner`), DAL entry points SHALL run inline. Plugin binding calls SHALL run on the plugin's affinity executor: inline for `MainLoop` plugins, and via a main-loop to Matter-thread hop with a deadline for `MatterThread` plugins.

#### Scenario: Inline on main loop
- **WHEN** a DAL entry point is called on the main-loop thread
- **THEN** it SHALL execute inline without posting to the main loop

#### Scenario: Matter-thread plugin hop
- **WHEN** a read resolves to a plugin with `MatterThread` affinity
- **THEN** the binding's `Read` SHALL run on the Matter thread and the main loop SHALL wait for it with a deadline

#### Scenario: Hop deadline exceeded
- **WHEN** an executor hop does not complete before its deadline
- **THEN** the DAL operation SHALL fail with `Timeout` and late completion SHALL NOT access freed state

### Requirement: The Matter thread never waits on the main loop
DAL entry points that would block SHALL NOT wait when called on the Matter thread; they SHALL fail immediately with failure type `Internal` and log the violation. `Unregister` called on the Matter thread SHALL be posted to the main loop without waiting.

#### Scenario: DAL read from Matter thread fails fast
- **WHEN** a DAL read is invoked on the Matter thread
- **THEN** it SHALL return `Internal` immediately without blocking

#### Scenario: Unregister from Matter thread does not block
- **WHEN** `Unregister` is called on the Matter thread
- **THEN** it SHALL return without waiting and the unregistration SHALL complete on the main loop

### Requirement: Atomic plugin capability
A plugin SHALL declare the `Atomic` capability to support sequence allocation and conditional writes. Sequence allocation or a conditional write routed to a plugin without the `Atomic` capability SHALL fail with `NotAtomic`. An `Atomic` plugin SHALL check every conditional write in a commit batch before applying any write in that batch.

#### Scenario: Non-atomic plugin rejects conditional write
- **WHEN** `StageConditional` is called for a key served by a plugin without the `Atomic` capability
- **THEN** staging SHALL fail with `NotAtomic`

#### Scenario: Non-atomic plugin rejects sequence allocation
- **WHEN** `AllocateSequence` is called for a key served by a plugin without the `Atomic` capability
- **THEN** it SHALL fail with `NotAtomic`

### Requirement: Atomic sequence allocation
`DalTransaction::AllocateSequence(key)` SHALL atomically increment the integer counter stored at `key` in its `Atomic` plugin (an absent counter counts as 0) and SHALL return the new value. The allocation SHALL be applied immediately, without staging, and SHALL NOT be rolled back when the DAL Transaction aborts or its commit fails. Concurrent allocations on the same key from different DAL Transactions SHALL return distinct values. A batch of sequence keys SHALL be allocated in one hop alongside the step's batched supplement read.

#### Scenario: Concurrent allocations are distinct
- **WHEN** two open DAL Transactions each allocate from `(D, "TD_NEXT_SESSION_ID")` in the `Transient` namespace
- **THEN** they SHALL receive different values

#### Scenario: First allocation from absent counter
- **WHEN** a sequence key has no value and is allocated
- **THEN** the returned value SHALL be 1 and the stored counter SHALL be 1

#### Scenario: Allocation survives abort
- **WHEN** a DAL Transaction allocates value `n` from key `K` and is then aborted
- **THEN** the counter at `K` SHALL remain `n` and the next allocation SHALL return `n + 1`

### Requirement: Conditional writes
`DalTransaction::StageConditional(key, expected, value)` SHALL stage a write that applies only if the key's current value at commit equals `expected` (or, when `expected` is absent, the key is absent). The check and apply SHALL be atomic within the owning plugin. A mismatch SHALL fail the commit with failure type `commitFailed` and reason `conditionFailed`, SHALL apply no write in that plugin's batch, and SHALL trigger rollback of already-committed plugins per the commit rollback requirement. Later reads of the key in the same DAL Transaction SHALL return the staged value.

#### Scenario: Condition holds
- **WHEN** `K` is `"3"` at commit and the transaction staged `StageConditional(K, "3", "4")`
- **THEN** the commit SHALL succeed and `K` SHALL be `"4"`

#### Scenario: Condition fails after interleaving
- **WHEN** transaction A stages `StageConditional(K, "3", "4")`, transaction B commits `K = "5"`, and then A commits
- **THEN** A's commit SHALL fail with `commitFailed` reason `conditionFailed` and `K` SHALL remain `"5"`

#### Scenario: Condition failure rolls back earlier plugins
- **WHEN** a commit applies writes to plugin P1 and then a conditional write in plugin P2 fails its condition
- **THEN** P1's prior values SHALL be restored and the commit SHALL fail with reason `conditionFailed`

#### Scenario: Expected absence
- **WHEN** `StageConditional(K, absent, "1")` is committed and `K` already exists
- **THEN** the commit SHALL fail with reason `conditionFailed`

### Requirement: DAL implementation of the SBMD data-access seam
The system SHALL provide `DalSbmdDataAccess`, an implementation of the `SbmdDataAccess` seam introduced by the SBMD Transaction orchestrator, backed by the DAL. It SHALL open one DAL Transaction per SBMD Transaction; serve each step's supplements (attributes, resources, persistent data, transient data, and declared sequences) with one batched DAL read and one batched sequence allocation; map handler data ops (`updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, and conditional writes) to staged DAL writes; record attribute-write overlays; and commit or abort the DAL Transaction. DAL commit failures SHALL be reported to the orchestrator as failure type `commitFailed` with the reason (`pluginError`, `conditionFailed`, or `rollbackFailed`), and aborts as `aborted`.

#### Scenario: Persistent data op maps to metadata write
- **WHEN** a step returns `setPersistentData("pin", "1234")`
- **THEN** `DalSbmdDataAccess` SHALL stage a `Metadata` write for path `"sbmd.pin"` on the device

#### Scenario: One read hop per step
- **WHEN** a step declares attribute, resource, and transient supplements
- **THEN** `DalSbmdDataAccess` SHALL issue a single batched DAL read for all of them

#### Scenario: Conditional failure reported
- **WHEN** a DAL commit fails with reason `conditionFailed`
- **THEN** `DalSbmdDataAccess` SHALL report failure type `commitFailed` with reason `conditionFailed`
