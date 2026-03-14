## ADDED Requirements

### Requirement: Matter init root span
The system SHALL create a root span named `matter.init` at the beginning of each `maybeInitMatter()` invocation, with attribute `retry.attempt` set to the current attempt number (0-based). The span SHALL be ended with success status when initialization completes successfully, or error status with the failure reason when initialization fails.

#### Scenario: Successful first-time init
- **WHEN** `maybeInitMatter()` is called for the first time and initialization succeeds
- **THEN** a `matter.init` span SHALL be exported with `retry.attempt=0` and OK status

#### Scenario: Init fails and retries
- **WHEN** `maybeInitMatter()` fails on attempt 0 and succeeds on attempt 1
- **THEN** two `matter.init` spans SHALL be exported: one with `retry.attempt=0` and ERROR status, one with `retry.attempt=1` and OK status

### Requirement: CHIP stack init span
The system SHALL create a child span named `matter.init.stack` covering the `Matter::Init()` call. The span SHALL inherit parent context from TLS. On failure, the span SHALL record the CHIP error string.

#### Scenario: Stack init succeeds
- **WHEN** `Matter::Init()` completes successfully
- **THEN** a `matter.init.stack` span SHALL be exported as a child of `matter.init` with OK status

#### Scenario: Stack init fails
- **WHEN** `Matter::Init()` fails (e.g., `InitChipStack` returns error)
- **THEN** the `matter.init.stack` span SHALL be ended with ERROR status and the CHIP error message

### Requirement: Server init span
The system SHALL create a child span named `matter.init.server` covering the `Server::GetInstance().Init()` call inside `Matter::Start()`. The span SHALL inherit parent context from TLS.

#### Scenario: Server init succeeds
- **WHEN** `Server::GetInstance().Init()` completes successfully
- **THEN** a `matter.init.server` span SHALL be exported as a child of `matter.init` with OK status

#### Scenario: Server init skipped on re-entry
- **WHEN** `Matter::Start()` is called but `serverIsInitialized` is already true
- **THEN** no `matter.init.server` span SHALL be created

### Requirement: Commissioner init span
The system SHALL create a child span named `matter.init.commissioner` covering the `InitCommissioner()` call inside `Matter::Start()`. The span SHALL inherit parent context from TLS. On failure, the span SHALL record the CHIP error string.

#### Scenario: Commissioner init succeeds
- **WHEN** `InitCommissioner()` completes successfully
- **THEN** a `matter.init.commissioner` span SHALL be exported as a child of `matter.init` with OK status

#### Scenario: Commissioner init fails
- **WHEN** `InitCommissioner()` returns a non-success CHIP_ERROR
- **THEN** the `matter.init.commissioner` span SHALL be ended with ERROR status and the CHIP error message

### Requirement: Fabric creation span
The system SHALL create a child span named `matter.fabric.create` inside `InitCommissioner()` when a new NOC chain must be generated (no existing chain or no matching fabric found). The span SHALL cover the key allocation, NOC chain generation, and chain persistence steps. The span SHALL NOT be created when an existing NOC chain and fabric are loaded successfully.

#### Scenario: New fabric created
- **WHEN** `InitCommissioner()` determines no existing NOC chain or fabric exists
- **THEN** a `matter.fabric.create` span SHALL be exported as a child of `matter.init.commissioner` with OK status

#### Scenario: Existing fabric loaded
- **WHEN** `InitCommissioner()` finds an existing NOC chain and fabric
- **THEN** no `matter.fabric.create` span SHALL be created

### Requirement: mDNS advertisement span
The system SHALL create a child span named `matter.init.advertise` inside `InitCommissioner()` covering the `DnssdServer::AdvertiseOperational()` call.

#### Scenario: Advertisement succeeds
- **WHEN** `AdvertiseOperational()` is called during commissioner init
- **THEN** a `matter.init.advertise` span SHALL be exported as a child of `matter.init.commissioner`
