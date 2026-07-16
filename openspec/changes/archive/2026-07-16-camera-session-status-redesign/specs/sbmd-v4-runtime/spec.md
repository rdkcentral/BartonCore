## ADDED Requirements

### Requirement: volatile resource mode disables value caching

The SBMD runtime SHALL support a resource mode `volatile`. A resource declared with the `volatile` mode SHALL be registered with `CACHING_POLICY_NEVER`, causing `updateResource` to emit a `resourceUpdated` event on every call (for a resource that emits events) regardless of whether the new value equals the currently stored value. The `volatile` mode SHALL NOT, by itself, add read, write, or execute access, and SHALL be accepted by mode-to-bitmask conversion without error. A resource is registered `CACHING_POLICY_NEVER` when it declares a read handler OR declares the `volatile` mode; otherwise it is registered `CACHING_POLICY_ALWAYS`.

#### Scenario: Volatile resource emits on unchanged value
- **WHEN** a resource declared with `modes: ['volatile']` is updated twice with the same value
- **THEN** the runtime SHALL deliver two `resourceUpdated` events (no value-change suppression)

#### Scenario: Volatile mode is accepted by the schema and runtime
- **WHEN** a driver declares a resource with `volatile` in its modes array
- **THEN** the spec SHALL validate against the SBMD schema AND the resource SHALL register successfully without adding read, write, or execute modes

#### Scenario: Non-volatile resource without a read handler remains cached
- **WHEN** an event-only resource is declared without a read handler and without the `volatile` mode
- **THEN** the runtime SHALL register it with `CACHING_POLICY_ALWAYS` and suppress `resourceUpdated` events whose value is unchanged
