## ADDED Requirements

### Requirement: CMake project configuration
The build system SHALL use CMake (minimum 3.16.5) with the project name `barton-core`. It SHALL use C99 and C++17 standards, both set as required. Position-independent code SHALL be enabled globally.

#### Scenario: Minimum CMake version
- **WHEN** CMake version is below 3.16.5
- **THEN** the build SHALL fail with an error

#### Scenario: Language standards
- **WHEN** the project is configured
- **THEN** `CMAKE_CXX_STANDARD` SHALL be 17, `CMAKE_C_STANDARD` SHALL be 99, and both SHALL be required

### Requirement: Build targets
The build system SHALL produce three library targets: `BartonCore` (shared library with SOVERSION), `BartonCoreStatic` (static library), and `bCoreObject` (object library compiled once, reused by both). A reference application target `barton-core-reference` SHALL be built when `BCORE_BUILD_REFERENCE=ON`.

#### Scenario: Shared library versioning
- **WHEN** the shared library is built
- **THEN** its CMake `SOVERSION` property SHALL be the major version number only (e.g., `1`), and its CMake `VERSION` property SHALL be the full major.minor.patch (e.g., `1.5.3`)

#### Scenario: Static library
- **WHEN** the project is built
- **THEN** `BartonCoreStatic` SHALL be available for static linking with the same object files

### Requirement: Modular feature flags
The build system SHALL support ON/OFF feature flags via the `bcore_option()` CMake function. Each flag SHALL map to a compile definition. Features SHALL be independently toggleable.

#### Scenario: Zigbee support flag
- **WHEN** `BCORE_ZIGBEE=ON` (default)
- **THEN** `BARTON_CONFIG_ZIGBEE=1` SHALL be defined and Zigbee code SHALL be compiled

#### Scenario: Zigbee disabled
- **WHEN** `BCORE_ZIGBEE=OFF`
- **THEN** `BARTON_CONFIG_ZIGBEE` SHALL NOT be defined and all Zigbee code SHALL be excluded

#### Scenario: Matter support flag
- **WHEN** `BCORE_MATTER=ON` (default)
- **THEN** `BARTON_CONFIG_MATTER=1` SHALL be defined and Matter code SHALL be compiled

#### Scenario: Thread support flag
- **WHEN** `BCORE_THREAD=ON` (default)
- **THEN** `BARTON_CONFIG_THREAD=1` SHALL be defined and Thread code SHALL be compiled

### Requirement: Feature flag catalog
The build system SHALL support the following ON/OFF flags with specified defaults (16 public + 3 private = 19 total):

**Public flags:**

| Flag | Default | Definition |
|------|---------|------------|
| `BCORE_ZIGBEE` | ON | `BARTON_CONFIG_ZIGBEE` |
| `BCORE_THREAD` | ON | `BARTON_CONFIG_THREAD` |
| `BCORE_MATTER` | ON | `BARTON_CONFIG_MATTER` |
| `BCORE_PHILIPS_HUE` | OFF | `BARTON_CONFIG_PHILIPS_HUE` |
| `BCORE_GEN_GIR` | OFF | `BARTON_CONFIG_GEN_GIR` |
| `BCORE_GENERATE_DEFAULT_LABELS` | OFF | `BARTON_CONFIG_GENERATE_DEFAULT_LABELS` |
| `BCORE_MATTER_USE_RANDOM_PORT` | OFF | `BARTON_CONFIG_MATTER_USE_RANDOM_PORT` |
| `BCORE_BUILD_REFERENCE` | ON | `BARTON_CONFIG_BUILD_REFERENCE` |
| `BCORE_BUILD_WITH_SSP` | OFF | `BARTON_CONFIG_BUILD_WITH_SSP` |
| `BCORE_BUILD_WITH_ASAN` | OFF | `BARTON_CONFIG_BUILD_WITH_ASAN` |
| `BCORE_TEST_COVERAGE` | OFF | `BARTON_CONFIG_TEST_COVERAGE` |
| `BCORE_MATTER_SKIP_SDK` | OFF | `BARTON_CONFIG_MATTER_SKIP_SDK` |
| `BCORE_MATTER_USE_DEFAULT_COMMISSIONABLE_DATA` | OFF | `BARTON_CONFIG_USE_DEFAULT_COMMISSIONABLE_DATA` |
| `BCORE_MATTER_ENABLE_OTA_PROVIDER` | OFF | `BARTON_CONFIG_MATTER_ENABLE_OTA_PROVIDER` |
| `BCORE_MATTER_USE_MATTERJS` | OFF | `BARTON_CONFIG_MATTER_USE_MATTERJS` |
| `BCORE_MATTER_VALIDATE_SCHEMAS` | ON | `BARTON_CONFIG_MATTER_VALIDATE_SCHEMAS` |
| `BCORE_BUILD_THIRD_PARTY_BARTON_COMMON` | ON | `BARTON_CONFIG_BUILD_THIRD_PARTY_BARTON_COMMON` |

**Private flags (internal use):**

| Flag | Default | Definition |
|------|---------|------------|
| `BCORE_SUPPORT_ALARMS` | OFF | `BARTON_CONFIG_SUPPORT_ALARMS` |
| `BCORE_M1LTE` | OFF | `BARTON_CONFIG_M1LTE` |
| `BCORE_SUPPORT_ZIGBEE_TELEMETRY` | OFF | `BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY` |

#### Scenario: Flag off excludes definition
- **WHEN** any `BCORE_*` flag is set to OFF
- **THEN** its corresponding `BARTON_CONFIG_*` definition SHALL NOT be set

### Requirement: String configuration options
The build system SHALL support string configuration via `bcore_string_option()` for: `BCORE_MATTER_LIB` (default: `"BartonMatter"`), `BCORE_MATTER_PROVIDER_IMPLEMENTATIONS`, `BCORE_MATTER_DELEGATE_IMPLEMENTATIONS`, `BCORE_MATTER_BLE_CONTROLLER_DEVICE_NAME` (default: `"Matter-Controller"`), `BCORE_MATTER_SBMD_JS_ENGINE` (default: `"mquickjs"`, valid values: `"quickjs"` or `"mquickjs"`), `BCORE_MATTER_SBMD_SPECS_DIR` (semicolon-delimited list of directories), `BCORE_LINK_LIBRARIES`.

#### Scenario: Custom Matter library name
- **WHEN** `BCORE_MATTER_LIB=CustomMatter` is set
- **THEN** the build SHALL link against `CustomMatter` instead of `BartonMatter`

#### Scenario: Custom provider implementations
- **WHEN** `BCORE_MATTER_PROVIDER_IMPLEMENTATIONS` is set to custom source paths
- **THEN** those sources SHALL be compiled and linked instead of the default providers

### Requirement: Integer configuration options
The build system SHALL support integer configuration via `bcore_int_option()` for: `BCORE_ZIGBEE_STARTUP_TIMEOUT_SECONDS` (default: 120), `BCORE_MQUICKJS_MEMSIZE_BYTES` (default: 2097152, size of the pre-allocated memory buffer for the mquickjs JavaScript engine).

#### Scenario: Custom Zigbee timeout
- **WHEN** `BCORE_ZIGBEE_STARTUP_TIMEOUT_SECONDS=60` is set
- **THEN** `BARTON_CONFIG_ZIGBEE_STARTUP_TIMEOUT_SECONDS=60` SHALL be defined

### Requirement: Dependency version constraints
The build system SHALL enforce minimum versions for required dependencies: GLib/GIO (2.62.4), CMocka (1.1.5, test only), GoogleTest (1.14.0, test only), libcurl (7.82.0), OpenSSL (1.1.1l–1.1.1v), libuuid (1.0.3), libxml2 (2.9.8), mbedcrypto (2.28.4), jsoncpp (1.9.5, when BCORE_MATTER=ON).

#### Scenario: GLib version check
- **WHEN** CMake configures with GLib 2.60.0
- **THEN** configuration SHALL fail because the minimum required version is 2.62.4

#### Scenario: OpenSSL version range
- **WHEN** OpenSSL 3.0.0 is detected
- **THEN** configuration SHALL fail because it exceeds the maximum version 1.1.1v

### Requirement: Stack protection and sanitizer support
The build system SHALL support optional stack smash protection (`BCORE_BUILD_WITH_SSP`) adding `-fstack-protector-strong`, and AddressSanitizer (`BCORE_BUILD_WITH_ASAN`) adding `-fsanitize=address` to both compiler and linker flags.

#### Scenario: Enable ASAN
- **WHEN** `BCORE_BUILD_WITH_ASAN=ON`
- **THEN** `-fsanitize=address` SHALL be added to C flags, C++ flags, shared linker flags, and EXE linker flags

### Requirement: Test coverage support
The build system SHALL support code coverage via LCOV when `BCORE_TEST_COVERAGE=ON`. Coverage SHALL exclude system includes, test files, and Matter SDK install files. A `runTestsWithCoverage` target SHALL be created wrapping the `runTests` target.

#### Scenario: Coverage enabled
- **WHEN** `BCORE_TEST_COVERAGE=ON`
- **THEN** coverage compiler flags SHALL be appended and the `runTestsWithCoverage` target SHALL be available

### Requirement: CTest integration
The build system SHALL integrate with CTest for test execution. A custom `runTests` target SHALL execute `ctest --output-on-failure` in the build directory. Unit tests SHALL use CMocka (C) and Google Test (C++).

#### Scenario: Run tests target
- **WHEN** `make runTests` is invoked
- **THEN** CTest SHALL execute all registered tests with output on failure

### Requirement: GObject Introspection generation
When `BCORE_GEN_GIR=ON`, the build system SHALL generate GIR and typelib files under the `BCore` namespace for all public GObject types, enabling language bindings.

#### Scenario: GIR generation
- **WHEN** `BCORE_GEN_GIR=ON`
- **THEN** `g-ir-scanner` and `g-ir-compiler` SHALL produce `BCore-<version>.gir` and `BCore-<version>.typelib`

### Requirement: Docker build support
The project SHALL include Docker configuration for containerized builds. `dockerw` SHALL serve as a wrapper script for running commands inside the Docker container. The Dockerfile SHALL record all system dependencies.

#### Scenario: Docker build
- **WHEN** `./dockerw ./build.sh` is executed
- **THEN** the project SHALL build inside the Docker container with all dependencies available

### Requirement: Removed option detection
The build system SHALL detect and error on removed/replaced CMake options to prevent stale configurations from silently changing behavior.

#### Scenario: Removed option set
- **WHEN** a removed CMake option is set in the cache
- **THEN** configuration SHALL fail with a `FATAL_ERROR` explaining the option was removed

### Requirement: Version derivation from Git
The build system SHALL derive the project version from `git describe --tags` matching semver tags. The SO version SHALL use major.minor.patch, and the API version SHALL use major.0.

#### Scenario: Version from tag
- **WHEN** the latest tag is `1.5.3`
- **THEN** `BARTON_CORE_VERSION` SHALL start with `1.5.3`, `BARTON_CORE_SO_VERSION` SHALL be `1.5.3`, and `BARTON_CORE_API_VERSION` SHALL be `1.0`
