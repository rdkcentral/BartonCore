## ADDED Requirements

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
