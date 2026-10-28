## ADDED Requirements

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
- **AND** if that pending reconfiguration then fails, the device SHALL be synchronized as the pass would otherwise have done; if the device is being removed, only once that removal fails

### Requirement: Pending reconfiguration metric
The device service SHALL record a `device.reconfiguration.pending` gauge (unit `1`, no attributes) through the observability metrics API, holding the number of devices with a pending or in-progress reconfiguration. It SHALL be recorded as 0 when the service starts and updated whenever a reconfiguration becomes or stops being pending.

#### Scenario: Gauge tracks pending reconfigurations
- **WHEN** a reconfiguration is scheduled and later finishes or is cancelled
- **THEN** the gauge SHALL increase by one when it is scheduled and decrease by one when it stops being pending

## MODIFIED Requirements

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
