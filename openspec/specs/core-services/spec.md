## ADDED Requirements

### Requirement: DeviceService orchestrator lifecycle
The `deviceService` SHALL orchestrate the complete service lifecycle in two phases: `deviceServiceInitialize()` (configuration, database init) and `deviceServiceStart()` (event system, drivers, subsystems). Shutdown SHALL follow a defined teardown order (event handler → storage monitor → comm-fail → drivers → subsystems → event producer → database). When all subsystems report ready, the service SHALL first flag every device that requires startup reconfiguration as having a pending reconfiguration, and only then publish readiness for device operation: report `isReadyForDeviceOperation` as true in the service status, notify drivers of the status change, and emit the `READY_FOR_DEVICE_OPERATION` status event. No observer SHALL see the service as ready while a device awaiting startup reconfiguration is not yet flagged. Readiness SHALL be reported as false again once shutdown begins.

#### Scenario: Two-phase startup
- **WHEN** `b_core_client_new()` is called followed by `b_core_client_start()`
- **THEN** initialization SHALL set up configuration and database, and start SHALL launch event handlers, initialize drivers via driver manager, initialize subsystems via subsystem manager, start driver startup, and notify subsystems when all drivers are started

#### Scenario: Asynchronous readiness
- **WHEN** all registered subsystems report ready via their callbacks
- **THEN** the device service SHALL flag devices requiring reconfiguration as pending, then report itself ready for device operation, notify drivers, and emit a `status-changed` event with reason `READY_FOR_DEVICE_OPERATION`

#### Scenario: Reconfiguration flagged before readiness is published
- **WHEN** a device requiring reconfiguration exists at startup and the service becomes ready for device operation
- **THEN** at the first moment `isReadyForDeviceOperation` is reported as true, that device SHALL already be marked as having a pending reconfiguration, unless that reconfiguration has already finished or could not be scheduled, so any client (one reacting to the event, polling the status, or calling gated functions directly) SHALL observe `B_CORE_SERVICE_STATE_ERROR_NOT_READY` or `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING` for it until then

#### Scenario: Shutdown during startup
- **WHEN** shutdown begins while the service is still flagging devices for startup reconfiguration
- **THEN** the service SHALL stop scheduling startup work and SHALL NOT report itself ready, notify drivers of readiness, or emit `READY_FOR_DEVICE_OPERATION`, and shutdown SHALL wait (for up to 120 seconds) for the startup pass to finish before stopping drivers and subsystems, unless shutdown was started from that pass's own thread

#### Scenario: Shutdown sequence
- **WHEN** `b_core_client_stop()` is called
- **THEN** the service SHALL shut down in this order: event handler, storage monitor, comm-fail watchdog, drivers, subsystems, event producer, database, main loop, configuration

### Requirement: Reconfiguration pending state
A device SHALL be reported as having a pending reconfiguration (`deviceServiceIsReconfigurationPending()`) from the moment its reconfiguration is registered for scheduling until that reconfiguration's work is done (before its completion event is sent), it is cancelled, or it fails to be scheduled, and at no other time. Because public reads of the device are refused while it is pending, the pending state SHALL NOT outlive the scheduled work.

#### Scenario: Scheduling fails
- **WHEN** a reconfiguration is requested but the task scheduler reports that its task could not be scheduled
- **THEN** the device SHALL no longer be reported as pending, and the reconfiguration context SHALL be released

#### Scenario: Task finishes before scheduling returns
- **WHEN** a reconfiguration is scheduled with no delay and its task finishes before the scheduling call returns
- **THEN** the scheduling call SHALL NOT access the released reconfiguration context

#### Scenario: Device removed right after scheduling
- **WHEN** a device is removed after its reconfiguration has been scheduled but before that reconfiguration has started
- **THEN** the removal SHALL cancel the not-yet-started reconfiguration rather than wait for it to run

#### Scenario: Pending reconfiguration cancelled
- **WHEN** a reconfiguration that has not started yet is cancelled (device removal or shutdown)
- **THEN** the device SHALL no longer be reported as pending, and the reference held for the cancelled task SHALL be released

#### Scenario: Reconfiguration starts as it is being cancelled
- **WHEN** a removal or shutdown tries to cancel a reconfiguration whose task has already started
- **THEN** the device SHALL remain reported as pending until that task's work is done, the task SHALL be asked to terminate, and a removal SHALL wait for the task to finish, including sending any completion event, for up to 60 seconds

#### Scenario: Same reconfiguration cancelled twice
- **WHEN** two cancellers (for example a removal racing shutdown) both try to cancel the same not-yet-started reconfiguration
- **THEN** exactly one SHALL cancel it and release its reference, and the other SHALL NOT act on the cancelled task's handle

#### Scenario: Older reconfiguration finishes after a newer one is scheduled
- **WHEN** a reconfiguration finishes while the pending entry for its device belongs to a newer reconfiguration
- **THEN** the newer reconfiguration SHALL remain pending

#### Scenario: Reconfiguration requested while the previous one finishes
- **WHEN** a reconfiguration of a device is requested while that device's previous reconfiguration is sending its completion event
- **THEN** the request SHALL return without waiting and SHALL be scheduled, and the device SHALL be reported as pending from then on, rather than the request being dropped as a duplicate

#### Scenario: Reconfiguration requested while the device is being removed
- **WHEN** a reconfiguration of a device is requested while a removal of that device is in progress
- **THEN** it SHALL NOT be scheduled while the removal is in progress; if the removal fails and the device still exists, it SHALL then be scheduled, provided the removal did not give up waiting (after 60 seconds) for a reconfiguration of the device that is still running

#### Scenario: Reconfiguration requested for an unknown device
- **WHEN** a reconfiguration is requested for a device that has no stored record
- **THEN** it SHALL NOT be scheduled, and the device SHALL NOT be reported as pending, except momentarily if the device is deleted while the request is being made

#### Scenario: Reconfiguration requested during shutdown
- **WHEN** a reconfiguration is requested after shutdown has begun
- **THEN** it SHALL NOT be scheduled and the device SHALL NOT be reported as pending

#### Scenario: Shutdown with a reconfiguration in progress
- **WHEN** shutdown begins while a reconfiguration task is running
- **THEN** shutdown SHALL ask it to terminate and block (not poll) until it finishes, including any completion event already being sent, or 120 seconds pass, before stopping drivers and subsystems. A shutdown started from that task's own completion event handler SHALL NOT wait for the task

#### Scenario: Running reconfiguration finishes as a removal starts waiting
- **WHEN** a removal must wait for a running reconfiguration and that reconfiguration finishes before the removal begins waiting
- **THEN** the removal SHALL proceed without waiting for the timeout

#### Scenario: Repeated startup pass after a reconfiguration finished
- **WHEN** the startup pass runs again and a device it saw as needing reconfiguration was already reconfigured by an earlier pass
- **THEN** that device SHALL NOT be reconfigured again

#### Scenario: Startup pass while a reconfiguration is pending
- **WHEN** a startup pass (the first or a repeated one) finds a device with a reconfiguration pending, whether from an earlier pass or requested for another reason
- **THEN** that device SHALL be neither reconfigured again nor synchronized by this pass, whatever its stored class and profile versions
- **AND** if that pending reconfiguration then fails, the device SHALL be synchronized as the pass would otherwise have done; if the device is being removed, only once that removal fails, and if a reconfiguration of the device is pending by then, only if that reconfiguration fails too

### Requirement: Pending reconfiguration metric
The device service SHALL record a `device.reconfiguration.pending` gauge (unit `1`, no attributes) through the observability metrics API, holding the number of devices with a pending or in-progress reconfiguration. It SHALL be recorded as 0 when the service starts and updated whenever a reconfiguration becomes or stops being pending.

#### Scenario: Gauge tracks pending reconfigurations
- **WHEN** a reconfiguration is scheduled and later finishes or is cancelled
- **THEN** the gauge SHALL increase by one when it is scheduled and decrease by one when it stops being pending

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
The `deviceCommunicationWatchdog` SHALL monitor per-device communication health with configurable per-device timeouts. A monitor thread SHALL wake every 60 seconds to check timers. The check interval MAY be overridden via the `barton.commFail.monitorIntervalSecs` property; when non-zero this value replaces the 60-second default. The override SHALL be applied both at startup (one-shot read after `deviceServiceCommFailInit()`) and at runtime (via the `BCorePropertyProvider` `property-changed` signal), so that a write made after service start takes effect on the next watchdog loop iteration.

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

#### Scenario: Live commFailOverrideSeconds reprogramming
- **WHEN** the `commFailOverrideSeconds` metadata key is written for a device via `setMetadata()`
- **THEN** the running per-device watchdog timer SHALL be reprogrammed immediately to the new timeout without requiring a device reconnect or service restart

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
