## ADDED Requirements

### Requirement: Unit test skill SKILL.md conforms to Agent Skills spec
The `run-unit-tests` skill SHALL be located at `.github/skills/run-unit-tests/SKILL.md`. The frontmatter SHALL include `name: run-unit-tests`, a `description` field explaining the skill covers running C/C++ unit tests, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: run-unit-tests` matching the directory name
- **AND** the `description` field SHALL mention unit tests, ctest, CMocka, and Google Test

### Requirement: Unit test skill documents test execution
The skill SHALL instruct the agent to run unit tests via `ctest --output-on-failure --test-dir build`. The skill SHALL note that the project must be built first.

#### Scenario: Agent runs all unit tests
- **WHEN** the agent needs to run unit tests
- **THEN** the skill SHALL instruct the agent to run `ctest --output-on-failure --test-dir build`

### Requirement: Unit test skill documents test filtering
The skill SHALL explain how to run a subset of tests using `ctest -R <pattern> --test-dir build` and how to list available tests with `ctest -N --test-dir build`.

#### Scenario: Agent runs specific unit tests
- **WHEN** the agent needs to run only tests matching a pattern
- **THEN** the skill SHALL instruct the agent to use `ctest -R <pattern> --output-on-failure --test-dir build`

### Requirement: Unit test skill documents test frameworks
The skill SHALL note that C tests use CMocka and C++ tests use Google Test/Google Mock. The skill SHALL explain that tests are defined in CMakeLists.txt files using `bcore_add_cmocka_test()` and `bcore_add_cpp_test()` macros, located under `core/test/`, `libs/device/test/`, and `api/c/test/`.

#### Scenario: Agent understands test framework context
- **WHEN** the agent reads the unit test skill
- **THEN** it SHALL know that C tests use CMocka and C++ tests use Google Test
- **AND** it SHALL know the test source locations

### Requirement: Unit test skill includes error recovery pattern
The skill SHALL instruct the agent that if `ctest` fails with a command-not-found error, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: ctest not found outside Docker
- **WHEN** `ctest` fails with a command-not-found error
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container
