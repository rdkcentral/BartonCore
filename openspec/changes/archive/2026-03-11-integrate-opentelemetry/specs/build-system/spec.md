## MODIFIED Requirements

### Requirement: Feature flag catalog
The build system SHALL support the following ON/OFF flags with specified defaults (17 public + 3 private = 20 total):

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
| `BCORE_MATTER_USE_MATTERJS` | ON | `BARTON_CONFIG_MATTER_USE_MATTERJS` |
| `BCORE_BUILD_THIRD_PARTY_BARTON_COMMON` | ON | `BARTON_CONFIG_BUILD_THIRD_PARTY_BARTON_COMMON` |
| `BCORE_OBSERVABILITY` | OFF | `BARTON_CONFIG_OBSERVABILITY` |

**Private flags (internal use):**

| Flag | Default | Definition |
|------|---------|------------|
| `BCORE_SUPPORT_ALARMS` | OFF | `BARTON_CONFIG_SUPPORT_ALARMS` |
| `BCORE_M1LTE` | OFF | `BARTON_CONFIG_M1LTE` |
| `BCORE_SUPPORT_ZIGBEE_TELEMETRY` | OFF | `BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY` |

#### Scenario: OpenTelemetry flag off (default)
- **WHEN** `BCORE_OBSERVABILITY=OFF` (default)
- **THEN** `BARTON_CONFIG_OBSERVABILITY` SHALL NOT be defined, no OpenTelemetry code SHALL be compiled, and no opentelemetry-cpp dependency SHALL be required

#### Scenario: OpenTelemetry flag on
- **WHEN** `BCORE_OBSERVABILITY=ON`
- **THEN** `BARTON_CONFIG_OBSERVABILITY=1` SHALL be defined, `core/src/observability/*.cpp` SHALL be compiled, and opentelemetry-cpp SHALL be fetched and linked

#### Scenario: Flag off excludes definition
- **WHEN** any `BCORE_*` flag is set to OFF
- **THEN** its corresponding `BARTON_CONFIG_*` definition SHALL NOT be set

## ADDED Requirements

### Requirement: OpenTelemetry C++ SDK dependency
When `BCORE_OBSERVABILITY=ON`, the build system SHALL fetch opentelemetry-cpp via `FetchContent_Declare` pinned to a specific release tag. Only the SDK core, OTLP HTTP exporter, and Logs SDK components SHALL be built. The dependency SHALL use libcurl (already required) as the HTTP transport.

#### Scenario: FetchContent retrieval
- **WHEN** CMake configures with `BCORE_OBSERVABILITY=ON`
- **THEN** opentelemetry-cpp SHALL be fetched from its GitHub repository at the pinned tag

#### Scenario: Minimal component build
- **WHEN** opentelemetry-cpp is configured
- **THEN** only `opentelemetry_api`, `opentelemetry_sdk`, `opentelemetry_exporter_otlp_http`, and `opentelemetry_logs` targets SHALL be built (no gRPC, no Prometheus, no Jaeger exporters)

#### Scenario: Dependency not fetched when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF`
- **THEN** no opentelemetry-cpp source SHALL be downloaded or compiled

### Requirement: Observability sources conditional compilation
When `BCORE_OBSERVABILITY=ON`, the build system SHALL compile C++ source files from `core/src/observability/` and link them into the `bCoreObject` target. The observability headers SHALL be available to all core source files.

#### Scenario: Sources included
- **WHEN** `BCORE_OBSERVABILITY=ON`
- **THEN** `core/src/observability/*.cpp` SHALL be added to the `SOURCES` list for `bCoreObject`

#### Scenario: Sources excluded
- **WHEN** `BCORE_OBSERVABILITY=OFF`
- **THEN** no files from `core/src/observability/` SHALL be compiled

#### Scenario: Headers accessible
- **WHEN** `BCORE_OBSERVABILITY=ON`
- **THEN** `core/src/observability/` SHALL be in the private include path so that any core `.c` file can `#include "observability/observabilityTracing.h"`
