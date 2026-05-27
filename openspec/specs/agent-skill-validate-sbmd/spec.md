## ADDED Requirements

### Requirement: SBMD validation skill SKILL.md conforms to Agent Skills spec
The `validate-sbmd` skill SHALL be located at `.github/skills/validate-sbmd/SKILL.md`. The frontmatter SHALL include `name: validate-sbmd`, a `description` field explaining the skill covers validating SBMD specification files, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: validate-sbmd` matching the directory name
- **AND** the `description` SHALL mention SBMD, validation, schema, and Matter drivers

### Requirement: SBMD validation skill documents spec validation
The skill SHALL explain how to validate SBMD YAML files using `scripts/ci/validate_sbmd_specs.py`. The skill SHALL document the command syntax: `python3 scripts/ci/validate_sbmd_specs.py <schema_file> <sbmd_file> [<sbmd_file> ...]`. The skill SHALL note the validator checks both YAML schema conformance and embedded JavaScript syntax.

#### Scenario: Agent validates SBMD specs
- **WHEN** the agent needs to validate SBMD spec files after editing
- **THEN** the skill SHALL show the validation command with the correct schema and spec file paths

### Requirement: SBMD validation skill documents stub generation
The skill SHALL explain how to generate JavaScript stubs from TypeScript definitions using `scripts/ci/generate_sbmd_stubs.py`. The skill SHALL document the command syntax: `python3 scripts/ci/generate_sbmd_stubs.py <d.ts_file> <output_json>`. The skill SHALL note that stubs keep the validator in sync with TypeScript interface definitions.

#### Scenario: Agent regenerates SBMD stubs
- **WHEN** the agent has modified TypeScript definition files
- **THEN** the skill SHALL show the stub generation command

### Requirement: SBMD validation skill documents spec file locations
The skill SHALL identify that SBMD spec files live at `core/deviceDrivers/matter/sbmd/specs/`, the schema is generated during the build, and the stubs output is at `build/sbmd-stubs.json`. The skill SHALL note that validation runs automatically during build when `BCORE_MATTER_VALIDATE_SCHEMAS=ON` (the default).

#### Scenario: Agent finds SBMD specs
- **WHEN** the agent needs to locate SBMD spec files
- **THEN** the skill SHALL direct the agent to `core/deviceDrivers/matter/sbmd/specs/`

### Requirement: SBMD validation skill includes error recovery pattern
The skill SHALL instruct the agent that if `python3` or validation dependencies are not found, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: Validation tools not found outside Docker
- **WHEN** validation scripts fail with missing dependencies
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container
