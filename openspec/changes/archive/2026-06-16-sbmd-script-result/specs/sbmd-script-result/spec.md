## ADDED Requirements

### Requirement: ScriptResult represents the observable contract of a mapper script
A `ScriptResult` holds two optional fields: `error` and `operation`. Their presence or absence determines the observable outcome. When `error` is set, the script failed. When `operation` is set and `error` is not, the script produced an action for the caller to take. When neither is set, the script ran successfully but produced no action — this is the skip-resource-update outcome. `SkipsResourceUpdate()` is a derived convenience accessor, not an independent state field.

#### Scenario: Error state when error field is present
- **WHEN** `ScriptResult` has a non-empty `error` field
- **THEN** `IsError()` SHALL return `true` and the error message SHALL be accessible via `ErrorMessage()`

#### Scenario: SkipsResourceUpdate derived when no fields are set
- **WHEN** `ScriptResult` has neither an `error` field nor an `operation` field set
- **THEN** `SkipsResourceUpdate()` SHALL return `true`, `IsError()` SHALL return `false`, and `HasOperation()` SHALL return `false`

#### Scenario: Operation state when operation is present
- **WHEN** `ScriptResult` has an `operation` field set and no `error` field
- **THEN** `HasOperation()` SHALL return `true`, `IsError()` SHALL return `false`, and `SkipsResourceUpdate()` SHALL return `false`

---

### Requirement: ScriptResult parses all valid script JSON shapes
`ScriptResult::FromJsonValue()` SHALL parse a `Json::Value` object into a `ScriptResult` according to the SBMD script JSON schema v3.0.

#### Scenario: Empty object produces no-op
- **WHEN** the JSON object has no recognized keys
- **THEN** `FromJsonValue()` SHALL return a `ScriptResult` where `SkipsResourceUpdate()` is `true`

#### Scenario: `value` key produces ResourceUpdate operation
- **WHEN** the JSON object contains a `value` key with a string, number, or boolean
- **THEN** `FromJsonValue()` SHALL return a `ScriptResult` where `HasOperation()` is `true` and the operation is a `ResourceUpdate` containing the string representation of the value

#### Scenario: `invoke` key produces Invoke operation
- **WHEN** the JSON object contains a valid `invoke` sub-object with `clusterId` and `commandId`
- **THEN** `FromJsonValue()` SHALL return a `ScriptResult` where `HasOperation()` is `true` and the operation is a `ScriptWriteResult` with `OperationType::Invoke`

#### Scenario: `write` key produces Write operation
- **WHEN** the JSON object contains a valid `write` sub-object with `clusterId`, `attributeId`, and `tlvBase64`
- **THEN** `FromJsonValue()` SHALL return a `ScriptResult` where `HasOperation()` is `true` and the operation is a `ScriptWriteResult` with `OperationType::Write`

#### Scenario: `error` key produces error state
- **WHEN** the JSON object contains an `error` key with a string value
- **THEN** `FromJsonValue()` SHALL return a `ScriptResult` where `IsError()` is `true` and `ErrorMessage()` returns the error string

---

### Requirement: ScriptResult rejects ambiguous or malformed JSON as an error
`ScriptResult::FromJsonValue()` SHALL return an error `ScriptResult` whenever the JSON object is structurally invalid or ambiguous.

#### Scenario: Multiple payload keys present
- **WHEN** the JSON object contains more than one of `value`, `invoke`, `write`, or `error`
- **THEN** `FromJsonValue()` SHALL return an error `ScriptResult` with a descriptive message identifying the conflict

#### Scenario: `invoke` missing required clusterId
- **WHEN** the JSON object contains an `invoke` key but the sub-object is missing `clusterId`
- **THEN** `FromJsonValue()` SHALL return an error `ScriptResult`

#### Scenario: `invoke` missing required commandId
- **WHEN** the JSON object contains an `invoke` key but the sub-object is missing `commandId`
- **THEN** `FromJsonValue()` SHALL return an error `ScriptResult`

#### Scenario: `write` missing required attributeId
- **WHEN** the JSON object contains a `write` key but the sub-object is missing `attributeId`
- **THEN** `FromJsonValue()` SHALL return an error `ScriptResult`

#### Scenario: `write` missing required tlvBase64
- **WHEN** the JSON object contains a `write` key but the sub-object is missing `tlvBase64`
- **THEN** `FromJsonValue()` SHALL return an error `ScriptResult`

#### Scenario: `write` missing required clusterId
- **WHEN** the JSON object contains a `write` key but the sub-object is missing `clusterId`
- **THEN** `FromJsonValue()` SHALL return an error `ScriptResult`

#### Scenario: Non-object return treated as error
- **WHEN** the JSON value passed to `FromJsonValue()` is not a JSON object (e.g. null, string, number, or array)
- **THEN** `FromJsonValue()` SHALL return an error `ScriptResult`

---

### Requirement: JS engine exceptions produce error ScriptResults
When a mapper script throws an unhandled JavaScript exception, the engine implementation SHALL convert the exception into an error `ScriptResult` rather than propagating a native exception or returning a `bool false`.

#### Scenario: JS exception maps to error state
- **WHEN** a script throws a JavaScript exception during execution
- **THEN** the engine SHALL return a `ScriptResult` where `IsError()` is `true` and `ErrorMessage()` contains the exception message

---

### Requirement: SbmdScript mapper methods return ScriptResult
All mapper methods on the `SbmdScript` abstract interface SHALL return `ScriptResult` by value, replacing the previous `bool` + output-parameter pattern.

#### Scenario: Successful read mapper
- **WHEN** `MapAttributeRead()` is called and the script returns `{ "value": "true" }`
- **THEN** the returned `ScriptResult` SHALL have `HasOperation()` true with a `ResourceUpdate` containing `"true"`

#### Scenario: No-op event mapper
- **WHEN** `MapEvent()` is called and the script returns `{}`
- **THEN** the returned `ScriptResult` SHALL have `SkipsResourceUpdate()` true

#### Scenario: No-op read mapper on explicit resource read
- **WHEN** `MapAttributeRead()` is called (from an explicit `read_resource` request) and the script returns `{}` or `{ value: null }`
- **THEN** the MatterDevice layer SHALL NOT fail the read operation; the caller SHALL receive a null value, consistent with how no-op is handled in subscription updates and seedFrom

#### Scenario: Failed mapper
- **WHEN** `MapWrite()` is called and the script throws an exception
- **THEN** the returned `ScriptResult` SHALL have `IsError()` true

---

### Requirement: SBMD script JSON schema v3.0 defines `value` and `error` keys
The SBMD script JSON schema version 3.0 SHALL define the following valid top-level keys for script return objects: `value` (string/number/boolean), `invoke` (object), `write` (object), `error` (string). The `output` key from schema v2.x is removed.

#### Scenario: Script uses `value` for resource update
- **WHEN** a read or event mapper script returns `{ value: "42" }`
- **THEN** the Barton resource SHALL be updated with the string `"42"`

#### Scenario: Script uses `error` for intentional error
- **WHEN** a mapper script returns `{ error: "PIN required but not provided" }`
- **THEN** `ScriptResult::IsError()` SHALL return `true` and the error string SHALL appear in system logs

#### Scenario: Script uses `Sbmd.Response.value()` helper
- **WHEN** a read mapper script calls `return Sbmd.Response.value("locked")`
- **THEN** the returned JSON SHALL be `{ "value": "locked" }` and the resource SHALL be updated

#### Scenario: Script uses `Sbmd.Response.error()` helper
- **WHEN** a mapper script calls `return Sbmd.Response.error("unexpected TLV format")`
- **THEN** the returned JSON SHALL be `{ "error": "unexpected TLV format" }` and `ScriptResult::IsError()` SHALL return `true`

---

### Requirement: All bundled SBMD specs comply with schema v3.0
All `.sbmd` specification files in `core/deviceDrivers/matter/sbmd/specs/` SHALL use `schemaVersion: "3.0"` and SHALL use `value` instead of `output` in all script return objects.

#### Scenario: Schema version declared in spec file
- **WHEN** any bundled `.sbmd` file is parsed
- **THEN** the `schemaVersion` field SHALL equal `"3.0"`

#### Scenario: No `output` key in bundled spec scripts
- **WHEN** any bundled `.sbmd` spec file is inspected
- **THEN** no script body SHALL contain `return { output:` or `return {output:`
