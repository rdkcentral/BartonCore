## ADDED Requirements

### Requirement: Subsystem initialization counters
The system SHALL maintain counters tracking subsystem initialization lifecycle events.

#### Scenario: Subsystem init started
- **WHEN** `subsystemManagerInitialize()` begins initializing a subsystem
- **THEN** a counter `subsystem.init.started` SHALL be incremented with attribute `subsystem.name`

#### Scenario: Subsystem init completed
- **WHEN** a subsystem reports successful initialization via `onSubsystemInitialized()`
- **THEN** a counter `subsystem.init.completed` SHALL be incremented with attribute `subsystem.name`

#### Scenario: Subsystem init failed
- **WHEN** a subsystem initialization fails
- **THEN** a counter `subsystem.init.failed` SHALL be incremented

> **Note:** The counter is created and released with the other subsystem meters, but no subsystem currently reports a failure through this path. It is available for future use.

### Requirement: Subsystem readiness gauge
The system SHALL track the number of initialized subsystems and the overall readiness state.

#### Scenario: Initialized count updated
- **WHEN** a subsystem completes initialization
- **THEN** a gauge `subsystem.initialized.count` SHALL be updated with the current number of initialized subsystems

#### Scenario: Ready for devices
- **WHEN** all registered subsystems report ready and `readyForDevicesCB()` fires
- **THEN** a gauge `subsystem.ready_for_devices` SHALL be set to 1

#### Scenario: Not yet ready
- **WHEN** subsystem initialization is in progress and not all subsystems have reported ready
- **THEN** a gauge `subsystem.ready_for_devices` SHALL remain at 0

### Requirement: Subsystem initialization duration histogram
The system SHALL record the duration of individual subsystem initializations.

#### Scenario: Init duration recorded
- **WHEN** a subsystem finishes initialization
- **THEN** a histogram `subsystem.init.duration` SHALL record the elapsed time in seconds with attribute `subsystem.name`

### Requirement: Subsystem meters initialization
All subsystem manager meters SHALL be created during `subsystemManagerInitialize()` and released during `subsystemManagerShutdown()`.

#### Scenario: Meters created at subsystem manager init
- **WHEN** `subsystemManagerInitialize()` is called with `BCORE_OBSERVABILITY=ON`
- **THEN** all subsystem manager counters, gauges, and histograms SHALL be created
