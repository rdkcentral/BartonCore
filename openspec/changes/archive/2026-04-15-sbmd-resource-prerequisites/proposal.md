## Why

Matter device types expose optional clusters and optional attributes within required clusters, meaning the set of resources a device actually supports can only be determined at commissioning time from discovered data — not from the SBMD spec alone. Without a prerequisite gate, SBMD specs have no principled way to conditionally register resources: the spec either always registers a resource (risking failures when the cluster/attribute is absent) or marks it `optional: true` (which defers failure to bind time rather than preventing the attempt). A prerequisite system allows a resource to declare exactly which cluster and attribute presence is required before registration is attempted, making conditional registration explicit, deterministic, and spec-driven.

## What Changes

- **New `prerequisites` field on SBMD resources**: Each resource may declare a list of prerequisites — each prerequisite specifies an alias name. The alias resolves at parse time to the cluster and optional attribute the device must have present.
- **Aliases in `matterMeta`**: Each driver spec defines a named list of Matter elements (attributes or events) under `matterMeta.aliases`. An alias gives a stable author-chosen name to an element's cluster/attribute/event metadata (`clusterId`, `attributeId` or `eventId`, `name`, `type`). This is the single source of truth for each Matter element used by the spec.
- **Alias-based mapper metadata**: Resources reference aliases in both their `prerequisites` and their read/event mapper metadata. The `mapper.read` section uses `alias: <name>` instead of an inline `attribute:` block; the `mapper.event` section similarly uses `alias: <name>`. This eliminates all repetition of cluster/attribute/event IDs across the spec.
- **`prerequisites: none` explicit opt-out**: A resource may declare `prerequisites: none` to explicitly assert that it should always be registered without a cluster/attribute gate.
- **Parser enforcement**: Every resource MUST declare `prerequisites` (either a list or `none`). This is enforced at parse time regardless of which mappers the resource implements.
- **Runtime prerequisite checking**: Before registering a resource, `SpecBasedMatterDeviceDriver` evaluates the resource's prerequisites against the device's `DeviceDataCache`. If any prerequisite is unmet: for `optional` resources the resource is silently skipped and commissioning continues; for required (non-optional) resources, `AddDevice()` returns false and commissioning is aborted. This mirrors the existing behavior of mapper bind failure: the `optional` flag governs failure tolerance in both cases.
- **SBMD spec file updates**: Existing specs (primarily `air-quality-sensor.sbmd`) will be updated to use `matterMeta.aliases` and reference those aliases in `prerequisites` and mapper metadata.
- **JSON schema update**: `sbmd-spec-schema.json` will be extended with an `alias` definition, `aliases` added to `matterMeta`, the `prerequisite` definition updated to require `alias: <name>` form, and the `readMapper`/`eventMapper` definitions updated to use `alias: <name>`.

## Non-goals

- Conformance (`O.a+`) prerequisite expressions — checking FeatureMap bits, logical combinations of cluster presence, or any form of compound boolean expression over device data.
- Dynamic re-evaluation of prerequisites after initial commissioning.
- Prerequisites on non-SBMD (native C) device drivers.
- Changing the semantics of `optional: true` (behavior unchanged).
- Modifying the Matter commissioning flow or `DeviceDataCache` population logic.

## Capabilities

### New Capabilities

- `sbmd-resource-prerequisites`: Declarative per-resource prerequisite gates in SBMD specs — Matter element aliases defined in `matterMeta.aliases`, referenced by alias name in both `prerequisites` and mapper metadata, evaluated against discovered device data at commissioning time.

### Modified Capabilities

- `sbmd-system`: The `resource` definition gains a new `prerequisites` field; the parser gains enforcement rules for read/event resources; `SpecBasedMatterDeviceDriver` gains runtime prerequisite evaluation before resource registration.

## Impact

- **SBMD driver layer** (`core/deviceDrivers/matter/sbmd/`): `SbmdSpec.h` (new `SbmdAlias` struct, `aliases` field on `SbmdMatterMeta`, simplified `SbmdPrerequisite`), `SbmdParser.cpp/.h` (alias parsing + resolution), `SpecBasedMatterDeviceDriver.cpp/.h` (simplified runtime check), `sbmd-spec-schema.json` (schema extension).
- **SBMD specs** (`core/deviceDrivers/matter/sbmd/specs/`): `air-quality-sensor.sbmd` updated; other specs updated to add `prerequisites: none` opt-outs where needed.
- **Tests** (`core/deviceDrivers/matter/sbmd/test/` or adjacent): New unit tests for parsing and runtime behavior, written before implementation (TDD).
- **CMake flags**: `BCORE_MATTER` — all changes are scoped to Matter-enabled builds.
- **No public API changes**: Prerequisites are an internal SBMD driver concern; the Barton resource model and C API are unchanged.
