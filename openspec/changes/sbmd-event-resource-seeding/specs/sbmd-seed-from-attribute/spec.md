## ADDED Requirements

### Requirement: seedFrom mapper — schema and structure
A resource's mapper MAY contain a `seedFrom` section with an `alias` (a string naming an **attribute** alias defined in `matterMeta.aliases`) and a `script` (JavaScript string). The `seedFrom` mapper SHALL only appear when the same resource also declares a `mapper.event` section. The `alias` SHALL resolve to an attribute alias; event aliases SHALL NOT be accepted. The `script` SHALL be required; absence of `script` SHALL be a parse error.

#### Scenario: Valid seedFrom mapper
- **WHEN** a mapper declares `seedFrom.alias: lockState` and `lockState` is an attribute alias, and `seedFrom.script` is present, and `mapper.event` is also present
- **THEN** the parser SHALL populate `hasInitialRead = true`, `initialReadAttribute` with the resolved cluster/attribute/type, and `initialReadScript` with the script text

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
When a device is configured (during commissioning), the driver SHALL read each resource's `seedFrom` attribute from the device data cache and update the resource value via `updateResource()`. This SHALL occur after the attribute cache has been fully primed (i.e., after `DeviceDataCache::Start()` completes). Resources in `skippedOptionalResources` SHALL be skipped. If the attribute is absent from the cache, the resource SHALL NOT be updated.

#### Scenario: Locked resource seeded at commissioning
- **WHEN** a door lock device is commissioned and `LockState` (cluster `0x0101`, attribute `0x0000`) is present in the attribute cache
- **THEN** the `locked` resource SHALL receive a value from the `seedFrom` script at configure time before any event has fired

#### Scenario: Optional resource not seeded when skipped
- **WHEN** a resource is marked `optional: true` and was skipped due to absent cluster
- **THEN** `SeedInitialResourceValues()` SHALL NOT attempt to read its seedFrom attribute

#### Scenario: Cache miss — resource not updated
- **WHEN** the seedFrom attribute is absent from the device data cache at configure time
- **THEN** `updateResource()` SHALL NOT be called for that resource

### Requirement: seedFrom seeds resource at synchronize time
When a device is synchronized (at hub restart or communication restoration), the driver SHALL re-read each resource's `seedFrom` attribute from the device data cache and update the resource value via `updateResource()`. This SHALL use the same `SeedInitialResourceValues()` helper as configure time and apply the same guards (skipped resources, cache miss).

#### Scenario: Locked resource re-seeded at hub restart
- **WHEN** the hub restarts and `SynchronizeDevice()` is called for a commissioned door lock
- **THEN** the `locked` resource SHALL be re-seeded from the `LockState` attribute cache before any event fires

### Requirement: seedFrom script interface
The JavaScript script in `mapper.seedFrom` SHALL receive attribute value data via the same `sbmdReadArgs` interface used by `mapper.read` scripts: `sbmdReadArgs.tlvBase64` (base64-encoded TLV of the attribute value), `sbmdReadArgs.clusterId`, `sbmdReadArgs.attributeId`, `sbmdReadArgs.attributeName`, `sbmdReadArgs.attributeType`, `sbmdReadArgs.endpointId`, `sbmdReadArgs.deviceUuid`, and `sbmdReadArgs.clusterFeatureMaps`. The script SHALL return the Barton string value for the resource.

#### Scenario: seedFrom script decodes uint8 to boolean
- **WHEN** a `seedFrom` script receives a `uint8` TLV for the `LockState` attribute (value `1` = locked)
- **THEN** the script SHALL decode the value and return `"true"` for the `locked` resource

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
