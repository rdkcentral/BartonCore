## ADDED Requirements

### Requirement: NULL value means unknown
A resource, metadata entry, or system property whose value is `NULL` SHALL be interpreted as "exists, value unknown". An empty string SHALL be interpreted as a known empty value. Not-found SHALL be reported out-of-band and never solely as a `NULL` return.

#### Scenario: Unknown resource read
- **WHEN** a client reads a readable resource whose value is `NULL`
- **THEN** the call SHALL return `NULL` and SHALL NOT set a `GError`

#### Scenario: Missing resource read
- **WHEN** a client reads a resource URI that does not exist
- **THEN** the call SHALL return `NULL` and set `B_CORE_CLIENT_ERROR` / `RESOURCE_NOT_FOUND`

#### Scenario: Empty string preserved
- **WHEN** a value of `""` is stored and reloaded
- **THEN** it SHALL be returned as `""`, not `NULL`

### Requirement: Clients may write NULL to resources
The device service SHALL pass a client `NULL` resource write to the owning driver. A driver SHALL either apply it (updating the resource to `NULL`) or reject it by returning failure, and SHALL NOT dereference a `NULL` `newValue` or `previousValue`.

#### Scenario: NULL resource write accepted
- **WHEN** `b_core_client_write_resource` is called with a `NULL` value on a resource whose driver supports unknown
- **THEN** it SHALL return `TRUE`, the resource value SHALL become `NULL`, and a resource-updated event SHALL be emitted

#### Scenario: NULL resource write rejected by driver
- **WHEN** `b_core_client_write_resource` is called with a `NULL` value on a resource whose driver does not support unknown
- **THEN** it SHALL return `FALSE` and the resource value SHALL be unchanged

### Requirement: Clients may record unknown metadata and system properties
`b_core_client_write_metadata` and `b_core_client_set_system_property` SHALL accept `NULL`. Removal SHALL use `b_core_client_delete_metadata` / `b_core_client_delete_system_property`.

#### Scenario: Unknown system property
- **WHEN** a system property is set to `NULL`
- **THEN** `b_core_client_get_system_property` SHALL return `NULL` without error

#### Scenario: Deleted system property
- **WHEN** a system property is deleted
- **THEN** `b_core_client_get_system_property` SHALL set `SYSTEM_PROPERTY_NOT_FOUND`

### Requirement: NULL survives persistence
Resources, metadata, and system properties with `NULL` values SHALL be written as JSON `null` and reloaded as `NULL`. A missing value key in existing files SHALL load as `NULL`.

#### Scenario: Restart preserves unknown
- **WHEN** an item is set to `NULL` and the service restarts
- **THEN** the item SHALL exist with value `NULL`

### Requirement: Sensitive values never persist in plaintext
For `RESOURCE_MODE_SENSITIVE` resources, a `NULL` value SHALL be stored as JSON `null` without invoking encryption. If encryption of a non-`NULL` value fails, the record SHALL NOT be saved.

#### Scenario: Encryption failure
- **WHEN** encryption of a sensitive resource value fails during save
- **THEN** no plaintext value SHALL be written and the previous stored record SHALL remain

### Requirement: Unknown changes emit events
Transitions between a value and `NULL` in either direction SHALL emit the corresponding updated event with a nullable value.

#### Scenario: Resource becomes unknown
- **WHEN** a driver updates a resource from `"true"` to `NULL`
- **THEN** a resource-updated event SHALL be emitted with a `NULL` value

### Requirement: SBMD scripts can report unknown
An SBMD script result with `value: null` SHALL set the target to `NULL`. A result without a `value` property SHALL be treated as a script error.

#### Scenario: Script returns null
- **WHEN** a mapper returns `{ value: null }`
- **THEN** the resource SHALL be updated to `NULL`
