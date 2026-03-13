## ADDED Requirements

### Requirement: Device driver initialization counter
The device driver manager SHALL maintain a counter tracking successful driver initializations.

#### Scenario: Driver init succeeded
- **WHEN** a driver's `init()` callback returns success
- **THEN** a counter `driver.init.success` SHALL be incremented with attribute `driver.name`

### Requirement: Active driver counter
The device driver manager SHALL track registered drivers.

#### Scenario: Driver registered
- **WHEN** `deviceDriverManagerRegisterDriver()` adds a driver
- **THEN** a counter `driver.registered.count` SHALL be incremented with attribute `driver.name`

### Requirement: Communication watchdog state gauge
The communication watchdog SHALL maintain a gauge tracking how many devices are currently in comm-fail state.

#### Scenario: Comm-fail count updated
- **WHEN** a device enters or exits comm-fail state
- **THEN** a gauge `device.commfail.current` SHALL be updated with the current number of devices in comm-fail state

### Requirement: Communication watchdog check counter
The communication watchdog SHALL count periodic check cycles.

#### Scenario: Watchdog check performed
- **WHEN** the comm-fail watchdog thread performs a monitoring cycle
- **THEN** a counter `device.communication.check.performed` SHALL be incremented

### Requirement: Storage persist counter
The database layer SHALL count device persist operations.

#### Scenario: Device persisted
- **WHEN** a device's data is written to persistent storage (JSON file)
- **THEN** a counter `storage.device.persist` SHALL be incremented

### Requirement: Storage device count gauge
The database layer SHALL track the total number of devices in persistent storage.

#### Scenario: Database device count updated
- **WHEN** a device is added to or removed from the persistent database
- **THEN** a gauge `storage.device.count` SHALL be updated with the current total

### Requirement: Database restore counters
The system SHALL count database restore operations.

#### Scenario: Restore attempted
- **WHEN** `deviceServiceRestoreConfig()` initiates a configuration restore
- **THEN** a counter `storage.restore.attempt` SHALL be incremented

#### Scenario: Restore succeeded
- **WHEN** a configuration restore completes successfully
- **THEN** a counter `storage.restore.success` SHALL be incremented

#### Scenario: Restore failed
- **WHEN** a configuration restore fails
- **THEN** a counter `storage.restore.failed` SHALL be incremented

### Requirement: Event production counter
The event producer SHALL count events emitted, broken down by event type.

#### Scenario: Event emitted
- **WHEN** the device event producer emits a GObject signal (device-added, device-removed, discovery-started, etc.)
- **THEN** a counter `event.produced` SHALL be incremented with attribute `event.type` set to the event signal name

### Requirement: Internal meters initialization
All internal service meters SHALL be created during their respective module initialization and released during shutdown.

#### Scenario: Driver manager meters lifecycle
- **WHEN** the device driver manager initializes with `BCORE_OBSERVABILITY=ON`
- **THEN** all driver counters and gauges SHALL be created and later released at shutdown

#### Scenario: Watchdog meters lifecycle
- **WHEN** the communication watchdog initializes with `BCORE_OBSERVABILITY=ON`
- **THEN** all new watchdog counters and gauges SHALL be created alongside the existing ones

#### Scenario: Event producer meters lifecycle
- **WHEN** the device event producer starts up with `BCORE_OBSERVABILITY=ON`
- **THEN** the event production counter SHALL be created and later released at shutdown
