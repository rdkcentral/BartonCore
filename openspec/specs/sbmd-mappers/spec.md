# SBMD Mappers

## Purpose

Specifies SBMD resource-mapper features: prerequisite gating of resources via cluster/attribute aliases, and the `seedFrom` mapper that seeds an event-backed resource from a Matter attribute at configure and synchronize time.

## Requirements

### Requirement: Alias definitions in `matterMeta`
Each SBMD driver spec MAY declare a `matterMeta.aliases` list. Each alias SHALL have a `name` (unique within the spec) and exactly one of `attribute` (with `clusterId`, `attributeId`, `name`, `type`) or `event` (with `clusterId`, `eventId`, `name`). An alias name referenced in `prerequisites` or mapper metadata that does not exist in `matterMeta.aliases` SHALL be a parse-time error.

#### Scenario: Alias with attribute metadata
- **WHEN** `matterMeta.aliases` contains `{name: stateValue, attribute: {clusterId: "0x0045", attributeId: "0x0000", name: "StateValue", type: "bool"}}`
- **THEN** the parser SHALL produce an `SbmdAlias` with `name = "stateValue"`, non-empty `attribute`, and `nullopt` `event`

#### Scenario: Alias with event metadata
- **WHEN** `matterMeta.aliases` contains `{name: lockOperation, event: {clusterId: "0x0101", eventId: "0x0002", name: "LockOperation"}}`
- **THEN** the parser SHALL produce an `SbmdAlias` with `name = "lockOperation"`, `nullopt` `attribute`, and non-empty `event`

#### Scenario: Alias referenced but not defined
- **WHEN** a `prerequisites` entry or mapper field references an alias name that does not appear in `matterMeta.aliases`
- **THEN** the parser SHALL reject the spec with an error

### Requirement: Prerequisite declaration on SBMD resources
Each SBMD resource SHALL declare a `prerequisites` field. It SHALL be either an explicit opt-out (`none` or `null`, both meaning always register) or a non-empty list of prerequisite entries. Each entry SHALL take exactly the form `{alias: <name>}` referencing a name defined in `matterMeta.aliases`. Absence of the field on any resource is a parse-time error regardless of which mappers the resource implements. The preferred opt-out form is `none` for readability, but `null` is accepted for YAML authors who prefer explicit null syntax.

#### Scenario: Resource with attribute alias prerequisite
- **WHEN** a resource declares `prerequisites: [{alias: stateValue}]` and the spec defines an attribute alias named `stateValue` with `clusterId: 0x0045`, `attributeId: 0x0000`
- **THEN** the parser SHALL produce an `SbmdPrerequisite` with `clusterId = 0x0045` and `attributeIds = [0x0000]`

#### Scenario: Resource with event alias prerequisite
- **WHEN** a resource declares `prerequisites: [{alias: lockOperation}]` and the spec defines an event alias named `lockOperation` with `clusterId: 0x0101`
- **THEN** the parser SHALL produce an `SbmdPrerequisite` with `clusterId = 0x0101` and empty `attributeIds` (cluster-only check)

> **Known limitation**: Event alias prerequisites perform a cluster-presence check only.
> The Matter `EventList` attribute (0xFFFA), which would allow verifying that a
> specific event ID is supported before any event fires, is marked provisional in the
> current CHIP SDK version and is not reliably available on real devices. When
> `EventList` becomes stable and widely supported, event prerequisites SHOULD be
> upgraded to also verify the specific event ID.

#### Scenario: Resource with prerequisites: none
- **WHEN** a resource declares `prerequisites: none`
- **THEN** the parser SHALL produce an empty `std::vector<SbmdPrerequisite>` (always-register)

### Requirement: Alias-based mapper metadata
Read mappers SHALL reference an alias in place of an inline `attribute:` block. Event mappers SHALL reference an alias in place of an inline `event:` block. The alias is resolved at parse time, populating the same `SbmdMapper.readAttribute` / `SbmdMapper.event` fields used by the rest of the runtime.

#### Scenario: Read mapper with attribute alias
- **WHEN** `mapper.read` declares `alias: stateValue` and `stateValue` is an attribute alias
- **THEN** the parser SHALL populate `SbmdMapper.readAttribute` from the alias's `attribute` metadata

#### Scenario: Event mapper with event alias
- **WHEN** `mapper.event` declares `alias: lockOperation` and `lockOperation` is an event alias
- **THEN** the parser SHALL populate `SbmdMapper.event` from the alias's `event` metadata

### Requirement: Parser enforcement of prerequisites on all resources
The `SbmdParser` SHALL require the `prerequisites` field to be present on every resource, regardless of which mappers it implements. Absence of the `prerequisites` field on any resource SHALL be a parse-time error.

#### Scenario: Read mapper resource missing prerequisites
- **WHEN** a resource has a `mapper.read` section and no `prerequisites` field
- **THEN** `SbmdParser::ParseFile()` SHALL return `nullptr`

#### Scenario: Event mapper resource missing prerequisites
- **WHEN** a resource has a `mapper.event` section and no `prerequisites` field
- **THEN** `SbmdParser::ParseFile()` SHALL return `nullptr`

#### Scenario: Write-only resource missing prerequisites
- **WHEN** a resource has only a `mapper.write` section and no `prerequisites` field
- **THEN** `SbmdParser::ParseFile()` SHALL return `nullptr`

#### Scenario: Execute-only resource missing prerequisites
- **WHEN** a resource has only a `mapper.execute` section and no `prerequisites` field
- **THEN** `SbmdParser::ParseFile()` SHALL return `nullptr`

### Requirement: Prerequisite evaluation at commissioning time
Before binding and registering any SBMD resource, `SpecBasedMatterDeviceDriver` SHALL evaluate the resource's `prerequisites` against the device's `DeviceDataCache`. All prerequisites in the list must be satisfied for the resource to be registered. If any prerequisite is unmet, the outcome depends on the resource's `optional` flag:
- If the resource is `optional: true`, it SHALL be silently skipped (not registered, commissioning continues).
- If the resource is required (no `optional: true`), `AddDevice()` SHALL return `false` (commissioning aborted).

This mirrors the existing failure semantics of `configureResource`: the `optional` flag governs failure tolerance whether the failure is a mapper bind error or an unmet prerequisite.

#### Scenario: Optional resource silently skipped when prerequisite cluster absent
- **WHEN** an optional resource has a prerequisite and the device's data cache contains no endpoint with the prerequisite's cluster
- **THEN** the resource SHALL not be registered and `AddDevice()` SHALL return true (success)

#### Scenario: Required resource fails commissioning when prerequisite cluster absent
- **WHEN** a required (non-optional) resource has a prerequisite and the device's data cache contains no endpoint with the prerequisite's cluster
- **THEN** `AddDevice()` SHALL return false (commissioning aborted)

#### Scenario: Resource registered when prerequisite cluster present
- **WHEN** a resource has a prerequisite and the device's data cache contains at least one endpoint with the prerequisite's cluster
- **THEN** the resource SHALL be registered normally

#### Scenario: Optional resource silently skipped when prerequisite attribute absent
- **WHEN** an optional resource has a prerequisite (from an attribute alias) with cluster + attribute, the cluster is present, but the attribute is not in the cache
- **THEN** the resource SHALL not be registered and `AddDevice()` SHALL return true (success)

#### Scenario: Required resource fails commissioning when prerequisite attribute absent
- **WHEN** a required (non-optional) resource has a prerequisite (from an attribute alias) with cluster + attribute, the cluster is present, but the attribute is not in the cache
- **THEN** `AddDevice()` SHALL return false (commissioning aborted)

#### Scenario: Resource registered when prerequisite cluster and attribute both present
- **WHEN** a resource has an attribute-alias prerequisite and both the cluster and attribute are present in the cache
- **THEN** the resource SHALL be registered normally

#### Scenario: Alias prerequisite resolved at parse time
- **WHEN** a resource with `prerequisites: [{alias: measuredValue}]` has an alias `measuredValue` with `clusterId: 0x0405`, `attributeId: 0x0000`, and that cluster+attribute is present in the cache
- **THEN** the prerequisite SHALL be satisfied and the resource SHALL be registered

#### Scenario: All prerequisites must be satisfied
- **WHEN** a resource declares two prerequisites and only one is satisfied
- **THEN** the resource SHALL not be registered

#### Scenario: Empty prerequisites list (none) always registers
- **WHEN** a resource declares `prerequisites: none` (parsed as empty vector)
- **THEN** the resource SHALL always be registered regardless of device data cache contents

### Requirement: Skipped prerequisite resources not registered
Resources skipped due to unmet prerequisites SHALL be excluded from both the mapper binding phase (`AddDevice()`) and the resource registration phase (`DoRegisterResources()`). This SHALL use the same skipped-resource tracking mechanism used for skipped optional resources.

#### Scenario: Prerequisite-skipped resource excluded from device model
- **WHEN** a resource is skipped due to unmet prerequisites
- **THEN** no Barton resource entry SHALL be created for it in the device model

#### Scenario: Prerequisite-skipped resource excluded from mapper bindings
- **WHEN** a resource is skipped due to unmet prerequisites
- **THEN** no mapper binding SHALL be established for it in the script engine

### Requirement: seedFrom mapper — schema and structure
A resource's mapper MAY contain a `seedFrom` section with an `alias` (a string naming an **attribute** alias defined in `matterMeta.aliases`) and a `script` (JavaScript string). The `seedFrom` mapper SHALL only appear when the same resource also declares a `mapper.event` section. The `alias` SHALL resolve to an attribute alias; event aliases SHALL NOT be accepted. The `script` SHALL be required; absence of `script` SHALL be a parse error.

#### Scenario: Valid seedFrom mapper
- **WHEN** a mapper declares `seedFrom.alias: lockState` and `lockState` is an attribute alias, and `seedFrom.script` is present, and `mapper.event` is also present
- **THEN** the parser SHALL populate `seedFromAttribute` with the resolved cluster/attribute/type and `seedFromScript` with the script text

#### Scenario: seedFrom with event alias rejected
- **WHEN** a mapper declares `seedFrom.alias: lockOperation` and `lockOperation` is an event alias (not an attribute alias)
- **THEN** the parser SHALL reject the spec with an error

#### Scenario: seedFrom without script rejected
- **WHEN** a mapper declares `seedFrom.alias: lockState` but omits `seedFrom.script`
- **THEN** the parser SHALL reject the spec with an error

### Requirement: seedFrom mutual exclusion with read
A resource mapper SHALL NOT declare both `read` and `seedFrom`. The presence of both SHALL be a parse-time error.

#### Scenario: seedFrom + read on same resource rejected
- **WHEN** a mapper declares both `read` and `seedFrom`
- **THEN** the parser SHALL reject the spec with an error

### Requirement: seedFrom requires event
A `seedFrom` mapper SHALL NOT appear on a resource without a `mapper.event` section. Absence of `mapper.event` when `mapper.seedFrom` is present SHALL be a parse-time error.

#### Scenario: seedFrom without event rejected
- **WHEN** a mapper declares `seedFrom` but does not declare `event`
- **THEN** the parser SHALL reject the spec with an error

### Requirement: seedFrom attribute not subscribed for live updates
Attributes referenced by `mapper.seedFrom` SHALL NOT be registered in the device's live attribute subscription callback table. `seedFrom` attribute reads are one-shot operations that occur only at configure and synchronize time; they SHALL NOT update the resource in response to live attribute subscription callbacks.

#### Scenario: seedFrom attribute does not update resource on subscription
- **WHEN** a resource declares only `event` and `seedFrom` (no `read`)
- **THEN** the driver SHALL NOT register the seedFrom attribute in `readableAttributeLookup`
- **THEN** a live attribute subscription update for the seedFrom attribute SHALL NOT update the resource value

### Requirement: seedFrom seeds resource at configure time
When a device is configured (during commissioning), the driver SHALL compute each resource's `seedFrom` value from the device data cache during resource registration (`DoRegisterResources`) and pass it as the initial value to `createDeviceResource()`/`createEndpointResource()`. This SHALL occur on the Matter thread (which is where `DoRegisterResources` executes) and before the device is persisted or `DEVICE_ADDED` is emitted, so the resource SHALL have the correct initial value when `DEVICE_ADDED` fires. Resources in `skippedOptionalResources` SHALL be skipped. If the attribute is absent from the cache, the resource SHALL be created with a null initial value.

#### Scenario: Locked resource seeded at commissioning
- **WHEN** a door lock device is commissioned and `LockState` (cluster `0x0101`, attribute `0x0000`) is present in the attribute cache
- **THEN** the `locked` resource SHALL have a non-null value when `DEVICE_ADDED` fires — no intermediate null state SHALL be visible to clients

#### Scenario: Optional resource not seeded when skipped
- **WHEN** a resource is marked `optional: true` and was skipped due to absent cluster
- **THEN** `DoRegisterResources()` SHALL NOT attempt to read its seedFrom attribute

#### Scenario: Cache miss — resource created with null value
- **WHEN** the seedFrom attribute is absent from the device data cache at configure time
- **THEN** `createDeviceResource()` SHALL be called with a null initial value for that resource

### Requirement: seedFrom seeds resource at synchronize time
When a device is synchronized (at Barton restart or communication restoration), the driver SHALL re-read each resource's `seedFrom` attribute from the device data cache and update the resource value via `updateResource()`. This SHALL use the same `SeedInitialResourceValues()` helper as configure time and apply the same guards (skipped resources, cache miss).

#### Scenario: Locked resource re-seeded at device synchronization
- **WHEN** `SynchronizeDevice()` is called for a commissioned door lock (e.g., after a comm-fail restore or Barton restart)
- **THEN** the `locked` resource SHALL be re-seeded from the `LockState` attribute cache before any event fires

### Requirement: seedFrom script interface
The JavaScript script in `mapper.seedFrom` SHALL receive attribute value data via the same `sbmdReadArgs` interface used by `mapper.read` scripts: `sbmdReadArgs.tlvBase64` (base64-encoded TLV of the attribute value), `sbmdReadArgs.clusterId`, `sbmdReadArgs.attributeId`, `sbmdReadArgs.attributeName`, `sbmdReadArgs.attributeType`, `sbmdReadArgs.endpointId`, `sbmdReadArgs.deviceUuid`, and `sbmdReadArgs.clusterFeatureMaps`. The script SHALL return a JSON object of the form `{ output: <string> }`, identical to the return contract of `mapper.read` scripts.

#### Scenario: seedFrom script decodes uint8 to boolean
- **WHEN** a `seedFrom` script receives a `uint8` TLV for the `LockState` attribute (value `1` = locked)
- **THEN** the script SHALL decode the value and return `{ output: "true" }` for the `locked` resource

#### Scenario: seedFrom script receives same args as read script
- **WHEN** a `seedFrom` script is invoked
- **THEN** `sbmdReadArgs` SHALL contain the same fields as a `read` mapper script invocation for the same attribute

### Requirement: Event script partial handling of LockOperation types
The `event` script paired with `seedFrom` MUST handle all values of the event's discriminating field. For the `LockOperation` event on the DoorLock cluster, the script SHALL return a resource value only for `LockOperationType` values that represent a lock state transition (Lock = 0, Unlock = 1). For all other `LockOperationType` values (NonAccessUserEvent = 2, ForcedUserEvent = 3, Unlatch = 4), the script SHALL return no `output` key, leaving the resource value unchanged.

#### Scenario: LockOperation event with Lock type updates resource
- **WHEN** a `LockOperation` event arrives with `LockOperationType` = 0 (Lock)
- **THEN** the event script SHALL return `{ output: "true" }` and the `locked` resource SHALL be updated

#### Scenario: LockOperation event with non-state-change type does not update resource
- **WHEN** a `LockOperation` event arrives with `LockOperationType` = 2 (NonAccessUserEvent)
- **THEN** the event script SHALL return no `output` key and the `locked` resource SHALL NOT be updated

### Requirement: seedFrom JSON schema entry
The SBMD JSON schema (`sbmd-spec-schema.json`) SHALL define a `seedFromMapper` type requiring `alias` (string) and `script` (string), and SHALL add `seedFrom` as an optional property of the `mapper` object.

#### Scenario: seedFrom schema present in mapper
- **WHEN** a mapper contains a `seedFrom` object with `alias` and `script`
- **THEN** JSON schema validation SHALL accept the mapper

#### Scenario: seedFrom missing required fields fails schema validation
- **WHEN** a mapper contains a `seedFrom` object that omits `script`
- **THEN** JSON schema validation SHALL reject the spec
