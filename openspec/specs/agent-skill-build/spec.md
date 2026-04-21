## ADDED Requirements

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
