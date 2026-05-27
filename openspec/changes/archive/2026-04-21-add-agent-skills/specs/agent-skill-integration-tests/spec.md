## ADDED Requirements

### Requirement: Integration test skill SKILL.md conforms to Agent Skills spec
The `run-integration-tests` skill SHALL be located at `.github/skills/run-integration-tests/SKILL.md`. The frontmatter SHALL include `name: run-integration-tests`, a `description` field explaining the skill covers running Python integration tests, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: run-integration-tests` matching the directory name
- **AND** the `description` field SHALL mention integration tests, pytest, and Python

### Requirement: Integration test skill documents test execution
The skill SHALL instruct the agent to run integration tests via `testing/py_test.sh testing/` from the repo root. The skill SHALL explain that `py_test.sh` is a wrapper that handles Address Sanitizer (ASAN) `LD_PRELOAD` setup automatically.

#### Scenario: Agent runs all integration tests
- **WHEN** the agent needs to run integration tests
- **THEN** the skill SHALL instruct the agent to run `./testing/py_test.sh testing/`

### Requirement: Integration test skill documents test filtering
The skill SHALL explain how to run specific tests: by file path (`./testing/py_test.sh testing/test/light_test.py`), by keyword (`./testing/py_test.sh testing/ -k "light"`), and by marker (`./testing/py_test.sh testing/ -m "requires_matterjs"`).

#### Scenario: Agent runs specific integration tests
- **WHEN** the agent needs to run a subset of integration tests
- **THEN** the skill SHALL show filtering by file path, `-k` keyword, and `-m` marker

### Requirement: Integration test skill documents prerequisites
The skill SHALL explain the prerequisite chain for integration tests: the project must be built, the built artifacts must be installed (`cmake --build build --target install`), D-Bus must be running (`sudo service dbus start`), and matter.js dependencies must be installed (`npm --prefix testing/mocks/devices/matterjs ci`) for tests using the `requires_matterjs` marker. The skill SHALL note that `scripts/ci/run_integration_tests.sh` performs all prerequisites automatically.

#### Scenario: Agent prepares prerequisites manually
- **WHEN** the agent needs to run integration tests after a fresh build
- **THEN** the skill SHALL list the prerequisite steps in order

#### Scenario: Agent uses CI script for full setup
- **WHEN** the agent wants to ensure all prerequisites are met
- **THEN** the skill SHALL offer `./scripts/ci/run_integration_tests.sh` as the all-in-one option

### Requirement: Integration test skill documents pytest configuration
The skill SHALL note that pytest is configured in `pyproject.toml` with `testpaths = ["testing"]`, log level `DEBUG`, and `--capture=sys`.

#### Scenario: Agent understands test discovery
- **WHEN** the agent needs to find where tests are defined
- **THEN** the skill SHALL direct the agent to `testing/test/` for test files and `pyproject.toml` for pytest configuration

### Requirement: Integration test skill includes error recovery pattern
The skill SHALL instruct the agent that if `pytest` or `py_test.sh` fails with a command-not-found or import error, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: pytest not found outside Docker
- **WHEN** `pytest` fails with a command-not-found or import error
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container
