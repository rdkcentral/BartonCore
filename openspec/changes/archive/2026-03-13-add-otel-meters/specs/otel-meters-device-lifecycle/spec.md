## ADDED Requirements

### Requirement: Device discovery counters
The system SHALL maintain counters tracking device discovery operations, broken down by device class and subsystem.

#### Scenario: Discovery started
- **WHEN** `deviceServiceDiscoverStart()` initiates a discovery scan
- **THEN** a counter `device.discovery.started` SHALL be incremented

#### Scenario: Discovery completed
- **WHEN** a discovery scan completes (timeout or explicit stop)
- **THEN** a counter `device.discovery.completed` SHALL be incremented

#### Scenario: Device discovered
- **WHEN** a new device is found during discovery and passed to the device service
- **THEN** a counter `device.discovered.count` SHALL be incremented with attribute `device.class`

### Requirement: Device add/remove counters
The system SHALL maintain counters tracking device database additions and removals.

#### Scenario: Device added successfully
- **WHEN** a device is successfully persisted to the device database
- **THEN** a counter `device.add.success` SHALL be incremented with attribute `device.class`

#### Scenario: Device add failed
- **WHEN** a device addition fails (database error, validation failure)
- **THEN** a counter `device.add.failed` SHALL be incremented with attribute `device.class`

#### Scenario: Device removed successfully
- **WHEN** a device is successfully removed from the device database
- **THEN** a counter `device.remove.success` SHALL be incremented with attribute `device.class`

### Requirement: Device rejection counter
The system SHALL count devices rejected by discovery filters.

#### Scenario: Device rejected by filter
- **WHEN** `checkDeviceDiscoveryFilters()` rejects a device during discovery
- **THEN** a counter `device.rejected.count` SHALL be incremented with attribute `device.class`

### Requirement: Discovery duration histogram
The system SHALL record the duration of discovery operations.

#### Scenario: Discovery duration recorded
- **WHEN** a discovery scan completes
- **THEN** a histogram `device.discovery.duration` SHALL record the elapsed time in seconds with attribute `device.class`

### Requirement: Device lifecycle meters initialization
All device lifecycle meters SHALL be created during `deviceServiceInitialize()` and released during `deviceServiceShutdown()`.

#### Scenario: Meters created at init
- **WHEN** `deviceServiceInitialize()` is called with `BCORE_OBSERVABILITY=ON`
- **THEN** all device lifecycle counters and histograms SHALL be created via the observability metrics API

#### Scenario: Meters are no-ops when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF`
- **THEN** all device lifecycle meter creation calls SHALL return NULL and recording calls SHALL be no-ops
