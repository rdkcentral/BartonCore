## ADDED Requirements

### Requirement: CASE session connection span
The observability layer SHALL trace Matter CASE session establishment as a child span of the commissioning or pairing operation.

#### Scenario: CASE connect span created
- **WHEN** `DeviceDataCache` initiates a device connection via `GetConnectedDevice()`
- **THEN** a child span named `matter.case.connect` SHALL be created under the parent operation's span context with attribute `device.id`

#### Scenario: CASE connect span records success
- **WHEN** `OnDeviceConnected()` fires in `DeviceDataCache`
- **THEN** the `matter.case.connect` span SHALL be ended with OK status

#### Scenario: CASE connect span records failure
- **WHEN** `OnDeviceConnectionFailure()` fires in `DeviceDataCache`
- **THEN** the `matter.case.connect` span SHALL be ended with ERROR status and the CHIP_ERROR code as an attribute `error.code`

### Requirement: Matter subscription span
The observability layer SHALL trace the subscription establishment as a child span.

#### Scenario: Subscribe span created
- **WHEN** `OnDeviceConnected()` sends a subscription request (auto-resubscribe with wildcard)
- **THEN** a child span named `matter.subscribe` SHALL be created under the parent operation's span context with attribute `device.id`

#### Scenario: Subscribe span records establishment
- **WHEN** `OnSubscriptionEstablished()` fires
- **THEN** the `matter.subscribe` span SHALL be ended with OK status

#### Scenario: Subscribe span records error
- **WHEN** `OnError()` fires before subscription is established
- **THEN** the `matter.subscribe` span SHALL be ended with ERROR status

### Requirement: Matter report cycle span
The observability layer SHALL trace subscription report cycles.

#### Scenario: Report cycle span created
- **WHEN** `OnReportBegin()` fires on the DeviceDataCache
- **THEN** a span named `matter.report` SHALL be created with attribute `device.id`

#### Scenario: Report cycle span completed
- **WHEN** `OnReportEnd()` fires
- **THEN** the `matter.report` span SHALL be ended with OK status

### Requirement: Matter fabric removal span
The observability layer SHALL trace the fabric removal operation during device removal.

#### Scenario: Fabric remove span created
- **WHEN** `MatterDeviceDriver::DeviceRemoved()` initiates fabric index read and RemoveFabric command
- **THEN** a child span named `matter.fabric.remove` SHALL be created under the `device.remove` span context with attribute `device.uuid`

#### Scenario: Fabric remove span runs on Matter thread
- **WHEN** the fabric removal executes via `RunOnMatterSync()`
- **THEN** the span context SHALL be propagated to the Matter platform thread so the span correctly covers the full operation

#### Scenario: Fabric remove span records outcome
- **WHEN** the fabric removal completes or fails
- **THEN** the `matter.fabric.remove` span SHALL be ended with status reflecting the outcome

### Requirement: Span context storage in DeviceDataCache
DeviceDataCache SHALL store a ref-counted parent span context for creating child spans from Matter platform thread callbacks.

#### Scenario: Context ref-acquired at construction
- **WHEN** a `DeviceDataCache` is constructed with a parent span context
- **THEN** it SHALL call `observabilitySpanContextRef()` on the context and store it for use as the parent for `matter.case.connect`, `matter.subscribe`, and `matter.report` spans

#### Scenario: Callbacks set TLS from stored context
- **WHEN** a Matter platform thread callback fires (`OnDeviceConnected`, `OnSubscriptionEstablished`, `OnReportBegin`, etc.)
- **THEN** the callback SHALL set the stored context as the thread-local current context via `observabilitySpanContextSetCurrent()` before creating child spans

#### Scenario: Context released at destruction
- **WHEN** the `DeviceDataCache` is destroyed
- **THEN** the stored `ObservabilitySpanContext*` SHALL be released via `observabilitySpanContextRelease()`
