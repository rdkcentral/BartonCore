## ADDED Requirements

### Requirement: Matter commissioning counters
The system SHALL maintain counters tracking each stage of the Matter commissioning pipeline.

#### Scenario: Commissioning attempt
- **WHEN** `CommissioningOrchestrator::Commission()` is called
- **THEN** a counter `matter.commissioning.attempt` SHALL be incremented

#### Scenario: Commissioning success
- **WHEN** the commissioning pipeline completes successfully
- **THEN** a counter `matter.commissioning.success` SHALL be incremented

#### Scenario: Commissioning failure
- **WHEN** the commissioning pipeline fails at any stage
- **THEN** a counter `matter.commissioning.failed` SHALL be incremented

### Requirement: Matter commissioning duration histogram
The system SHALL record the total duration of commissioning operations.

#### Scenario: Commissioning duration
- **WHEN** a commissioning operation completes (success or failure)
- **THEN** a histogram `matter.commissioning.duration` SHALL record the elapsed time in seconds

### Requirement: Matter pairing counters
The system SHALL maintain counters tracking device pairing operations.

#### Scenario: Pairing attempt
- **WHEN** `CommissioningOrchestrator::Pair()` is called for an existing device node ID
- **THEN** a counter `matter.pairing.attempt` SHALL be incremented

#### Scenario: Pairing success
- **WHEN** device pairing completes successfully
- **THEN** a counter `matter.pairing.success` SHALL be incremented

#### Scenario: Pairing failure
- **WHEN** device pairing fails
- **THEN** a counter `matter.pairing.failed` SHALL be incremented

### Requirement: Matter OTA counters
The system SHALL maintain counters tracking OTA (Over-The-Air) update request flow.

#### Scenario: OTA query image request
- **WHEN** `OTAProviderImpl::HandleQueryImage()` receives a query from a device
- **THEN** a counter `matter.ota.query_image.received` SHALL be incremented

#### Scenario: OTA update available
- **WHEN** an OTA image is available and the query responds with `UpdateAvailable`
- **THEN** a counter `matter.ota.query_image.available` SHALL be incremented

#### Scenario: OTA update not available
- **WHEN** no OTA image is available and the query responds with `NotAvailable`
- **THEN** a counter `matter.ota.query_image.not_available` SHALL be incremented

#### Scenario: OTA apply update requested
- **WHEN** `OTAProviderImpl::HandleApplyUpdateRequest()` receives a request
- **THEN** a counter `matter.ota.apply_update.received` SHALL be incremented

### Requirement: Matter subsystem initialization gauge
The system SHALL track whether the Matter subsystem is currently initializing.

#### Scenario: Matter init started
- **WHEN** `maybeInitMatter()` begins the initialization sequence
- **THEN** a gauge `matter.initializing` SHALL be set to 1

#### Scenario: Matter init completed
- **WHEN** Matter subsystem initialization finishes (success or failure)
- **THEN** a gauge `matter.initializing` SHALL be set to 0

### Requirement: Matter meters initialization
All Matter subsystem meters SHALL be created during Matter subsystem initialization and released during Matter subsystem shutdown.

#### Scenario: Meters created at Matter init
- **WHEN** the Matter subsystem initializes with `BCORE_OBSERVABILITY=ON`
- **THEN** all Matter counters, gauges, and histograms SHALL be created

#### Scenario: Meters conditional on BCORE_MATTER
- **WHEN** `BCORE_MATTER=OFF`
- **THEN** no Matter meter code SHALL be compiled
