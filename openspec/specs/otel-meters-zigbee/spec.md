## ADDED Requirements

### Requirement: Zigbee device join counters
The system SHALL maintain counters tracking device join and announce events on the Zigbee network.

#### Scenario: Device join detected
- **WHEN** the Zigbee event handler detects a new device joining the network
- **THEN** a counter `zigbee.device.join.count` SHALL be incremented

#### Scenario: Device announce received
- **WHEN** `deviceAnnounced()` callback fires for a device announcement
- **THEN** a counter `zigbee.device.announce.count` SHALL be incremented

#### Scenario: Device processed after announce
- **WHEN** `processNewDevice()` successfully processes a newly joined and announced device
- **THEN** a counter `zigbee.device.processed.count` SHALL be incremented

### Requirement: Zigbee health check gauge
The system SHALL maintain a gauge tracking Zigbee network health check state.

#### Scenario: Health check enabled gauge
- **WHEN** the Zigbee health check subsystem starts or stops monitoring
- **THEN** a gauge `zigbee.health_check.enabled` SHALL be set to 1 (enabled) or 0 (disabled)

### Requirement: Zigbee interference counters
The system SHALL maintain counters tracking radio interference events.

#### Scenario: Interference detected
- **WHEN** `zigbeeHealthCheckSetProblem()` detects radio interference (CCA failures exceeding threshold)
- **THEN** a counter `zigbee.interference.detected` SHALL be incremented

#### Scenario: Interference resolved
- **WHEN** interference condition clears
- **THEN** a counter `zigbee.interference.resolved` SHALL be incremented

#### Scenario: Interference active gauge
- **WHEN** interference state changes
- **THEN** a gauge `zigbee.interference.active` SHALL be set to 1 (interference present) or 0 (clear)

### Requirement: Zigbee PAN ID attack counters
The system SHALL maintain counters tracking PAN ID spoofing attack detection events.

#### Scenario: PAN ID attack detected
- **WHEN** `zigbeeDefenderSetPanIdAttack()` detects a PAN ID spoofing attack
- **THEN** a counter `zigbee.pan_id_attack.detected` SHALL be incremented

#### Scenario: PAN ID attack active gauge
- **WHEN** PAN ID attack state changes
- **THEN** a gauge `zigbee.pan_id_attack.active` SHALL be set to 1 (attack in progress) or 0 (clear)

### Requirement: Zigbee network startup counter
The system SHALL count Zigbee network initialization completions.

#### Scenario: Network startup completed
- **WHEN** the Zigbee subsystem completes network initialization
- **THEN** a counter `zigbee.network.startup.completed` SHALL be incremented

### Requirement: Zigbee meters initialization
All Zigbee meters SHALL be created during Zigbee subsystem initialization and released during shutdown.

#### Scenario: Meters created at Zigbee init
- **WHEN** the Zigbee subsystem initializes with `BCORE_OBSERVABILITY=ON`
- **THEN** all Zigbee counters and gauges SHALL be created

#### Scenario: Meters conditional on BCORE_ZIGBEE
- **WHEN** `BCORE_ZIGBEE=OFF`
- **THEN** no Zigbee meter code SHALL be compiled
