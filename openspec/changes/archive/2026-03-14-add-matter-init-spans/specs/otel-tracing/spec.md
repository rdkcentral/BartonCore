## MODIFIED Requirements

### Requirement: Observability initialization and shutdown
The observability layer SHALL be initialized during `deviceServiceInitialize()` and shut down (with flush) during `deviceServiceShutdown()`. Initialization SHALL configure a `TracerProvider` with an OTLP HTTP span exporter. Shutdown SHALL flush all pending spans before releasing resources. Subsystem initialization spans SHALL form parent-child hierarchies — the Matter subsystem's async init SHALL produce `matter.init` root spans with child spans for stack, server, commissioner, and fabric creation phases.

#### Scenario: Successful initialization
- **WHEN** `deviceServiceInitialize()` is called with `BCORE_OBSERVABILITY=ON`
- **THEN** a `TracerProvider` SHALL be created with an OTLP HTTP exporter configured from environment variables (`OTEL_EXPORTER_OTLP_ENDPOINT`, `OTEL_SERVICE_NAME`)

#### Scenario: Initialization with default config
- **WHEN** no `OTEL_EXPORTER_OTLP_ENDPOINT` environment variable is set
- **THEN** the exporter SHALL default to `http://localhost:4318`

#### Scenario: Clean shutdown
- **WHEN** `deviceServiceShutdown()` is called
- **THEN** the observability layer SHALL flush all pending spans and shut down the `TracerProvider` before the service stops

#### Scenario: Feature disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` (default)
- **THEN** all tracing API calls SHALL be no-ops with zero runtime overhead

#### Scenario: Matter init spans hierarchy
- **WHEN** the Matter subsystem initializes successfully
- **THEN** a `matter.init` root span SHALL be exported with child spans `matter.init.stack`, `matter.init.server`, and `matter.init.commissioner` sharing the same traceId
