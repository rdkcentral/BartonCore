## ADDED Requirements

### Requirement: Not-found errors for value reads
`b_core_client_read_resource` SHALL set `RESOURCE_NOT_FOUND` (2) and `b_core_client_read_metadata` SHALL set `METADATA_NOT_FOUND` (201) when the target does not exist. A new `BCoreSystemPropertyError` range (300-399) SHALL define `SYSTEM_PROPERTY_NOT_FOUND` (300), used by `b_core_client_read_system_property`.

#### Scenario: Metadata not found
- **WHEN** a client reads metadata that was never written
- **THEN** the call SHALL return `NULL` and set `METADATA_NOT_FOUND`

### Requirement: Explicit delete for metadata and system properties
The API SHALL provide `b_core_client_delete_metadata` and `b_core_client_delete_system_property`, returning `TRUE` only when an entry was removed.

#### Scenario: Delete missing property
- **WHEN** a client deletes a system property that does not exist
- **THEN** the call SHALL return `FALSE`

### Requirement: Nullable annotations
Every public getter, property, and event property that carries a resource, metadata, or system-property value SHALL be annotated `(nullable)` for GObject Introspection.

#### Scenario: Binding sees nullable
- **WHEN** the GIR for `BCoreResource:value` is generated
- **THEN** it SHALL mark the value as nullable
