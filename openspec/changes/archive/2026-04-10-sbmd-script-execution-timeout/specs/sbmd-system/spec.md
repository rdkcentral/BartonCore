## MODIFIED Requirements

### Requirement: mquickjs memory configuration
When using the mquickjs engine, the system SHALL support configuring the pre-allocated memory buffer size via the `BCORE_MQUICKJS_MEMSIZE_BYTES` CMake integer option (default: 1048576 bytes = 1 MB). The mquickjs engine uses a fixed-size, non-growing memory buffer. Additionally, the system SHALL support `BCORE_SBMD_SCRIPT_TIMEOUT_MS` (default: 5000) for script execution timeout.

#### Scenario: Custom mquickjs memory size
- **WHEN** `BCORE_MQUICKJS_MEMSIZE_BYTES=4194304` is set
- **THEN** the mquickjs engine SHALL allocate a 4 MB memory buffer

#### Scenario: Script timeout configuration
- **WHEN** `BCORE_SBMD_SCRIPT_TIMEOUT_MS=10000` is set
- **THEN** the mquickjs engine SHALL allow scripts up to 10 seconds of execution time before interrupting
