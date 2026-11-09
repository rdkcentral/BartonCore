## ADDED Requirements

### Requirement: Device resources plugin
The system SHALL register a `Resource`-namespace DAL plugin with `MainLoop` affinity that is readable and writable. A path SHALL be `"<endpointId>/<resourceId>"` for endpoint resources or `"<resourceId>"` for device-level resources. Reads SHALL return the current resource value via `deviceServiceGetResourceById` and SHALL release the returned resource. Commits SHALL apply values via `updateResource`. Deletes SHALL be rejected.

#### Scenario: Endpoint resource read
- **WHEN** a DAL Transaction reads `Resource` key `(D, "1/isOn")`
- **THEN** the plugin SHALL return the value of resource `isOn` on endpoint `1` of device `D`

#### Scenario: Device-level resource write
- **WHEN** a DAL Transaction commits `Resource` key `(D, "firmwareVersion") = "2"`
- **THEN** the plugin SHALL call `updateResource` with a NULL endpoint for device `D`

### Requirement: Device metadata plugin
The system SHALL register a `Metadata`-namespace DAL plugin with `MainLoop` affinity that is readable and writable, reading and writing `/devices/<deviceId>/metadata/<path>` via `deviceServiceGetMetadata` and `deviceServiceSetMetadata`. SBMD persistent data key `k` SHALL map to path `"sbmd.k"`.

#### Scenario: Persistent data read
- **WHEN** a DAL Transaction reads `Metadata` key `(D, "sbmd.pin")`
- **THEN** the plugin SHALL return the value of metadata `sbmd.pin` on device `D`, or absent if unset

#### Scenario: Metadata write failure
- **WHEN** `deviceServiceSetMetadata` returns false during commit
- **THEN** the binding commit SHALL fail and the DAL commit SHALL fail with `commitFailed`

### Requirement: Device-service properties plugin
The system SHALL register a `Property`-namespace DAL plugin with `MainLoop` affinity that is readable and writable, reading and writing system properties via `deviceServiceGetSystemProperty` and `deviceServiceSetSystemProperty`. Keys SHALL have an empty device ID.

#### Scenario: Property read
- **WHEN** a DAL Transaction reads `Property` key `("", "name")`
- **THEN** the plugin SHALL return the system property value, or absent if unset

### Requirement: Transient-data plugin owns transient data with TTL
The system SHALL register a DAL-native `Transient`-namespace plugin with `MainLoop` affinity that is readable and writable and is the sole owner of SBMD transient data. Each write SHALL carry a TTL and SHALL set expiry to the write time plus the TTL on a monotonic clock. A read at or after expiry SHALL remove the entry and report it absent. A later write to the same key SHALL replace the value and expiry. Transient data SHALL NOT be persisted. `SpecBasedMatterDeviceDriver::transientStore` SHALL be removed.

#### Scenario: Value readable before expiry
- **WHEN** transient key `(D, "sessions")` is committed with TTL 60s and read 10s later
- **THEN** the committed value SHALL be returned

#### Scenario: Value absent after expiry
- **WHEN** transient key `(D, "sessions")` is committed with TTL 60s and read 60s or more later
- **THEN** the read SHALL report absent and the entry SHALL be removed

#### Scenario: Write without TTL rejected
- **WHEN** a transient write without a TTL is committed
- **THEN** the binding commit SHALL fail

#### Scenario: Behavior unchanged for existing drivers
- **WHEN** an existing SBMD driver sets and later reads transient data through today's handler ops and supplements
- **THEN** it SHALL observe the same values and expiry behavior as before this change

### Requirement: Transient plugin is main-loop confined
The transient-data plugin and its storage SHALL be accessed only on the device-service main loop and SHALL NOT use a mutex. No code running on the Matter thread SHALL read or write transient data.

#### Scenario: Purge from device removal is posted
- **WHEN** device removal runs on the Matter thread
- **THEN** the transient purge for that device SHALL be posted to the main loop without the Matter thread waiting

### Requirement: Transient plugin supports atomic operations
The transient-data plugin SHALL declare the `Atomic` capability. Sequence allocation SHALL treat an absent or expired counter as 0, store the incremented value as a decimal string, return it immediately, and SHALL NOT be undone by abort or commit failure; a non-numeric existing value SHALL fail the allocation. On commit, the plugin SHALL check every conditional write in the batch against current, non-expired values before applying any write; on any mismatch it SHALL apply no write in the batch and SHALL fail with `commitFailed` reason `conditionFailed`. When the DAL rolls back a transient key, the prior value SHALL be restored with its remaining TTL.

#### Scenario: Sequence from absent counter
- **WHEN** `(D, "TD_NEXT_SESSION_ID")` is absent and a sequence is allocated from it
- **THEN** the allocation SHALL return 1 and the stored value SHALL be `"1"`

#### Scenario: Interleaved createSession calls get distinct IDs
- **WHEN** two concurrent SBMD Transactions for device `D` each allocate from `(D, "TD_NEXT_SESSION_ID")`
- **THEN** they SHALL receive distinct values

#### Scenario: Conditional write mismatch applies nothing
- **WHEN** a commit batch contains a conditional write whose expected value does not match and an unconditional write to another key
- **THEN** neither write SHALL be applied and the commit SHALL fail with reason `conditionFailed`

#### Scenario: Rollback does not extend TTL
- **WHEN** a transient key with 20s remaining is overwritten during a commit that is later rolled back
- **THEN** the restored entry SHALL expire about 20s after the original read, not after a fresh full TTL

### Requirement: Orchestrator switched to the DAL
After the plugins are registered, the SBMD orchestrator SHALL use `DalSbmdDataAccess` instead of `LegacySbmdDataAccess`. `SpecBasedMatterDeviceDriver::transientStore`, its accessors (`SetTransientData`, `GetTransientData`, `MakeTransientFetcher`, `MakeTransientSetter`), and `LegacySbmdDataAccess` in its entirety SHALL be removed; no legacy fallback SHALL remain. Driver-visible supplement values and op effects SHALL be unchanged.

#### Scenario: Existing drivers unchanged
- **WHEN** the existing SBMD drivers run their unit and integration tests with the orchestrator using `DalSbmdDataAccess`
- **THEN** all tests SHALL pass with unchanged results

### Requirement: Transient data purged on device removal
When a Matter device is removed, the transient-data plugin SHALL discard all transient data for that device.

#### Scenario: Removed device has no transient data
- **WHEN** device `D` with transient data is removed and later re-added
- **THEN** reads of `D`'s transient keys SHALL report absent

### Requirement: Per-device Matter attribute plugin
For each `MatterDevice`, the Matter driver SHALL register a read-only `Attribute`-namespace DAL plugin scoped to that device ID with `MatterThread` affinity. A path SHALL be `"<endpointId>/0x<clusterId>/0x<attributeId>"`. Reads SHALL run on the Matter thread, SHALL read the device data cache (`MatterDevice::GetCachedAttributeData`), and SHALL return the attribute value as base64-encoded TLV, the same encoding as today's attribute supplements. A cache miss or a removed device SHALL be reported absent. Commits SHALL be rejected with `NotWritable`.

#### Scenario: Cached attribute read
- **WHEN** a DAL Transaction reads `Attribute` key `(D, "1/0x0006/0x0000")` and the OnOff attribute is cached
- **THEN** the plugin SHALL return the base64 TLV of the cached value, read on the Matter thread

#### Scenario: Cache miss
- **WHEN** the requested attribute is not in the device data cache
- **THEN** the read SHALL report absent

#### Scenario: Attribute write via DAL rejected
- **WHEN** a DAL Transaction stages a write to an `Attribute` key
- **THEN** staging SHALL fail with `NotWritable`

### Requirement: Attribute plugin lifetime owned by the Matter driver
The Matter driver SHALL register a device's attribute plugin after the `MatterDevice` is added in `MatterDeviceDriver::AddDevice`, and SHALL unregister it in `MatterDeviceDriver::DeviceRemoved` and `MatterDeviceDriver::Shutdown` before the `MatterDevice` is destroyed. Unregistration from the Matter thread SHALL NOT wait on the main loop. The DAL SHALL NOT infer plugin lifetimes.

#### Scenario: Plugin registered on add
- **WHEN** a `MatterDevice` for `D` is added
- **THEN** an `Attribute` plugin scoped to `D` SHALL be registered

#### Scenario: Device removal aborts bound transactions
- **WHEN** device `D` is removed while a DAL Transaction is bound to `D`'s attribute plugin
- **THEN** the plugin SHALL be unregistered and that transaction SHALL be aborted with failure type `aborted`

### Requirement: Read-your-own-writes overlay for attribute writes
Attribute writes SHALL be device operations tracked by the SBMD Transaction's operation log, not DAL writes. After a `writeAttribute` operation succeeds, the orchestrator SHALL record the written value as an overlay on the DAL Transaction for that device's attribute key; subsequent reads of that key in the same DAL Transaction SHALL return the written value. The overlay SHALL be discarded when the DAL Transaction ends and SHALL NOT modify the device data cache.

#### Scenario: Later step sees written value
- **WHEN** step 1 writes attribute `(D, "1/0x0101/0x0023")` successfully and step 2 in the same SBMD Transaction reads it before a subscription report arrives
- **THEN** step 2 SHALL receive the written value

#### Scenario: Overlay not visible to other transactions
- **WHEN** another DAL Transaction reads the same attribute key
- **THEN** it SHALL receive the device data cache value

### Requirement: Matter-thread reads use the shared ThreadHop
`MatterThread`-affinity plugin reads SHALL be executed through the DAL's `DalExecutor` over the same `ThreadHop` instance used by the SBMD orchestrator. The Matter driver SHALL NOT install separate DAL executor hooks.

#### Scenario: Matter-affinity read executes on the Matter thread
- **WHEN** the DAL routes a read to a `MatterThread`-affinity plugin
- **THEN** the binding's `Read` SHALL execute on the Matter thread via `ThreadHop::OnMatter`
