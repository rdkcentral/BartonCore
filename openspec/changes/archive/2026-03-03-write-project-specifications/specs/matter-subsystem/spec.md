## ADDED Requirements

### Requirement: Matter SDK initialization
The Matter subsystem SHALL initialize the CHIP SDK, set up the GLib main loop integration, and configure operational parameters. Initialization SHALL be triggered via an `initialize` callback registered with `subsystemManagerRegister()` and SHALL report readiness via `notifySubsystemInitialized()`.

#### Scenario: Matter subsystem startup
- **WHEN** the Matter subsystem initializes with `BCORE_MATTER=ON`
- **THEN** the CHIP SDK SHALL be initialized, the event loop started, and the subsystem SHALL report ready to the subsystem manager

### Requirement: Commissioning orchestrator
The Matter subsystem SHALL provide a `CommissioningOrchestrator` that manages the multi-step Matter commissioning flow including device discovery, PASE session establishment, certificate provisioning, and operational network configuration.

#### Scenario: Commission via setup payload
- **WHEN** `b_core_client_commission_device()` is called with a Matter setup payload (QR code or manual code)
- **THEN** the orchestrator SHALL parse the payload, discover the device, establish a PASE session, provision operational credentials, and configure the operational network

#### Scenario: Commission Matter-over-Thread device
- **WHEN** a Matter device requires Thread network commissioning
- **THEN** the orchestrator SHALL obtain Thread network credentials from the Thread subsystem and provide them during commissioning

#### Scenario: Commission Matter-over-WiFi device
- **WHEN** a Matter device requires Wi-Fi network commissioning
- **THEN** the orchestrator SHALL obtain Wi-Fi credentials from the `BCoreNetworkCredentialsProvider` and provide them during commissioning

### Requirement: Device discovery for Matter
The Matter subsystem SHALL provide a `DeviceDiscoverer` that discovers Matter devices on the network and identifies their device types, endpoints, and cluster compositions.

#### Scenario: Discover Matter device types
- **WHEN** a Matter device is discovered or commissioned
- **THEN** the discoverer SHALL read the device's descriptor cluster to determine all device types and endpoint compositions

### Requirement: Device data cache
The Matter subsystem SHALL maintain a `DeviceDataCache` for each commissioned device, populated via Matter subscription reports. The cache SHALL store the latest attribute values received from subscription notifications.

#### Scenario: Subscription-based cache update
- **WHEN** a Matter device sends a subscription report with updated attribute values
- **THEN** the `DeviceDataCache` SHALL update the cached values and notify the SBMD driver of changes

#### Scenario: Read from cache
- **WHEN** a driver reads a cached attribute
- **THEN** the cache SHALL return the last received value without contacting the device

### Requirement: Persistent storage delegate
The Matter subsystem SHALL implement a `PersistentStorageDelegate` that maps Matter SDK storage operations to Barton's storage system, persisting fabric credentials, session data, and operational state.

#### Scenario: Persist Matter fabric data
- **WHEN** the Matter SDK stores fabric credentials
- **THEN** the persistent storage delegate SHALL write them to the configured `matter-storage-dir`

### Requirement: Access control delegate
The Matter subsystem SHALL implement an `AccessControlDelegate` for Matter ACL enforcement.

#### Scenario: ACL check
- **WHEN** a Matter command is received from a remote node
- **THEN** the access control delegate SHALL verify the request against the configured ACL

### Requirement: OTA provider (optional)
When `BCORE_MATTER_ENABLE_OTA_PROVIDER=ON`, the Matter subsystem SHALL implement an `OTAProviderImpl` that serves firmware updates to Matter devices with the OTA Requestor cluster.

#### Scenario: OTA provider enabled
- **WHEN** `BCORE_MATTER_ENABLE_OTA_PROVIDER=ON` and a device queries for firmware updates
- **THEN** the OTA provider SHALL check for available firmware and serve it if a newer version exists

#### Scenario: OTA provider disabled
- **WHEN** `BCORE_MATTER_ENABLE_OTA_PROVIDER=OFF`
- **THEN** no OTA provider functionality SHALL be compiled or available

### Requirement: Pluggable credential providers
The Matter subsystem SHALL support pluggable operational credentials issuers via the `BCORE_MATTER_DELEGATE_IMPLEMENTATIONS` CMake variable. The default SHALL be `CertifierOperationalCredentialsIssuer` (xPKI-based).

#### Scenario: Default credentials issuer
- **WHEN** no custom delegate is configured
- **THEN** the `CertifierOperationalCredentialsIssuer` SHALL be used for operational certificate provisioning

#### Scenario: Custom credentials issuer
- **WHEN** a client provides a custom delegate via `BCORE_MATTER_DELEGATE_IMPLEMENTATIONS`
- **THEN** that delegate SHALL be used instead of the default

### Requirement: Pluggable commissionable data provider
The Matter subsystem SHALL support pluggable commissionable data providers via `BCORE_MATTER_PROVIDER_IMPLEMENTATIONS`. When `BCORE_MATTER_USE_DEFAULT_COMMISSIONABLE_DATA=ON`, default values SHALL be used. Otherwise, values SHALL come from the property provider using well-known property keys (discriminator, passcode, SPAKE2+ parameters).

#### Scenario: Commissionable data from properties
- **WHEN** `BCORE_MATTER_USE_DEFAULT_COMMISSIONABLE_DATA=OFF`
- **THEN** the commissionable data provider SHALL read discriminator, passcode, and SPAKE2+ params from the `BCorePropertyProvider`

### Requirement: Pluggable device attestation
The Matter subsystem SHALL support pluggable Device Attestation Certificate (DAC) providers. The default SHALL be a test DAC provider. The attestation trust store directory SHALL be configurable via `matter-attestation-trust-store-dir` in init params.

#### Scenario: Test DAC provider
- **WHEN** no custom DAC provider is configured
- **THEN** the test DAC provider SHALL be used (suitable for development only)

### Requirement: Matter driver factory
The Matter subsystem SHALL include a `MatterDriverFactory` singleton (located in `core/deviceDrivers/matter/`) that manages Matter device driver registration and device-to-driver matching. Drivers (both native and SBMD-based) SHALL register via `RegisterDriver()`. Device matching SHALL use `GetDriver(DeviceDataCache*)`.

#### Scenario: SBMD driver registered via factory
- **WHEN** `SbmdFactory` creates a `SpecBasedMatterDeviceDriver`
- **THEN** it SHALL register the driver with `MatterDriverFactory::Instance().RegisterDriver()`

#### Scenario: Device type matching
- **WHEN** a Matter device is commissioned with device type 0x0100
- **THEN** `MatterDriverFactory::GetDriver()` SHALL return the SBMD driver whose spec includes 0x0100

### Requirement: MatterDeviceDriver base class
The system SHALL provide a `MatterDeviceDriver` base class (located in `core/deviceDrivers/matter/`) that bridges the synchronous C `DeviceDriver` interface with the asynchronous Matter SDK. It SHALL provide `RunOnMatterSync(work)` for scheduling work on the Matter event loop and blocking until completion. It SHALL manage `MatterDevice` instances per commissioned device.

#### Scenario: Synchronous Matter operation
- **WHEN** a driver needs to execute a Matter SDK operation from a non-Matter thread
- **THEN** `RunOnMatterSync()` SHALL schedule the work on the Matter event loop and block until completion

#### Scenario: Matter operation timeout
- **WHEN** a Matter operation does not complete within 15 seconds (normal) or 90 seconds (synchronize)
- **THEN** the operation SHALL time out and return an error

### Requirement: MatterDevice per-device state
Each commissioned Matter device SHALL be represented by a `MatterDevice` object (located in `core/deviceDrivers/matter/`) holding: device data cache (shared_ptr), optional `SbmdScript`, resource-to-attribute bindings, write/command response handling (`WriteClient::Callback` and `CommandSender::ExtendableCallback`), and feature cluster management.

#### Scenario: Resource binding
- **WHEN** an SBMD driver registers resources for a device
- **THEN** each resource's URI SHALL be bound to its read mapper via `BindResourceReadInfo()`, write mapper via `BindWriteInfo()`, execute mapper via `BindExecuteInfo()`, and optionally event mappers via `BindResourceEventInfo()`

### Requirement: Matter cluster abstractions
The Matter subsystem SHALL provide C++ cluster abstractions for: `BasicInformation` (device identity), `GeneralDiagnostics` (device health), `PowerSource` (battery/mains), `WifiNetworkDiagnostics` (Wi-Fi stats), `OTARequestor` (firmware updates). Each SHALL inherit from `MatterCluster`.

#### Scenario: Read basic information
- **WHEN** a Matter device is commissioned
- **THEN** the `BasicInformation` cluster abstraction SHALL read vendor name, product name, serial number, firmware version, and hardware version

### Requirement: Commissioning window management
The Matter subsystem SHALL support opening an Enhanced Commissioning Window on commissioned devices via `b_core_client_open_commissioning_window()`, returning manual pairing code and QR code strings.

#### Scenario: Open commissioning window
- **WHEN** `b_core_client_open_commissioning_window()` is called with a device ID and timeout
- **THEN** the Matter subsystem SHALL open an Enhanced Commissioning Window and return `BCoreCommissioningInfo` with manual code and QR code
