## ADDED Requirements

### Requirement: Specs conform to OpenSpec v4 structure
Every specification under `openspec/specs/` SHALL conform to the OpenSpec v4 main-spec structure: a top-level title, a `## Purpose` section, a `## Requirements` section, and at least one `#### Scenario:` block per requirement. Delta-only headers (`## ADDED Requirements`, `## MODIFIED Requirements`, `## REMOVED Requirements`) SHALL NOT appear in a checked-in main spec.

#### Scenario: All specs pass strict validation
- **WHEN** `openspec validate --specs --strict` is run at the repository root
- **THEN** every spec SHALL report valid and the command SHALL exit with status 0

#### Scenario: A malformed spec is rejected
- **WHEN** a spec is missing its `## Purpose` or `## Requirements` section, or a requirement has no `#### Scenario:` block
- **THEN** `openspec validate --specs --strict` SHALL report an ERROR for that spec and exit non-zero

### Requirement: Build image provides a pinned openspec CLI
The Docker builder image SHALL install a pinned version of the `openspec` CLI so that spec validation runs identically for developers and in CI. The builder image version SHALL be bumped whenever the installed `openspec` version changes.

#### Scenario: openspec available in the builder image
- **WHEN** a shell is opened inside the builder image
- **THEN** `openspec --version` SHALL succeed and report the pinned version

### Requirement: CI enforces strict spec validation
A CI workflow SHALL run `openspec validate --specs --strict` over all specs on every pull request and SHALL fail the check when any spec is invalid.

#### Scenario: Pull request with a valid spec set
- **WHEN** a pull request is opened and all specs conform to the v4 structure
- **THEN** the OpenSpec validation check SHALL pass

#### Scenario: Pull request introduces a malformed spec
- **WHEN** a pull request adds or edits a spec so that it no longer conforms to the v4 structure
- **THEN** the OpenSpec validation check SHALL fail and block the pull request

### Requirement: Pre-commit hook validates specs locally
The repository's pre-commit hook SHALL run `openspec validate --specs --strict` before each commit and SHALL abort the commit when any spec is invalid.

#### Scenario: Commit with valid specs
- **WHEN** a developer commits with all specs conforming to the v4 structure
- **THEN** the pre-commit hook SHALL pass and the commit SHALL proceed

#### Scenario: Commit with an invalid spec
- **WHEN** a developer attempts to commit a spec that fails strict validation
- **THEN** the pre-commit hook SHALL abort the commit and report the validation error
