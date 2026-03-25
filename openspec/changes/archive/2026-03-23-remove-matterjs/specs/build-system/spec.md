## REMOVED Requirements

### Requirement: matter.js build support
**Reason**: The `BCORE_MATTER_USE_MATTERJS` CMake option and associated build infrastructure (BCoreMatterJsClusters.cmake module, npm/Node.js dependency, matter.js clone/bundle pipeline, MatterJsClustersEmbedded.h generation) are removed. The SbmdUtils built-in library covers all TLV encoding needs without the external toolchain.
**Migration**: Remove `-DBCORE_MATTER_USE_MATTERJS=ON` from any build invocations. Node.js is no longer required in the build environment.

## MODIFIED Requirements

### Requirement: Feature flag catalog
The build system SHALL support the following ON/OFF flags with specified defaults (15 public + 3 private = 18 total):

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
| `BCORE_MATTER_VALIDATE_SCHEMAS` | ON | `BARTON_CONFIG_MATTER_VALIDATE_SCHEMAS` |
| `BCORE_BUILD_THIRD_PARTY_BARTON_COMMON` | ON | `BARTON_CONFIG_BUILD_THIRD_PARTY_BARTON_COMMON` |

**Private flags (internal use):**

| Flag | Default | Definition |
|------|---------|------------|
| `BCORE_SUPPORT_ALARMS` | OFF | `BARTON_CONFIG_SUPPORT_ALARMS` |
| `BCORE_M1LTE` | OFF | `BARTON_CONFIG_M1LTE` |
| `BCORE_SUPPORT_ZIGBEE_TELEMETRY` | OFF | `BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY` |

Note: `BCORE_MATTER_USE_MATTERJS` has been removed from this catalog.

#### Scenario: Flag off excludes definition
- **WHEN** any `BCORE_*` flag is set to OFF
- **THEN** its corresponding `BARTON_CONFIG_*` definition SHALL NOT be set

#### Scenario: Removed matterjs flag rejected
- **WHEN** `BCORE_MATTER_USE_MATTERJS` is set in the CMake cache
- **THEN** configuration SHALL fail with a `FATAL_ERROR` indicating the option has been removed
