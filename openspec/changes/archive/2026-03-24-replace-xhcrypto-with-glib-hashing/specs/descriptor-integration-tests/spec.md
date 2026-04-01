## ADDED Requirements

### Requirement: Local HTTP server fixture for descriptor tests
A `FixtureHTTPServer` SHALL serve device descriptor XML files over HTTP on an OS-assigned port with thread-safe request count tracking per path. A `DeviceDescriptorServer` mock SHALL wrap it to serve files from `testing/resources/deviceDescriptor/`, support indirect parametrize for custom file paths, and expose an `allowlist_content` property. The server SHALL start before `BCore.Client.start()` and stop after the test completes.

#### Scenario: Fixture serves AllowList.xml
- **WHEN** the HTTP server fixture is started with the test fixtures directory
- **THEN** it SHALL serve files from that directory at `http://localhost:<port>/`
- **AND** an HTTP GET to `http://localhost:<port>/AllowList.xml` SHALL return the fixture file contents

#### Scenario: Fixture uses OS-assigned port
- **WHEN** the HTTP server is initialized with port 0
- **THEN** the OS SHALL assign an available port and the fixture SHALL expose that port number to tests

### Requirement: Descriptor-focused test environment orchestrator
A test environment orchestrator SHALL configure the descriptor handler URL properties to point at the local HTTP server and SHALL disable unrelated subsystems (Matter, Zigbee) for fast startup.

#### Scenario: Orchestrator sets descriptor URL properties
- **WHEN** the orchestrator starts the BCore client
- **THEN** the allowlist URL property SHALL point to the local HTTP server's AllowList.xml
- **AND** the denylist URL property SHALL point to the local HTTP server's DenyList.xml (when provided)

#### Scenario: Orchestrator disables unrelated subsystems
- **WHEN** the orchestrator configures BCore for descriptor testing
- **THEN** Matter and Zigbee subsystems SHALL be disabled to reduce startup time and avoid unrelated failures

### Requirement: Configurable backoff delay parameters
The descriptor handler SHALL read retry delay parameters from the property provider, falling back to the current compile-time defaults when no property is set.

#### Scenario: Default delays preserved when no property set
- **WHEN** no override properties are set for descriptor handler delays
- **THEN** the exponential backoff base SHALL be 2 seconds
- **AND** the incremental backoff init SHALL be 15 seconds

#### Scenario: Test-configured fast retries
- **WHEN** the delay properties are set to 1 second via the property provider
- **THEN** retry delays SHALL use 1 second as the base/init value

### Requirement: Condition-based test synchronization
All test synchronization SHALL use condition-based waits with timeouts rather than fixed sleeps. Tests use `READY_FOR_PAIRING` status events, HTTP server request count waits (`wait_for_request_count`), and inotify-based file content waits (`wait_for_file_content`) as synchronization primitives.

#### Scenario: Wait for descriptor readiness
- **WHEN** the orchestrator's `wait_for_ready_for_pairing` is called with a timeout
- **AND** the READY_FOR_PAIRING status event fires within the timeout
- **THEN** it SHALL return True

#### Scenario: Wait for HTTP request count
- **WHEN** `wait_for_request_count` is called with a target count and timeout
- **AND** the count is not reached within the timeout
- **THEN** it SHALL return False

### Requirement: Integration test — initial allowlist download
An integration test SHALL verify that when BCore starts with a valid allowlist URL, the descriptor handler downloads the file, computes its MD5 checksum, and stores the checksum as a system property.

#### Scenario: First-time allowlist download
- **GIVEN** a local HTTP server serving AllowList.xml
- **AND** a clean BCore configuration (no prior descriptor state)
- **WHEN** BCore starts
- **THEN** the allowlist file SHALL be downloaded (verified by HTTP server request count)

### Requirement: Integration test — update detection on URL change
An integration test SHALL verify that when the allowlist URL is changed at runtime, the descriptor handler detects the change, downloads the new file, and updates the stored checksum.

#### Scenario: URL change triggers re-download
- **GIVEN** BCore is running with an allowlist already downloaded
- **WHEN** the allowlist URL property is changed to point at a different file (or a modified version)
- **THEN** the descriptor handler SHALL download the new file (verified by HTTP server request count)

### Requirement: Integration test — no re-download when file is unchanged
An integration test SHALL verify that when the descriptor handler re-checks an already-downloaded allowlist and the file's MD5 matches the stored checksum, no re-download occurs.

#### Scenario: MD5 match prevents re-download
- **GIVEN** BCore is running with an allowlist already downloaded and checksummed
- **WHEN** a descriptor update is triggered (e.g., property change that cycles back to the same URL)
- **THEN** the stored MD5 system property SHALL remain unchanged
- **AND** the file SHALL NOT be re-downloaded (verified by HTTP server request count remaining at 1)

### Requirement: Integration test — re-download on local file tampering
An integration test SHALL verify that when the local allowlist file is modified on disk (simulating corruption), the descriptor handler detects the MD5 mismatch and re-downloads the file.

#### Scenario: Local file corruption triggers re-download
- **GIVEN** BCore is running with an allowlist already downloaded and checksummed
- **WHEN** the local allowlist file is overwritten with different content
- **AND** a descriptor update is triggered
- **THEN** the server request count SHALL increase, confirming a re-download occurred
- **AND** the stored MD5 system property SHALL match the original served file's checksum

### Requirement: Native C log capture in forked pytest tests
C-level log output (from `icLog` debug backend writing to stdout) SHALL be captured and displayed in pytest output when tests run under `pytest-forked`.

#### Scenario: C log output visible for passing forked tests
- **GIVEN** pytest is configured with `--forked --capture=sys`
- **AND** `conftest.py` monkey-patches `forked_run_report` to emit child stdout/stderr
- **WHEN** a forked test passes and the C code emits log messages via `icLog`
- **THEN** those messages SHALL appear in the test output

### Requirement: Integration test — denylist download
An integration test SHALL verify that the denylist URL triggers the same download-and-checksum lifecycle as the allowlist.

#### Scenario: Denylist download and checksum storage
- **GIVEN** a local HTTP server serving DenyList.xml
- **AND** a clean BCore configuration
- **WHEN** BCore starts with a valid denylist URL
- **THEN** the denylist file SHALL be downloaded (verified by HTTP server request count)
