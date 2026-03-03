## ADDED Requirements

### Requirement: DeviceService orchestrator lifecycle
The `deviceService` SHALL orchestrate the complete service lifecycle in two phases: `deviceServiceInitialize()` (configuration, database init) and `deviceServiceStart()` (event system, drivers, subsystems). Shutdown SHALL follow a defined teardown order (event handler → storage monitor → comm-fail → drivers → subsystems → event producer → database).

#### Scenario: Two-phase startup
- **WHEN** `b_core_client_new()` is called followed by `b_core_client_start()`
- **THEN** initialization SHALL set up configuration and database, and start SHALL launch event handlers, initialize drivers via driver manager, initialize subsystems via subsystem manager, start driver startup, and notify subsystems when all drivers are started

#### Scenario: Asynchronous readiness
- **WHEN** all registered subsystems report ready via their callbacks
- **THEN** the device service SHALL emit a `status-changed` event with reason `READY_FOR_DEVICE_OPERATION`

#### Scenario: Shutdown sequence
- **WHEN** `b_core_client_stop()` is called
- **THEN** the service SHALL shut down in this order: event handler, storage monitor, comm-fail watchdog, drivers, subsystems, event producer, database, main loop, configuration

### Requirement: Device driver manager
The `deviceDriverManager` SHALL maintain a hash map of registered `DeviceDriver*` structs and an ordered linked list preserving registration order. Access SHALL be protected by a `pthread_mutex_t`.

#### Scenario: Driver registration
- **WHEN** `deviceDriverManagerRegisterDriver()` is called with a valid `DeviceDriver` struct
- **THEN** the driver SHALL be added to both the hash map (keyed by name) and the ordered list

#### Scenario: Driver lookup by device class
- **WHEN** `GetDeviceDriversByDeviceClass("light")` is called
- **THEN** all drivers with `"light"` in their `supportedDeviceClasses` list SHALL be returned

#### Scenario: Driver lookup by subsystem
- **WHEN** `GetDeviceDriversBySubsystem("matter")` is called
- **THEN** all drivers with `subsystemName == "matter"` SHALL be returned

#### Scenario: Ordered startup
- **WHEN** `deviceDriverManagerStartDeviceDrivers()` is called
- **THEN** each driver's `startup()` SHALL be invoked in registration order

### Requirement: Subsystem manager
The `subsystemManager` SHALL track registered subsystems, support pre-registration via `__attribute__((constructor))`, and fire a `readyForDevicesCallback` only when ALL registered subsystems report ready AND all drivers have been started (via `subsystemManagerAllDriversStarted()`).

#### Scenario: Pre-registration
- **WHEN** a subsystem registers via `__attribute__((constructor))` before `main()`
- **THEN** the subsystem SHALL be stored in `preRegisteredSubsystems` and moved to the active registry during `subsystemManagerInitialize()`

#### Scenario: Readiness gate
- **WHEN** two subsystems are registered (Matter and Zigbee) and only Matter reports ready
- **THEN** `subsystemManagerIsReadyForDevices()` SHALL return `FALSE`

#### Scenario: All subsystems ready
- **WHEN** both Matter and Zigbee subsystems report ready AND all drivers have been started
- **THEN** `subsystemManagerIsReadyForDevices()` SHALL return `TRUE` and the `readyForDevicesCallback` SHALL be invoked

#### Scenario: Subsystem migration
- **WHEN** a subsystem's stored version differs from its current version
- **THEN** the subsystem manager SHALL invoke `checkSubsystemForMigration()` to run the subsystem's `migrate` callback

### Requirement: Communication watchdog
The `deviceCommunicationWatchdog` SHALL monitor per-device communication health with configurable per-device timeouts. A monitor thread SHALL wake every 60 seconds to check timers.

#### Scenario: Default timeout
- **WHEN** a Zigbee device is registered with the watchdog without a custom timeout
- **THEN** the default timeout SHALL be 56 minutes (3,360,000 milliseconds)

#### Scenario: Communication failure detection
- **WHEN** no successful communication occurs with a device within its timeout period
- **THEN** the watchdog SHALL invoke the `failedCallback` and set the device's `inCommFail` flag

#### Scenario: Communication restored
- **WHEN** successful communication occurs with a device currently in comm-fail
- **THEN** the watchdog SHALL invoke the `restoredCallback` and clear the `inCommFail` flag

#### Scenario: Timer reset on communication
- **WHEN** successful communication occurs with a device
- **THEN** the watchdog SHALL reset the device's `commFailTimeRemainingMillis` to `commFailTimeoutSeconds * 1000`

#### Scenario: Debounce rapid events
- **WHEN** multiple communication events occur for a device within 5000 milliseconds
- **THEN** the watchdog SHALL always reset the comm-fail timer and restore comm-fail status, but SHALL debounce the `updateDeviceDateLastContacted()` database write to only fire once per 5000ms window

#### Scenario: Fast comm-fail mode
- **WHEN** fast comm-fail mode is activated
- **THEN** the watchdog thread SHALL use an accelerated check interval via condition variable signaling

### Requirement: JSON database persistence
The `jsonDatabase` SHALL persist all device data, endpoints, resources, metadata, and system properties as JSON files in the configured storage directory. One file per device, plus a `systemProperties` file.

#### Scenario: Device file creation
- **WHEN** a new device is added to the database
- **THEN** a JSON file named with the device UUID SHALL be created in the storage directory

#### Scenario: Device file structure
- **WHEN** a device file is read
- **THEN** it SHALL contain: `managingDeviceDriver`, `deviceClass`, `deviceClassVersion`, `uri`, `uuid`, `endpoints` (list), `resources` (list), `metadata` (list)

#### Scenario: Backup files
- **WHEN** a device file is saved
- **THEN** a backup copy with `.bak` extension SHALL also be maintained

#### Scenario: Schema versioning
- **WHEN** the database is initialized
- **THEN** it SHALL check the schema version (current: `"2"`) and perform migrations if needed

#### Scenario: URI-based lookup
- **WHEN** `GetResourceByUri("/deviceId/ep/1/r/isOn")` is called
- **THEN** the database SHALL parse the URI, locate the device, endpoint, and resource, and return it

#### Scenario: Regex-based resource query
- **WHEN** `GetResourcesByUriRegex(".*communicationFailure")` is called
- **THEN** the database SHALL return all resources across all devices matching the URI pattern

#### Scenario: System properties
- **WHEN** `GetSystemProperty("ZIGBEE_LOCAL_EUI64")` is called
- **THEN** the database SHALL return the value from the `systemProperties` file

### Requirement: Event producer
The `deviceEventProducer` SHALL emit all events as GObject signals on the `BCoreClient` class. Each event type SHALL have a corresponding signal and typed event object.

#### Scenario: Resource updated signal
- **WHEN** a device resource value changes
- **THEN** the event producer SHALL create a `BCoreResourceUpdatedEvent` and emit the `resource-updated` signal on the BCoreClient instance

#### Scenario: Device added signal
- **WHEN** a new device is fully configured and persisted
- **THEN** the event producer SHALL create a `BCoreDeviceAddedEvent` and emit the `device-added` signal

#### Scenario: Signal emission thread
- **WHEN** an event needs to be emitted
- **THEN** it SHALL be emitted directly via `g_signal_emit()` on the calling thread

### Requirement: Device event handler
The `deviceEventHandler` SHALL handle property provider changes (descriptor updates, comm-fail timeout, timezone, firmware URL) and route relevant changes to internal subsystems. Resource-level change events are handled directly in `deviceService.c`.

#### Scenario: Internal event handling
- **WHEN** a driver reports a resource change
- **THEN** the event handler SHALL process the change (update database, apply business logic) and forward it to the event producer

### Requirement: Discovery filter system
The system SHALL support discovery filters via `BCoreDiscoveryFilter` objects containing `uri` (regex pattern) and `value` (regex pattern). Filters SHALL be applied during device discovery to match only devices whose resources match the filter criteria.

#### Scenario: Filter by resource value
- **WHEN** discovery is started with a filter `uri=".*/r/type"` and `value="motion"`
- **THEN** only devices with a resource matching the URI pattern containing `"motion"` as the value SHALL be accepted

### Requirement: Device storage monitor
The system SHALL monitor the device storage directory for external changes and emit `storage-changed` events when files are modified outside the service.

#### Scenario: External file modification
- **WHEN** a device file is modified externally while the service is running
- **THEN** a `BCoreStorageChangedEvent` SHALL be emitted with the `what-changed` property indicating the type of change

### Requirement: Device scrubbing
During startup, the device service SHALL scrub persisted devices by removing any devices that have no enabled endpoints.

#### Scenario: Startup scrub
- **WHEN** `deviceServiceStart()` runs during service initialization
- **THEN** the service SHALL scan all persisted devices and remove any that have no enabled endpoints
