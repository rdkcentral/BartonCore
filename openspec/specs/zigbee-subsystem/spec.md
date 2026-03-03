## ADDED Requirements

### Requirement: ZHAL abstraction layer
The system SHALL define a Zigbee Hardware Abstraction Layer (ZHAL) as a C API that abstracts the underlying Zigbee radio and stack. ZHAL SHALL support device lifecycle callbacks, attribute report reception, cluster command reception, and network management operations.

#### Scenario: ZHAL callback registration
- **WHEN** the Zigbee subsystem initializes
- **THEN** it SHALL register a `zhalCallbacks` struct with function pointers for all device and network events

### Requirement: ZHAL device lifecycle callbacks
ZHAL SHALL provide callbacks for: `deviceAnnounced`, `deviceJoined`, `deviceLeft`, `deviceRejoined`. Each callback SHALL provide the device's EUI64 identifier.

#### Scenario: Device joined
- **WHEN** a Zigbee device joins the network
- **THEN** the `deviceJoined` callback SHALL be invoked with the device's EUI64

#### Scenario: Device left
- **WHEN** a Zigbee device leaves the network
- **THEN** the `deviceLeft` callback SHALL be invoked with the device's EUI64

### Requirement: ZHAL message reception
ZHAL SHALL provide callbacks for `attributeReportReceived` and `clusterCommandReceived`. Attribute reports (`ReceivedAttributeReport`) SHALL include EUI64, source endpoint, cluster ID, attribute data, RSSI, LQI, and manufacturer ID (`mfgId`). Cluster commands (`ReceivedClusterCommand`) SHALL include EUI64, source endpoint, cluster ID, command ID, command data, profile ID, `fromServer` flag, `mfgSpecific` flag, `mfgCode`, `seqNum`, `apsSeqNum`, RSSI, and LQI.

#### Scenario: Attribute report received
- **WHEN** a Zigbee device sends an attribute report
- **THEN** the `attributeReportReceived` callback SHALL be invoked with a `ReceivedAttributeReport` containing EUI64, endpoint, cluster ID, attribute data, RSSI, LQI, and mfgId

#### Scenario: Cluster command received
- **WHEN** a Zigbee device sends a cluster command
- **THEN** the `clusterCommandReceived` callback SHALL be invoked with a `ReceivedClusterCommand` containing the command details including EUI64, endpoint, cluster ID, command ID, data, profile ID, fromServer, mfgSpecific, mfgCode, seqNum, apsSeqNum, RSSI, and LQI

### Requirement: ZHAL communication tracking
ZHAL SHALL provide callbacks for `deviceCommunicationSucceeded` and `deviceCommunicationFailed` to track per-device communication health.

#### Scenario: Communication success
- **WHEN** a message to/from a Zigbee device succeeds
- **THEN** the `deviceCommunicationSucceeded` callback SHALL be invoked, resetting the communication watchdog timer

### Requirement: ZHAL network management
ZHAL SHALL provide `zhalSystemStatus` containing: network state (up/open), EUI64, original EUI64, channel, PAN ID, network key, and firmware version. ZHAL SHALL provide callbacks for `networkConfigChanged`, `networkHealthProblem`, and `networkHealthProblemRestored`.

#### Scenario: Network status query
- **WHEN** the Zigbee subsystem queries network status
- **THEN** ZHAL SHALL return a `zhalSystemStatus` with the current network parameters

### Requirement: ZHAL security events
ZHAL SHALL provide callbacks for `panIdAttackDetected` and `panIdAttackCleared` for detecting Zigbee PAN ID conflict/attack scenarios.

#### Scenario: PAN ID attack detected
- **WHEN** a PAN ID conflict is detected on the Zigbee network
- **THEN** the `panIdAttackDetected` callback SHALL be invoked and a `zigbee-pan-id-attack-changed` signal SHALL be emitted

### Requirement: ZHAL OTA support
ZHAL SHALL provide callbacks for `deviceOtaUpgradeMessageSent` and `deviceOtaUpgradeMessageReceived` for tracking over-the-air firmware upgrade progress.

#### Scenario: OTA message tracking
- **WHEN** a Zigbee device requests a firmware image block
- **THEN** the OTA callbacks SHALL track the progress of the firmware transfer

### Requirement: ZHAL attribute reporting configuration
ZHAL SHALL support configuring attribute reporting on Zigbee devices with min/max intervals and reportable change thresholds.

#### Scenario: Configure attribute reporting
- **WHEN** a Zigbee driver sets up attribute reporting for a device
- **THEN** ZHAL SHALL configure the device's reporting interval and threshold parameters

### Requirement: ZHAL status codes
ZHAL operations SHALL return status codes: `ZHAL_STATUS_OK` (0), `ZHAL_STATUS_FAIL` (-1), `ZHAL_STATUS_INVALID_ARG` (-2), `ZHAL_STATUS_NOT_IMPLEMENTED` (-3), `ZHAL_STATUS_TIMEOUT` (-4), `ZHAL_STATUS_OUT_OF_MEMORY` (-5), `ZHAL_STATUS_MESSAGE_DELIVERY_FAILED` (-6), `ZHAL_STATUS_NETWORK_BUSY` (-7), `ZHAL_STATUS_NOT_READY` (-8), `ZHAL_STATUS_LPM` (-9).

#### Scenario: Operation timeout
- **WHEN** a ZHAL operation does not complete within the expected time
- **THEN** it SHALL return `ZHAL_STATUS_TIMEOUT`

### Requirement: Zigbee network initialization
The Zigbee subsystem SHALL initialize the Zigbee network via ZHAL, configure the network parameters, and report readiness to the subsystem manager. Network startup SHALL have a configurable timeout (default: 120 seconds via `BCORE_ZIGBEE_STARTUP_TIMEOUT_SECONDS`).

#### Scenario: Successful network startup
- **WHEN** the Zigbee subsystem initializes and the network comes up within the timeout
- **THEN** the subsystem SHALL report ready to the subsystem manager

#### Scenario: Network startup timeout
- **WHEN** the Zigbee network does not start within the configured timeout
- **THEN** the subsystem SHALL report a failure

### Requirement: Zigbee network backup and restore
The Zigbee subsystem SHALL support backing up and restoring the Zigbee network configuration (network key, channel, PAN ID, EUI64) for RMA and migration scenarios.

#### Scenario: Network backup
- **WHEN** the Zigbee subsystem persists its state
- **THEN** the `ZIGBEE_NETWORK_CONFIG_DATA` system property SHALL contain the encoded network configuration

#### Scenario: Network restore
- **WHEN** `config_restore()` is called with a backup containing Zigbee network data
- **THEN** the Zigbee subsystem SHALL restore the network configuration from the backup

### Requirement: Device Descriptor List (DDL) processing
The Zigbee subsystem SHALL support an allow list of approved devices via an XML Device Descriptor List. The DDL SHALL contain device descriptors with manufacturer, model, hardware version, firmware version, and latest firmware information. Devices not matching any descriptor SHALL be rejected unless the driver has `neverReject=true`.

#### Scenario: Device matches descriptor
- **WHEN** a Zigbee device joins and its manufacturer/model/hardware match a descriptor in the DDL
- **THEN** the device SHALL be accepted for pairing

#### Scenario: Device not in DDL
- **WHEN** a Zigbee device joins and no descriptor matches its attributes
- **THEN** the device SHALL be rejected and told to leave the network (when `ZIGBEE_REJECT_UNKNOWN_DEVICES` is `"true"`)

#### Scenario: DDL bypass
- **WHEN** system property `deviceDescriptorBypass` is `"true"`
- **THEN** the DDL check SHALL be skipped and all devices SHALL be accepted

### Requirement: Zigbee OTA firmware management
The Zigbee subsystem SHALL support OTA firmware updates driven by DDL entries. When a DDL entry indicates a newer firmware version than what a paired device has, the firmware image SHALL be downloaded and offered to the device.

#### Scenario: Firmware update available
- **WHEN** the DDL is updated and a paired device's firmware version is older than the DDL's `latestFirmware.version`
- **THEN** the firmware file SHALL be downloaded and an Image Notify command SHALL be sent to the device

#### Scenario: Firmware pull mechanism
- **WHEN** a Zigbee device sends a "query next image" request
- **THEN** the stack SHALL respond indicating a newer image is available, and the device SHALL pull firmware blocks

### Requirement: Zigbee network monitoring
The Zigbee subsystem SHALL monitor network health including RSSI/LQI tracking per device, interference detection, and network statistics collection.

#### Scenario: Interference detected
- **WHEN** Zigbee network interference is detected
- **THEN** a `zigbee-interference` signal SHALL be emitted via the BCoreClient

### Requirement: Zigbee energy scan
The Zigbee subsystem SHALL support energy scanning across channels to assess RF interference levels.

#### Scenario: Perform energy scan
- **WHEN** `b_core_client_zigbee_energy_scan()` is called with a list of channels
- **THEN** the subsystem SHALL scan each channel and return RSSI statistics (min, max, average) and a quality score

### Requirement: Zigbee channel change
The Zigbee subsystem SHALL support changing the Zigbee operating channel, including a dry-run mode that calculates the optimal channel without performing the change.

#### Scenario: Channel change
- **WHEN** `b_core_client_change_zigbee_channel()` is called with `dryRun=FALSE`
- **THEN** the Zigbee network SHALL migrate to the new channel and a `zigbee-channel-changed` signal SHALL be emitted

#### Scenario: Channel change errors
- **WHEN** a channel change is attempted while another is in progress
- **THEN** the operation SHALL fail with `BCoreZigbeeChannelChangeError` `IN_PROGRESS`
