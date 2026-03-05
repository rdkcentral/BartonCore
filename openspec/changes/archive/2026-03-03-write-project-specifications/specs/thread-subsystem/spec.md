## ADDED Requirements

### Requirement: OTBR D-Bus integration
The Thread subsystem SHALL communicate with the OpenThread Border Router (OTBR) agent via D-Bus (using the `io.openthread.BorderRouter` interface on `DBUS_BUS_SYSTEM`). It SHALL wrap D-Bus access via an `OpenThreadClient` class that provides methods for `CreateNetwork()`, `RestoreNetwork()`, `GetChannel()`, `GetPanId()`, `GetExtPanId()`, `GetNetworkKey()`, `GetNetworkName()`, `GetDeviceRole()`, `SetNat64Enabled()`, and `ActivateEphemeralKeyMode()`. The `DeviceRole` enum SHALL include: UNKNOWN, DISABLED, DETACHED, CHILD, ROUTER, LEADER.

#### Scenario: OTBR connection
- **WHEN** the Thread subsystem initializes with `BCORE_THREAD=ON`
- **THEN** it SHALL connect to the OTBR agent via D-Bus and report readiness to the subsystem manager

### Requirement: Thread network backup and restore
The Thread subsystem SHALL support backing up and restoring Thread network configuration as opaque active dataset TLV blobs. The backup SHALL be stored as a base64-encoded TLV string in the `threadNetworkConfig` system property.

#### Scenario: Thread network backup
- **WHEN** the Thread subsystem persists its state
- **THEN** the Thread network active dataset TLVs SHALL be base64-encoded and saved to the `threadNetworkConfig` system property

#### Scenario: Thread network restore
- **WHEN** the Thread subsystem initializes and the `threadNetworkConfig` system property contains a previously-saved backup
- **THEN** the Thread subsystem SHALL restore the network configuration by calling `RestoreNetwork()` on the `OpenThreadClient` with the decoded TLV data

### Requirement: Thread credentials for Matter commissioning
The Thread subsystem SHALL provide Thread network credentials to the Matter subsystem for commissioning Matter-over-Thread devices. The credentials SHALL include the Thread operational dataset.

#### Scenario: Matter-over-Thread commissioning
- **WHEN** a Matter device requiring Thread network access is being commissioned
- **THEN** the Thread subsystem SHALL provide the active Thread operational dataset to the Matter commissioning orchestrator

### Requirement: NAT64 support
The Thread subsystem SHALL support enabling and disabling NAT64 on the Thread border router via `b_core_client_thread_set_nat64_enabled()`.

#### Scenario: Enable NAT64
- **WHEN** `b_core_client_thread_set_nat64_enabled(TRUE)` is called
- **THEN** the Thread subsystem SHALL enable NAT64 translation on the OTBR and return `TRUE`

#### Scenario: Disable NAT64
- **WHEN** `b_core_client_thread_set_nat64_enabled(FALSE)` is called
- **THEN** the Thread subsystem SHALL disable NAT64 translation on the OTBR and return `TRUE`

### Requirement: Ephemeral key commissioning
The Thread subsystem SHALL support activating ephemeral key mode for Thread 1.4 commissioning via `b_core_client_thread_activate_ephemeral_key_mode()`.

#### Scenario: Activate ephemeral key mode
- **WHEN** `b_core_client_thread_activate_ephemeral_key_mode()` is called
- **THEN** the Thread subsystem SHALL activate ephemeral key mode on the OTBR and return the ephemeral key string

### Requirement: Border router status monitoring
The Thread subsystem SHALL monitor the OTBR border router status and report its availability to the subsystem manager. It SHALL use a periodic monitor task (every 60 seconds) to check device role and emit status events on changes.

#### Scenario: Border router status
- **WHEN** the OTBR border router becomes unavailable
- **THEN** the Thread subsystem SHALL detect the status change and update the subsystem status accordingly

### Requirement: Default Thread network name
The Thread subsystem SHALL support a configurable default Thread network name via the `B_CORE_BARTON_DEFAULT_THREAD_NETWORK_NAME` property key.

#### Scenario: Default network name
- **WHEN** the Thread network is created for the first time
- **THEN** the network name SHALL use the value from the `barton.thread.defaultNetworkName` property if set

### Requirement: Conditional compilation
The Thread subsystem SHALL only be compiled and available when `BCORE_THREAD=ON` in the CMake configuration.

#### Scenario: Thread disabled
- **WHEN** `BCORE_THREAD=OFF` in CMake
- **THEN** all Thread subsystem code SHALL be excluded from the build and Thread-related API calls SHALL be no-ops or return errors
