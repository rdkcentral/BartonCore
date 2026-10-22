## MODIFIED Requirements

### Requirement: String-serialized resource values
All resource values SHALL be stored and transmitted as strings, regardless of the underlying data type, or as `NULL` when the value is unknown. Boolean values SHALL be `"true"` or `"false"`. Numeric values SHALL be decimal string representations. Complex values (JSON objects) SHALL be serialized as JSON strings. A `NULL` value SHALL mean "value unknown" for every resource type.

#### Scenario: Boolean resource value
- **WHEN** a boolean resource `isOn` is set to true
- **THEN** its value SHALL be the string `"true"`

#### Scenario: Temperature resource value
- **WHEN** a temperature resource has a value of 1900 (hundredths of degrees)
- **THEN** its value SHALL be the string `"1900"`

#### Scenario: Unknown resource value
- **WHEN** a device reports a Matter nullable attribute as null
- **THEN** the corresponding resource value SHALL be `NULL`
