## ADDED Requirements

### Requirement: Single data access layer for SBMD driver data
The SBMD runtime SHALL route every SBMD driver data read and write — Matter attribute cache reads, Barton resource reads/writes, SBMD persistent data reads/writes, and SBMD transient data reads/writes — through a single C++ Data Access Layer (`barton::sbmd::dal::DataAccessLayer`). Supplement fetchers and non-terminal data ops SHALL NOT access deviceService, `MatterDevice` caches, or transient storage directly.

#### Scenario: Supplement read goes through the DAL
- **WHEN** a handler declares `supplements: { transientData: ["sessions"] }` and is invoked
- **THEN** the `sessions` value SHALL be obtained via `DataAccessLayer::Read` for `DataKey::Transient(device, "sessions")`

#### Scenario: setTransientData goes through the DAL
- **WHEN** a handler result contains `storage.setTransientData("sessions", value, 3600)`
- **THEN** the value SHALL be written via `DataAccessLayer::Write` and SHALL be readable by the next handler for the same device

#### Scenario: Driver-visible behavior unchanged
- **WHEN** an existing SBMD driver runs against the DAL-backed runtime
- **THEN** supplement values, storage semantics (including `ttlSecs == 0` deleting the key), and resource updates SHALL be identical to the pre-DAL runtime

---

### Requirement: Per-key monotonic versions
The DAL SHALL maintain a version counter per key. The version SHALL increase by exactly one on every logical change to the key (write, delete, reconciliation, TTL expiry, invalidation). Writing a value byte-equal to the current value SHALL NOT change the version. Versions SHALL be independent per key so that a change to one key never alters another key's version. Versions SHALL NOT be exposed to driver JavaScript.

#### Scenario: Write bumps version
- **WHEN** a key at version N is written with a different value
- **THEN** its version SHALL become N+1

#### Scenario: Same-value write does not bump version
- **WHEN** a key is written with a value equal to its current value
- **THEN** its version SHALL remain unchanged

#### Scenario: Delete and recreate is detectable
- **WHEN** a key at version N is deleted and then written again
- **THEN** its version SHALL be N+2

#### Scenario: TTL expiry bumps version
- **WHEN** a transient key's TTL elapses and the key is next read
- **THEN** the read SHALL return absent and the key's version SHALL have increased

---

### Requirement: Efficient hash-indexed storage with shared values
The DAL SHALL index keys by hash for O(1) average lookup, with keys of the forms `(device, endpoint, cluster, attribute)`, `(device, endpoint, resourceId)`, `(device, persistentKey)`, and `(device, transientKey)`. Values SHALL be immutable and shared by reference so that reads do not copy value payloads, and a reader holding a value obtained before a write SHALL continue to observe the old value.

#### Scenario: Namespaces do not collide
- **WHEN** a transient key and a persistent key share the same name for the same device
- **THEN** they SHALL be stored and versioned independently

#### Scenario: Snapshot isolation of a held value
- **WHEN** a value handle is read and the key is subsequently overwritten
- **THEN** the held handle SHALL still contain the original value

---

### Requirement: Matter-thread confinement and cross-thread ingress
All DAL store access SHALL occur on the Matter event-loop thread without locking. Changes originating on other threads (GLib main loop, public API callers, deviceService worker threads) SHALL be submitted through a mutex-guarded ingress queue and applied on the Matter thread in submission order. Changes submitted on the Matter thread SHALL be applied synchronously after any previously queued cross-thread changes. No DAL mutex SHALL be held while invoking JavaScript or while an operation is parked.

#### Scenario: Cross-thread resource write is reconciled
- **WHEN** `updateResource` changes a resource value on a non-Matter thread
- **THEN** the change SHALL be applied to the DAL on the Matter thread and the key's version SHALL increase

#### Scenario: Ordering preserved
- **WHEN** two changes to the same key are submitted from other threads in order A then B
- **THEN** the DAL SHALL apply A before B and the final value SHALL be B

---

### Requirement: Live-update reconciliation
The DAL SHALL keep its view current by reconciling: Matter attribute reports (before driver attribute handlers run), resource changes made via `updateResource` (including those caused by public API writes through `deviceServiceWriteResource`), `sbmd.`-prefixed metadata changes made via `deviceServiceSetMetadata`, SBMD driver writes, and device removal. Writes the DAL itself performs to a backing store SHALL NOT be re-applied as foreign changes.

#### Scenario: Attribute report updates the view
- **WHEN** an attribute report for `(endpoint 1, cluster 0x0006, attribute 0x0000)` arrives
- **THEN** a subsequent DAL read of that key SHALL return the reported value with an increased version

#### Scenario: Concurrent change during a park is visible
- **WHEN** operation A is parked on a `requestCommand` and operation B writes transient key `sessions`
- **THEN** a DAL read of `sessions` performed after B and before A resumes SHALL return B's value

#### Scenario: Echo suppression
- **WHEN** the DAL writes a resource through `updateResource`
- **THEN** the resulting change notification SHALL NOT cause a second version bump

#### Scenario: Device removal clears keys
- **WHEN** a device is removed
- **THEN** all DAL keys for that device SHALL be discarded
