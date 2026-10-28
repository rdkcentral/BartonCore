# Agent Skills

## Purpose

Specifies the structure and required content of the repository's AI agent skills (`.github/skills/*/SKILL.md`): each skill conforms to the Agent Skills format and documents its domain — building, debugging, code formatting, unit tests, integration tests, Matter virtual devices, and SBMD validation — including an error-recovery pattern.

## Requirements

### Requirement: Build skill SKILL.md conforms to Agent Skills spec
The `build` skill SHALL be located at `.github/skills/build/SKILL.md`. The frontmatter SHALL include `name: build`, a `description` field explaining the skill covers building BartonCore, and `compatibility` noting it requires the BartonCore Docker development container. The `name` field SHALL match the parent directory name.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: build` matching the directory name
- **AND** the `description` field SHALL be non-empty and under 1024 characters
- **AND** the `description` SHALL mention building, compiling, and CMake

### Requirement: Build skill documents the build hierarchy
The skill body SHALL explain the layered build system: `build.sh` is the top-level convenience script that calls CMake, and CMake in turn invokes `build-matter.sh` when needed. The skill SHALL make clear that for incremental rebuilds after code changes, `cmake --build build` is the preferred command.

#### Scenario: Agent performs incremental rebuild
- **WHEN** the agent needs to rebuild after modifying source files
- **THEN** the skill SHALL instruct the agent to run `cmake --build build`

#### Scenario: Agent performs full build from scratch
- **WHEN** the agent needs a clean or first-time build
- **THEN** the skill SHALL instruct the agent to run `./build.sh`

#### Scenario: Agent understands build-matter.sh is automatic
- **WHEN** the agent reads the build skill
- **THEN** the skill SHALL state that `build-matter.sh` is invoked automatically by CMake and rarely needs manual execution

### Requirement: Build skill documents CMake configuration flags
The skill SHALL list the key CMake boolean flags that control the build: `BCORE_ZIGBEE`, `BCORE_MATTER`, `BCORE_THREAD`, `BCORE_PHILIPS_HUE`, `BCORE_BUILD_REFERENCE`, `BCORE_BUILD_WITH_ASAN`, `BCORE_BUILD_WITH_SSP`, `BCORE_GEN_GIR`, `BCORE_TEST_COVERAGE`, and `BCORE_MATTER_VALIDATE_SCHEMAS`. The skill SHALL explain how to pass flags (via `-D` to cmake or appended to `build.sh`).

#### Scenario: Agent disables a subsystem for build
- **WHEN** the agent needs to build without Zigbee support
- **THEN** the skill SHALL show how to pass `-DBCORE_ZIGBEE=OFF` to the build

### Requirement: Build skill documents dev profile
The skill SHALL note that the default development profile is at `config/cmake/platforms/dev/linux.cmake` and is automatically used by `build.sh`. The skill SHALL list what the dev profile enables (Debug build, ASAN, SSP, GIR, random Matter port).

#### Scenario: Agent understands default build configuration
- **WHEN** the agent runs `./build.sh` without arguments
- **THEN** the skill SHALL explain that it uses the dev profile with Debug, ASAN, and SSP enabled

### Requirement: Build skill includes error recovery pattern
The skill SHALL instruct the agent that if a build command fails with a tool-not-found error (e.g., `cmake: command not found`, `make: command not found`), the agent SHALL check for `/.dockerenv`. If the file does not exist, the agent SHALL inform the user they need to run inside the BartonCore development container.

#### Scenario: Build fails outside Docker
- **WHEN** a build command fails with a missing tool error
- **AND** the file `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container

### Requirement: Debug skill SKILL.md conforms to Agent Skills spec
The `debug` skill SHALL be located at `.github/skills/debug/SKILL.md`. The frontmatter SHALL include `name: debug`, a `description` field explaining the skill covers debugging BartonCore with gdb and pdb, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: debug` matching the directory name
- **AND** the `description` SHALL mention debugging, gdb, pdb, reference app, and integration tests

### Requirement: Debug skill documents gdb for the reference app
The skill SHALL explain how to debug the reference app with gdb. The skill SHALL document the binary location (`build/reference/barton-core-reference`), common command-line flags (`-b` for SBMD specs path, `-z` to disable Zigbee, `-t` to disable Thread, `-m` to disable Matter), and how to start a gdb session. The skill SHALL note that the VS Code launch configurations (`(gdb) Reference App` variants) provide an alternative.

#### Scenario: Agent debugs the reference app with gdb
- **WHEN** the agent needs to debug the reference app
- **THEN** the skill SHALL show how to launch `gdb build/reference/barton-core-reference` and pass arguments
- **AND** the skill SHALL mention available VS Code launch configs as an alternative

### Requirement: Debug skill documents pdb for integration tests
The skill SHALL explain how to debug Python integration tests using pdb. The skill SHALL show how to insert breakpoints (`breakpoint()` or `import pdb; pdb.set_trace()`) and how to run pytest with debugger support (`./testing/py_test.sh testing/test/<test_file>.py -s --no-header`). The skill SHALL note that the VS Code Python debugger can also be used via the Testing panel.

#### Scenario: Agent debugs an integration test with pdb
- **WHEN** the agent needs to debug a Python integration test
- **THEN** the skill SHALL show how to insert a breakpoint and run the test with `-s` flag
- **AND** the skill SHALL mention VS Code Testing panel as an alternative

### Requirement: Debug skill documents gdb with python3-gdb for native visibility
The skill SHALL explain how to use gdb with python3 to debug C/C++ code exercised by Python integration tests. The skill SHALL document the workflow: run `gdb python3`, answer yes to debug info download prompts, set C breakpoints (may require running first for symbol loading), then execute the test via `run -m pytest testing/test/<test_file>.py`. The skill SHALL note that if the build uses ASAN, the agent must run `set env LD_PRELOAD=<path-to-libasan.so>` in gdb before `run`. The skill SHALL note the command to find libasan: `gcc -print-file-name=libasan.so`.

#### Scenario: Agent debugs native code via Python tests
- **WHEN** the agent needs to debug C/C++ code triggered by a Python integration test
- **THEN** the skill SHALL show the gdb+python3 workflow step by step
- **AND** the skill SHALL include the ASAN LD_PRELOAD setup if needed

### Requirement: Debug skill documents ASAN considerations
The skill SHALL explain that the development build enables Address Sanitizer (ASAN) by default. The skill SHALL document that `testing/py_test.sh` handles ASAN preloading automatically for pytest runs, and that for gdb sessions the agent must manually preload libasan using `set env LD_PRELOAD` before running. The skill SHALL note that `ASAN_OPTIONS=verify_asan_link_order=0` is set in the container environment.

#### Scenario: Agent understands ASAN in debugging context
- **WHEN** the agent reads the debug skill
- **THEN** it SHALL understand that ASAN is enabled by default in dev builds
- **AND** it SHALL know how to handle ASAN when debugging

### Requirement: Debug skill includes error recovery pattern
The skill SHALL instruct the agent that if `gdb` is not found, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container. The skill SHALL note that the Docker container has `SYS_PTRACE` capability enabled for gdb to work.

#### Scenario: gdb not found outside Docker
- **WHEN** `gdb` is not found
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container

### Requirement: Format code skill SKILL.md conforms to Agent Skills spec
The `format-code` skill SHALL be located at `.github/skills/format-code/SKILL.md`. The frontmatter SHALL include `name: format-code`, a `description` field explaining the skill covers C/C++ code formatting, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: format-code` matching the directory name
- **AND** the `description` SHALL mention clang-format, formatting, and C/C++

### Requirement: Format code skill documents clang-format usage
The skill SHALL instruct the agent to run `clang-format` from the repository root so it picks up the `.clang-format` configuration. The skill SHALL show the command to format a file: `clang-format -i <file>`.

#### Scenario: Agent formats a file
- **WHEN** the agent needs to format a C/C++ file
- **THEN** the skill SHALL instruct the agent to run `clang-format -i <file>` from the repo root

### Requirement: Format code skill documents the diff-only rule
The skill SHALL state that agents MUST only format code that is part of the current diff. Agents SHALL NOT reformat entire files they did not modify. The pre-commit hook enforces formatting on staged files automatically.

#### Scenario: Agent respects diff-only rule
- **WHEN** the agent has modified a C/C++ file
- **THEN** the agent SHALL only format the file it modified
- **AND** the agent SHALL NOT run clang-format on unmodified files

### Requirement: Format code skill documents manual blank-line rules
The skill SHALL document the blank-line conventions that clang-format cannot enforce: a blank line before `if`, `for`, `while`, `switch`, and `return` statements (unless preceded by an opening brace or another control-flow statement's opening line), and a blank line after a closing brace `}` (unless followed by `else`, `catch`, or another closing brace).

#### Scenario: Agent applies blank-line rules
- **WHEN** the agent writes or modifies C/C++ code
- **THEN** the agent SHALL apply the manual blank-line conventions

### Requirement: Format code skill documents pre-commit hook
The skill SHALL note that a pre-commit hook (`hooks/pre-commit`) automatically runs clang-format on staged files at commit time. The skill SHALL note that hooks are installed via `./hooks/install.sh`.

#### Scenario: Agent understands pre-commit integration
- **WHEN** the agent reads the format code skill
- **THEN** it SHALL know that formatting is enforced automatically at commit time

### Requirement: Format code skill includes error recovery pattern
The skill SHALL instruct the agent that if `clang-format` is not found, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: clang-format not found outside Docker
- **WHEN** `clang-format` is not found
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container

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

### Requirement: Matter virtual devices skill SKILL.md conforms to Agent Skills spec
The `matter-virtual-devices` skill SHALL be located at `.github/skills/matter-virtual-devices/SKILL.md`. The frontmatter SHALL include `name: matter-virtual-devices`, a `description` field explaining the skill covers working with Matter test devices, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: matter-virtual-devices` matching the directory name
- **AND** the `description` SHALL mention Matter, sample apps, chip-tool, virtual devices, and testing

### Requirement: Matter skill documents pre-built sample apps
The skill SHALL list the Matter sample apps available in the Docker container at `/usr/local/bin/`: `chip-lighting-app`, `chip-lock-app`, `thermostat-app`, `contact-sensor-app`. The skill SHALL explain how to start each app and what command-line options they support (e.g., `--discriminator`, `--KVS`). The skill SHALL explain that these are CHIP SDK sample apps compiled from the Matter SDK.

#### Scenario: Agent starts a pre-built light device
- **WHEN** the agent needs a Matter light device for testing
- **THEN** the skill SHALL show how to start `chip-lighting-app` with appropriate arguments

#### Scenario: Agent starts a pre-built lock device
- **WHEN** the agent needs a Matter door lock device for testing
- **THEN** the skill SHALL show how to start `chip-lock-app` with appropriate arguments

### Requirement: Matter skill documents chip-tool
The skill SHALL explain that `chip-tool` is available at `/usr/local/bin/chip-tool` and is used to commission and interact with Matter devices. The skill SHALL show common chip-tool commands: pairing (`chip-tool pairing code <node-id> <pairing-code>`), cluster operations (`chip-tool onoff on <node-id> <endpoint>`), and reading attributes (`chip-tool <cluster> read <attribute> <node-id> <endpoint>`).

#### Scenario: Agent commissions a device with chip-tool
- **WHEN** the agent needs to commission a Matter device
- **THEN** the skill SHALL show the `chip-tool pairing code` command with placeholder node ID and pairing code

#### Scenario: Agent sends a cluster command
- **WHEN** the agent needs to interact with a commissioned device
- **THEN** the skill SHALL show cluster command examples (e.g., `chip-tool onoff on`)

### Requirement: Matter skill documents matter.js virtual devices
The skill SHALL explain the matter.js virtual device framework located at `testing/mocks/devices/matterjs/`. The skill SHALL describe the Python wrapper classes in `testing/mocks/devices/matter/` (`MatterLight`, `MatterDoorLock`) and how they spawn Node.js subprocesses. The skill SHALL explain the sideband API for programmatic control (`device.sideband.send()`, `device.sideband.get_state()`).

#### Scenario: Agent understands matter.js device architecture
- **WHEN** the agent reads the matter virtual devices skill
- **THEN** it SHALL understand that Python classes wrap matter.js Node.js processes
- **AND** it SHALL know about the sideband control interface

### Requirement: Matter skill documents creating custom virtual devices
The skill SHALL explain how to create a new matter.js virtual device: create a JavaScript entry point in `testing/mocks/devices/matterjs/src/`, create a Python wrapper class in `testing/mocks/devices/matter/` extending `MatterDevice`, and specify the `matterjs_entry_point`. The skill SHALL note that custom devices useful for testing should be committed into the test harness with new integration tests, but this decision is made by the human.

#### Scenario: Agent creates a new virtual device type
- **WHEN** the agent needs a device type not already available
- **THEN** the skill SHALL outline the steps to create a new matter.js device and Python wrapper
- **AND** the skill SHALL note that the human decides whether to commit it

### Requirement: Matter skill includes error recovery pattern
The skill SHALL instruct the agent that if sample app binaries or `chip-tool` are not found, or if `node` is not available for matter.js devices, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: chip-tool not found outside Docker
- **WHEN** `chip-tool` is not found in PATH
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container

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
The skill SHALL note that C tests use CMocka and C++ tests use Google Test/Google Mock. The skill SHALL explain that tests are defined in CMakeLists.txt files using `bcore_add_cmocka_test()` and `bcore_add_cpp_test()` macros, located under `core/test/`, `libs/device/descriptors/c/test/`, `libs/device/philipsHue/c/test/` when enabled, and `api/c/test/`.

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

### Requirement: SBMD validation skill SKILL.md conforms to Agent Skills spec
The `validate-sbmd` skill SHALL be located at `.github/skills/validate-sbmd/SKILL.md`. The frontmatter SHALL include `name: validate-sbmd`, a `description` field explaining the skill covers validating SBMD specification files, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: validate-sbmd` matching the directory name
- **AND** the `description` SHALL mention SBMD, validation, schema, and Matter drivers

### Requirement: SBMD validation skill documents spec validation
The skill SHALL explain how to validate SBMD v4 `.sbmd.js` driver files using `scripts/ci/validate_sbmd_specs.py`. The skill SHALL document the command syntax: `python3 scripts/ci/validate_sbmd_specs.py <schema_dir> <sbmd_file.sbmd.js> [<sbmd_file.sbmd.js> ...]`. The skill SHALL note that the validator uses Node.js to evaluate each `.sbmd.js` file, extract its `SbmdDriver()` registration object, and validate that object against the JSON schema (`sbmd-spec-schema.json`), and that this validation also runs automatically during the build via the `validate_sbmd_specs` target when `BCORE_MATTER_VALIDATE_SCHEMAS=ON` (the default).

#### Scenario: Agent validates SBMD specs
- **WHEN** the agent needs to validate SBMD spec files after editing
- **THEN** the skill SHALL show the validation command with the correct schema directory and `.sbmd.js` spec file paths

### Requirement: SBMD validation skill documents spec file locations
The skill SHALL identify that SBMD driver files live at `core/deviceDrivers/matter/sbmd/specs/` as `.sbmd.js` files, the JSON schema is at `core/deviceDrivers/matter/sbmd/schema/sbmd-spec-schema.json`, and the validation script is `scripts/ci/validate_sbmd_specs.py`. The skill SHALL note that validation runs automatically during build when `BCORE_MATTER_VALIDATE_SCHEMAS=ON` (the default).

#### Scenario: Agent finds SBMD specs
- **WHEN** the agent needs to locate SBMD spec files
- **THEN** the skill SHALL direct the agent to `core/deviceDrivers/matter/sbmd/specs/`

### Requirement: SBMD validation skill includes error recovery pattern
The skill SHALL instruct the agent that if `python3` or validation dependencies are not found, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: Validation tools not found outside Docker
- **WHEN** validation scripts fail with missing dependencies
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container
