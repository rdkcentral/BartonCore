## MODIFIED Requirements

### Requirement: Resource definitions with mappers
Each resource in an SBMD endpoint SHALL have `id` (string), `type` (string), and a `mapper` object. Resources MAY also specify `modes` (array of mode strings: `"read"`, `"write"`, `"execute"`, `"dynamic"`, `"emitEvents"`, `"lazySaveNext"`, `"sensitive"`) — if omitted, a default set is used. Resources MAY be marked `optional: true`.

Resources with a read mapper or event mapper SHALL declare a `prerequisites` field. The `prerequisites` field SHALL be either the scalar `none` (meaning the resource is always registered) or a list of prerequisite entries, each referencing a named alias in `matterMeta.aliases` (see the `sbmd-resource-prerequisites` capability spec). Absence of `prerequisites` on any resource SHALL be a parse-time error, regardless of which mappers the resource implements.

Read mappers SHALL reference an alias name via `alias: <name>` in place of an inline `attribute:` block. Event mappers SHALL reference an alias name via `alias: <name>` in place of an inline `event:` block. Named aliases are defined in `matterMeta.aliases`.

#### Scenario: Required resource — mapper bind failure
- **WHEN** a resource is defined without `optional: true` and the resource's mapper cannot be set up (e.g., endpoint resolution fails)
- **THEN** the driver SHALL fail device configuration

#### Scenario: Required resource — unmet prerequisites
- **WHEN** a resource is defined without `optional: true` and its declared prerequisites are not satisfied by the device's data cache
- **THEN** the driver SHALL fail device configuration (`AddDevice()` returns false)

#### Scenario: Optional resource
- **WHEN** a resource is defined with `optional: true` and the required cluster is not present
- **THEN** the driver SHALL skip the resource and continue with remaining resources

#### Scenario: Read mapper resource requires prerequisites field
- **WHEN** a resource has a `mapper.read` section and no `prerequisites` field
- **THEN** the parser SHALL reject the spec with an error

#### Scenario: Resource with prerequisites: none always registered
- **WHEN** a resource declares `prerequisites: none` and has a read mapper
- **THEN** the resource SHALL be registered unconditionally (no cluster/attribute gate applied)

### Requirement: Matter element aliases in `matterMeta`
The `matterMeta` section MAY contain an `aliases` list. Aliases declare the Matter cluster/attribute/event elements used by the driver and give each a unique spec-author-defined name. All references to Matter element metadata (in prerequisites and in mapper metadata) SHALL use alias names — inline `clusterId`/`attributeId` blocks in mappers and inline cluster IDs in prerequisites are not permitted.

#### Scenario: Alias resolves to mapper attribute metadata
- **WHEN** a read mapper declares `alias: <name>` and that alias is defined with `attribute` metadata
- **THEN** the driver's read mapper SHALL use the alias's cluster and attribute IDs for subscription and reads

#### Scenario: Alias resolves to prerequisite cluster check
- **WHEN** a prerequisite entry declares `alias: <name>` referencing an attribute alias
- **THEN** the prerequisite SHALL check that both the alias's cluster and attribute are present in the device's data cache

### Requirement: Read mapper
A resource's mapper MAY contain a `read` section with an `alias` (a string naming an attribute alias defined in `matterMeta.aliases`) and a `script` (JavaScript string). Alternatively, a `command` may be specified instead of `alias`. The alias is resolved at parse time to `clusterId`, `attributeId`, `name`, and `type`. The script SHALL receive the attribute value as TLV base64 via `sbmdReadArgs.tlvBase64` along with additional context fields (`clusterId`, `attributeId`, `attributeName`, `attributeType`, `endpointId`, `deviceUuid`, `clusterFeatureMaps`) and return the Barton string value. Inline `attribute:` blocks are not permitted — all attribute metadata comes from an alias.

#### Scenario: Read alias mapper resolves attribute metadata
- **WHEN** a read mapper declares `alias: stateValue` and `stateValue` is an attribute alias with `clusterId: 0x0045`, `attributeId: 0x0000`
- **THEN** the driver SHALL subscribe to and read cluster `0x0045`, attribute `0x0000`

### Requirement: Event mapper
A resource's mapper MAY contain an `event` section with an `alias` (a string naming an event alias defined in `matterMeta.aliases`) and a `script`. The alias is resolved at parse time to `clusterId`, `eventId`, and `name`. When the event fires, the script SHALL receive the event TLV via `sbmdEventArgs.tlvBase64` and return the updated resource value. Inline `event:` blocks (with inline `clusterId`, `eventId`, `name`) are not permitted — all event metadata comes from an alias.

#### Scenario: Event alias mapper resolves event metadata
- **WHEN** an event mapper declares `alias: lockOperation` and `lockOperation` is an event alias with `clusterId: 0x0101`, `eventId: 0x0002`
- **THEN** the driver SHALL subscribe to event `0x0002` on cluster `0x0101`
