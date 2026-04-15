## Context

SBMD (Specification-Based Matter Driver) specs declare resources whose mappers reference specific Matter clusters and attributes. At commissioning time, `SpecBasedMatterDeviceDriver::AddDevice()` binds each resource's mapper to the device's data and then registers resources via `DoRegisterResources()`. Currently there is no way to gate registration on whether the device actually has a given cluster or attribute — either a resource is always registered (with `optional: true` providing only a non-fatal bind failure), or it is registered unconditionally and may cause errors if the cluster is absent.

The `DeviceDataCache` (populated during commissioning from the device's Descriptor cluster and attribute subscriptions) provides two relevant query methods:

- `EndpointHasServerCluster(endpointId, clusterId)` — checks cluster presence on a specific endpoint
- `GetAttributeData(ConcreteDataAttributePath, TLVReader)` — fetches cached attribute data; returns `CHIP_NO_ERROR` only when data is present

The existing skip mechanism (`skippedOptionalResources` map, checked in both `AddDevice()` and `DoRegisterResources()`) provides the correct infrastructure for silently omitting resources — prerequisites will populate this same map when unmet.

## Goals / Non-Goals

**Goals:**
- Allow SBMD resource definitions to declare declarative prerequisite gates (cluster present, optionally attribute present) checked before registration.
- Define Matter element metadata (cluster/attribute/event IDs, type, human-readable name) once as named **aliases** under `matterMeta.aliases`, then reference those aliases by name in both `prerequisites` and mapper metadata — eliminating all repetition.
- Enforce at parse time that every resource declares `prerequisites` (either a list or the explicit `none` opt-out).
- Gate is evaluated against `DeviceDataCache` at `AddDevice()` time; unmet prerequisites cause silent skip (same mechanism as existing skipped optional resources).
- Test-driven: parser and runtime tests are written before implementation.

**Non-Goals:**
- O.a+ conformance checks (FeatureMap-based or compound boolean expressions over device data).
- Prerequisites on non-SBMD device drivers.
- Changing `optional: true` semantics.
- Dynamic re-evaluation after commissioning.
- Inline `clusterId`/`attributeId` metadata in mappers or prerequisites — all element metadata goes through aliases.

## Decisions

### Decision 1: `SbmdAlias` and alias-based `SbmdPrerequisite`

Matter element metadata is defined once as named aliases in `matterMeta.aliases`. Each alias has:
- `name`: spec-author-chosen identifier, unique within the driver spec
- Either `attribute` (with `clusterId`, `attributeId`, `name`, `type`) or `event` (with `clusterId`, `eventId`, `name`)

The inner `name` field within `attribute`/`event` carries the Matter spec's element name (e.g. `"LockState"`) and is exposed to JavaScript scripts as `sbmdReadArgs.attributeName` / `sbmdEventArgs.eventName`. It serves as documentation of the Matter spec element and as a script-facing string; it is not required to be globally unique.

Prerequisites reference an alias by name:
```yaml
prerequisites:
  - alias: lockState      # resolved at parse time to clusterId + attributeId
  - alias: lockOperation  # resolved at parse time to clusterId only (event alias)
```

The C++ `SbmdPrerequisite` struct after parse-time resolution:
```cpp
struct SbmdPrerequisite {
    uint32_t clusterId = 0;
    std::vector<uint32_t> attributeIds;  // empty = cluster presence sufficient
};
```
There is no `mapperRef` field — the former `from: read`/`from: event` shorthand is removed. All prerequisites are resolved to explicit `clusterId`/`attributeIds` at parse time via alias lookup.

**Alias lookup for attribute alias** → `clusterId = alias.attribute.clusterId`, `attributeIds = {alias.attribute.attributeId}`
**Alias lookup for event alias** → `clusterId = alias.event.clusterId`, `attributeIds = {}` (cluster presence only)

The C++ `SbmdAlias` struct:
```cpp
struct SbmdAlias {
    std::string name;
    std::optional<SbmdAttribute> attribute;
    std::optional<SbmdEvent> event;
};
```
Added to `SbmdMatterMeta`:
```cpp
std::vector<SbmdAlias> aliases;
```

**Rationale**: Defining element metadata exactly once — in `matterMeta.aliases` — and referencing it only by name everywhere eliminates all repetition and makes the spec author's intent clear. The alias is agnostic about how it is used; `prerequisites` and mapper fields are the consumers. Allowing two parallel systems (`from:` shorthand + `clusterId` inline) would be confusing and defeat the purpose.

### Decision 2: Alias-based mapper metadata

Mapper sections that previously embedded inline attribute/event metadata now reference aliases:

```yaml
mapper:
  read:
    alias: lockState      # replaces the inline 'attribute:' block
    script: |
      ...
  event:
    alias: lockOperation  # replaces the inner 'event:' block
    script: |
      ...
```

The parser resolves the alias at parse time and populates the same `SbmdMapper.readAttribute` / `SbmdMapper.event` fields that the rest of the runtime already uses. No runtime changes beyond `CheckPrerequisites()` simplification are needed — all consumers of `readAttribute` and `event` continue to work unchanged.

**Alternative considered**: Keeping both `attribute:` inline and `alias:` as valid options. Rejected because supporting two syntaxes doubles the parser complexity, the schema, and the spec-author mental model. Aliases are the single canonical form.

### Decision 3: `prerequisites` field type — `std::vector<SbmdPrerequisite>`

- Empty vector — `prerequisites: none` in YAML; always register (explicit opt-out of gating).
- Non-empty vector — all entries must be satisfied for the resource to be registered.

**Note**: An earlier draft used `std::optional<std::vector<SbmdPrerequisite>>` to distinguish "field absent" from "declared empty". This was removed in favor of a plain `std::vector<SbmdPrerequisite>`. Parse-time enforcement of the field's presence uses a local `bool prerequisitesDeclared` flag in `ParseResource()` — the same pattern already used for other required fields. `std::nullopt` no longer plays any role.

**Rationale**: Using `std::optional` to mean "not declared" (distinct from "declared empty") was unnecessary complexity — the parser can track `prerequisitesDeclared` directly. There is no mapper-type exemption: `prerequisites` is a resource-level declaration about what the *device* must support — it is orthogonal to which mappers the resource implements. `prerequisites: none` is the explicit acknowledgement that the resource should always be registered.

### Decision 4: Parser enforcement

`SbmdParser::ParseResource()` will check: the `prerequisites` YAML node must be present on every resource. A missing `prerequisites` field is always a parse error (returns `nullptr` from `ParseYamlNode`). Additionally, each alias referenced in `prerequisites` or mapper metadata must exist in `matterMeta.aliases`; an unresolved alias name is also a parse error.

`matterMeta` is parsed before resources and endpoints in `ParseYamlNode`, so the aliases vector is available when `ParseResource` is called.

**Rationale**: `prerequisites` is a resource-level statement of device requirements — it is orthogonal to which mappers the resource uses. Tying enforcement to mapper type creates an asymmetry with no principled basis: a write-only resource for a command on an optional cluster may legitimately need a prerequisite gate, and more importantly, requiring the field universally ensures spec authors make a deliberate choice rather than accidentally relying on an implicit exemption. Failing loudly at parse time (and therefore at build-time schema validation) catches omissions early.

### Decision 5: Prerequisite resolution in `SpecBasedMatterDeviceDriver`

Because aliases are resolved at parse time, `SbmdPrerequisite` fields `clusterId` and `attributeIds` are already fully populated by the time `CheckPrerequisites()` is called. The `mapperRef` field that previously held `"read"` or `"event"` no longer exists. `CheckPrerequisites()` therefore iterates prerequisites directly without any alias or mapper-reference resolution step:

```cpp
for (const auto &prereq : resource.prerequisites)
{
    // clusterId and attributeIds already resolved at parse time
    // check cluster presence ...
    // check attribute presence ...
}
```

If `CheckPrerequisites()` returns false:
- If `resource.optional` is `true`: the resource is added to `skippedOptionalResources` and skipped silently (debug log only). `DoRegisterResources()` skips it via the existing mechanism.
- If `resource.optional` is `false`: `AddDevice()` returns `false` immediately, aborting commissioning.

This mirrors the existing behavior of `configureResource` failure: the same optional/required distinction applies whether the gate is a mapper bind failure or an unmet prerequisite.

Cluster presence check: iterate `DeviceDataCache::GetEndpointIds()`, call `EndpointHasServerCluster(ep, clusterId)` — any match satisfies the cluster prerequisite.

Attribute presence check: for each `attributeId` in the prerequisite, iterate endpoints that have the cluster, call `GetAttributeData()` with a `ConcreteDataAttributePath` — `CHIP_NO_ERROR` return means the attribute is present and cached.

**Rationale**: Parse-time alias resolution moves all logic to where it belongs (the parser) and leaves the runtime check simple and uniform. The runtime only needs to know `clusterId` and `attributeIds` — it doesn't care how those were derived.

### Decision 6: Test-driven development order

For each component (parser, driver), unit tests are written first (in the `core/test/src/` directory alongside existing `sbmdParserTest.cpp`), committed as failing, and implementation follows in the same PR batch.

## Risks / Trade-offs

**[Risk] Attribute presence check via `GetAttributeData()` may be unreliable for attributes with CHIP_IM_GLOBAL_STATUS_UNSUPPORTED** → **Mitigation**: Use `EndpointHasServerCluster()` as the primary cluster check; attribute checking via `GetAttributeData()` is supplementary and a `CHIP_NO_ERROR` result is sufficient to confirm presence.

**[Risk] Parser enforcement is a breaking change for existing specs** → **Mitigation**: All existing `.sbmd` files must be updated in the same PR to add `matterMeta.aliases` and switch to alias-based prerequisites and mapper metadata. Schema validation at build time catches any missed updates.

**[Risk] Parse-time alias resolution requires `matterMeta` to be parsed before resources** → **Mitigation**: `ParseYamlNode` already parses `bartonMeta` and `matterMeta` before iterating resources and endpoints. The aliases vector is passed as a parameter to `ParseResource`, `ParseEndpoint`, `ParseMapper`, and `ParsePrerequisites` — no global state needed.

**[Risk] Thread safety: `DeviceDataCache` is queried from the `AddDevice()` call** → **Mitigation**: Unchanged from original design — `AddDevice()` runs on the Matter event loop thread, same as all existing cache queries.

## Migration Plan

1. Add `SbmdAlias` struct, `aliases` field on `SbmdMatterMeta`, and simplified `SbmdPrerequisite` (no `mapperRef`) to `SbmdSpec.h`.
2. Add `ParseAlias()` to `SbmdParser`; update `ParseMatterMeta()` to parse aliases; update `ParseResource()`, `ParseMapper()`, and `ParsePrerequisites()` to accept and use the aliases vector.
3. Simplify `CheckPrerequisites()` in `SpecBasedMatterDeviceDriver.cpp` — remove `mapperRef` resolution block.
4. Update `sbmd-spec-schema.json` to add `alias` definition, `aliases` in `matterMeta`, and update `prerequisite`/`readMapper`/`eventMapper` to use alias form.
5. Update all existing `.sbmd` specs to add `matterMeta.aliases` and switch to alias-based prerequisites and mapper metadata.
6. All existing functionality is preserved — the parsed `readAttribute` and `event` fields in `SbmdMapper` are still populated (via alias resolution), so all downstream consumers are unchanged.

## Open Questions

None — all design decisions were resolved in prior exploration.
