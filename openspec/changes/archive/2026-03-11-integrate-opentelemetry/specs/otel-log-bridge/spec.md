## ADDED Requirements

### Requirement: Log provider initialization
The observability layer SHALL configure a `LoggerProvider` with an OTLP HTTP log exporter during initialization. The provider SHALL use a simple log record processor.

#### Scenario: Log provider created
- **WHEN** `deviceServiceInitialize()` is called with `BCORE_OBSERVABILITY=ON`
- **THEN** a `LoggerProvider` SHALL be created with a simple processor and OTLP HTTP log exporter

#### Scenario: Log provider shutdown
- **WHEN** `deviceServiceShutdown()` is called
- **THEN** the `LoggerProvider` SHALL flush all pending log records and be shut down

#### Scenario: Log bridge disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF`
- **THEN** the icLog backend SHALL not be affected; existing logging behavior SHALL be preserved

### Requirement: icLog integration via linker wrap
A linker-wrap shim (`--wrap=icLogMsg`) SHALL intercept `icLogMsg` calls and forward them to the OpenTelemetry Logs SDK. The shim SHALL be compiled when `BCORE_OBSERVABILITY=ON` and linked into the final binary via the `--wrap` linker flag. The shim calls through to the original `__real_icLogMsg` to preserve existing log output.

#### Scenario: Log message forwarded
- **WHEN** `icLogDebug("Matter", "Device %s paired", deviceId)` is called
- **THEN** a log record SHALL be emitted to the OpenTelemetry LoggerProvider with severity DEBUG

#### Scenario: Category name mapping
- **WHEN** an `icLogMsg` call is made with `categoryName = "zigbee"`
- **THEN** the resulting OTel log record SHALL have `InstrumentationScope.name = "zigbee"`

#### Scenario: Priority to severity mapping
- **WHEN** log messages are emitted at each `icLog` priority level
- **THEN** the mapping SHALL be: `IC_LOG_TRACE` → `TRACE`, `IC_LOG_DEBUG` → `DEBUG`, `IC_LOG_INFO` → `INFO`, `IC_LOG_WARN` → `WARN`, `IC_LOG_ERROR` → `ERROR`

#### Scenario: Source location attributes
- **WHEN** `icLogMsg` is called with `file`, `func`, and `line` parameters
- **THEN** the OTel log record SHALL include attributes `code.filepath`, `code.function`, and `code.lineno` with the corresponding values

#### Scenario: Message body
- **WHEN** a formatted log message "Device abc123 timeout" is generated
- **THEN** the OTel log record body SHALL contain the string "Device abc123 timeout"

### Requirement: Trace-log correlation
When a log message is emitted within the context of an active span, the log record SHALL be automatically correlated with that span's trace context.

#### Scenario: Log within span context
- **WHEN** an `icLog` call is made while a `ObservabilitySpan` is active on the current thread
- **THEN** the resulting OTel log record SHALL include the span's `trace_id` and `span_id`

#### Scenario: Log without span context
- **WHEN** an `icLog` call is made with no active span
- **THEN** the log record SHALL be emitted without trace/span correlation (standalone log)

### Requirement: Dual output mode
The log bridge SHALL NOT suppress the existing logging backend. When enabled, log messages SHALL be forwarded to BOTH the existing backend (stdout/syslog) AND the OpenTelemetry Logs SDK.

#### Scenario: Dual output
- **WHEN** `BCORE_OBSERVABILITY=ON` and `icLogInfo("core", "Starting service")` is called
- **THEN** the message SHALL appear in the existing log output (e.g., stdout) AND be sent as an OTel log record to the collector

### Requirement: Log bridge unit tests
The log bridge module SHALL have C++ unit tests (Google Test) under `core/test/` that verify log record creation, severity mapping, attribute mapping, and trace-log correlation.

#### Scenario: Unit test severity mapping
- **WHEN** the log bridge unit tests are run
- **THEN** they SHALL verify that each `icLog` priority level maps to the correct OTel severity (TRACE, DEBUG, INFO, WARN, ERROR)

#### Scenario: Unit test source location attributes
- **WHEN** a log record is created with file, func, and line parameters
- **THEN** the unit test SHALL verify `code.filepath`, `code.function`, and `code.lineno` attributes are set correctly

#### Scenario: Unit test trace-log correlation
- **WHEN** a log record is emitted within an active span context
- **THEN** the unit test SHALL verify the log record contains the span's trace_id and span_id

### Requirement: Log bridge integration tests
The log bridge behavior SHALL be verified by pytest integration tests under `testing/` that exercise the full BartonCore library with `BCORE_OBSERVABILITY=ON`.

#### Scenario: Integration test structured logs emitted
- **WHEN** a pytest integration test starts a `BCore.Client` with OpenTelemetry enabled and collects exported OTLP data
- **THEN** the test SHALL verify that OTel log records are emitted with correct severity, scope, and body content matching `icLog` output
