## ADDED Requirements

### Requirement: Device commissioning span
The observability layer SHALL trace the full Matter commissioning operation as a span tree rooted at the `deviceServiceCommissionDevice()` entry point.

#### Scenario: Commission span created
- **WHEN** `deviceServiceCommissionDevice()` is called
- **THEN** a root span named `device.commission` SHALL be created with attributes `commissioning.payload` (the setup payload) and `commissioning.timeout_seconds`

#### Scenario: Commission span propagated to Matter subsystem
- **WHEN** `matterSubsystemCommissionDevice()` is called from the commissioning thread
- **THEN** the span context from the `device.commission` root span SHALL be propagated so that child spans created in the Matter subsystem share the same trace ID

#### Scenario: Commission span records success
- **WHEN** the commissioning operation completes successfully
- **THEN** the `device.commission` span SHALL be ended with OK status

#### Scenario: Commission span records failure
- **WHEN** the commissioning operation fails or times out
- **THEN** the `device.commission` span SHALL be ended with ERROR status and an error message describing the failure

### Requirement: Device pairing span
The observability layer SHALL trace the full Matter pairing operation (pre-commissioned device onboarding) as a span tree.

#### Scenario: Pair span created
- **WHEN** `deviceServiceAddMatterDevice()` is called
- **THEN** a root span named `device.pair` SHALL be created with attributes `device.node_id` and `pairing.timeout_seconds`

#### Scenario: Pair span propagated to orchestrator
- **WHEN** `CommissioningOrchestrator::Pair()` executes
- **THEN** child spans SHALL be created under the `device.pair` root for key steps: sending commissioning complete, creating the device data cache, waiting for subscription, and triggering device found

#### Scenario: Pair span records outcome
- **WHEN** the pairing operation completes
- **THEN** the `device.pair` span SHALL be ended with status reflecting success or failure

### Requirement: Device add span
The observability layer SHALL trace the device addition flow from `deviceServiceDeviceFound()` through driver configuration and database persistence.

#### Scenario: Device found as child of discovery or commission
- **WHEN** `deviceServiceDeviceFound()` is called during discovery
- **THEN** the existing `device.found` span SHALL be created as a child of the `device.discovery` span (if a discovery span context is available) rather than as an isolated root span

#### Scenario: Device found as child of pairing
- **WHEN** `deviceServiceDeviceFound()` is called from the pairing flow
- **THEN** the `device.found` span SHALL be created as a child of the `device.pair` span

#### Scenario: Device configure child span
- **WHEN** a device driver's configure callback is invoked during device addition
- **THEN** a child span named `device.configure` SHALL be created under the `device.found` span with attribute `device.class`

#### Scenario: Device persist child span
- **WHEN** the device is saved to the database via `jsonDatabaseAddDevice()`
- **THEN** a child span named `device.persist` SHALL be created under the `device.found` span with attribute `device.uuid`

### Requirement: Device remove span
The observability layer SHALL trace the device removal operation.

#### Scenario: Remove span created
- **WHEN** `deviceServiceRemoveDevice()` is called
- **THEN** a root span named `device.remove` SHALL be created with attribute `device.uuid`

#### Scenario: Driver removal child span
- **WHEN** the device driver's `deviceRemoved` callback is invoked
- **THEN** a child span named `device.driver.remove` SHALL be created under the `device.remove` span with attribute `driver.type`

#### Scenario: Remove span records outcome
- **WHEN** the removal operation completes
- **THEN** the `device.remove` span SHALL be ended with status reflecting success or failure

### Requirement: Commissioning window span
The observability layer SHALL trace the open commissioning window operation.

#### Scenario: Open commissioning window span
- **WHEN** `deviceServiceOpenCommissioningWindow()` is called
- **THEN** a root span named `device.commission_window.open` SHALL be created with attributes `device.node_id` and `window.timeout_seconds`

#### Scenario: Commissioning window span records outcome
- **WHEN** the operation completes
- **THEN** the span SHALL be ended with status reflecting success or failure

### Requirement: Span context propagation across commissioning threads
Span context SHALL be propagated from the main thread through the detached commissioning/pairing threads to the Matter platform thread without modifying internal function signatures.

#### Scenario: Context passed to commissioning thread via arg struct
- **WHEN** a detached thread is created for commissioning or pairing
- **THEN** the file-local thread argument struct SHALL carry a ref-counted `ObservabilitySpanContext*` from the root span, and the thread function SHALL set it as the thread-local current context at entry

#### Scenario: Context passed to CommissioningOrchestrator
- **WHEN** `CommissioningOrchestrator` is constructed
- **THEN** it SHALL accept and ref-acquire an `ObservabilitySpanContext*` for creating child spans during the orchestration lifecycle, releasing the ref in its destructor

#### Scenario: No function signature changes required
- **WHEN** context is propagated through the commissioning and pairing flows
- **THEN** the signatures of `matterSubsystemCommissionDevice()`, `matterSubsystemPairDevice()`, `onDeviceFoundCallback()`, and `DeviceFoundDetails` SHALL NOT be modified; same-thread flows SHALL use the thread-local current context instead
