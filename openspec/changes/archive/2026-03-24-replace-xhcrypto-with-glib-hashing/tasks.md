## Commit 1 — Device descriptor integration tests

### 1. Local HTTP server pytest fixture

- [x] 1.1 Create `testing/helpers/http_fixture_server.py` — a pytest fixture that starts Python's `http.server.HTTPServer` on port 0, serves files from a configurable directory, tracks request counts per path, and exposes the assigned port. Stops on teardown.
- [x] 1.2 Create `testing/mocks/device_descriptor_server.py` — a `DeviceDescriptorServer` wrapping `FixtureHTTPServer` that serves AllowList.xml and DenyList.xml from `testing/resources/deviceDescriptor/`, supports `request.param` indirect parametrize for custom file paths, and exposes an `allowlist_content` property.

### 2. Descriptor test environment orchestrator

- [x] 2.1 Create `testing/environment/descriptor_environment_orchestrator.py` — inherits from `BaseEnvironmentOrchestrator`, configures allowlist/denylist URL properties to point at the local HTTP server, disables Matter and Zigbee subsystems.
- [x] 2.2 Wire the HTTP fixture port into the orchestrator's URL properties.

### 3. Configurable backoff delay parameters

- [x] 3.1 In `core/src/deviceDescriptorHandler.c`, read the exponential delay base (currently `BASE_DD_EXPONENTIAL_DELAY_SECONDS = 2`) from the property provider, falling back to the compile-time default.
- [x] 3.2 In `core/src/deviceDescriptorHandler.c`, read the incremental delay init (currently `INIT_DD_TASK_WAIT_TIME_SECONDS = 15`) from the property provider, falling back to the compile-time default.
- [x] 3.3 Create property names for these overrides (e.g., `CPE_DD_EXPONENTIAL_DELAY_SECS`, `CPE_DD_INCREMENTAL_DELAY_SECS`) and document their purpose in nearby code comments.

### 4. Condition-based wait utility

- [x] 4.1 ~~Create `testing/utils/wait_for_property.py`~~ — Not needed as a separate utility. Tests use `DescriptorEnvironmentOrchestrator.wait_for_ready_for_pairing()` and `FixtureHTTPServer.wait_for_request_count()` for condition-based waits instead.

### 5. Integration tests

- [x] 5.1 Create `testing/test/device_descriptor_test.py` containing:
  - [x] 5.1.1 Test: initial allowlist download — assert stored MD5 matches expected checksum of AllowList.xml
  - [x] 5.1.2 Test: denylist download — assert stored denylist MD5 matches expected checksum of DenyList.xml
  - [x] 5.1.3 Test: update detection on URL change — change URL property at runtime, assert MD5 property updates
  - [x] 5.1.4 Test: no re-download when file is unchanged — trigger re-check, assert MD5 unchanged and verify via server request count
  - [x] 5.1.5 Test: re-download on local file tampering — corrupt local allowlist file, trigger re-check, assert server request count increases confirming re-download
- [x] 5.2 Run integration tests and verify all 5 tests pass

### 5b. Native C log capture fix

- [x] 5b.1 Add `--capture=sys` to pytest addopts in `pyproject.toml` (prevents pytest fd capture from overriding ForkedFunc's fd redirect)
- [x] 5b.2 In `testing/conftest.py`, add `pytest_configure` hook that monkey-patches `forked_run_report` to write child process `result.out`/`result.err` to parent's stderr
- [x] 5b.3 Verify native C log output (e.g., "md5 mismatch" from icLog) appears in test output

### 6. Commit 1 cleanup

- [x] 6.1 Run `clang-format` from repo root on modified C files (deviceDescriptorHandler.c)
- [x] 6.2 Verify blank line style rules in modified C code
- [x] 6.3 Commit with message following project conventions

## Commit 2 — Replace xhCrypto hashing with inline GLib calls

### 7. Create deviceServiceHash module and replace call sites

- [x] 7.1 Create `core/src/deviceServiceHash.h` declaring `deviceServiceHashComputeFileMd5HexString(const char *filePath)`.
- [x] 7.2 Create `core/src/deviceServiceHash.c` implementing the function using GLib `GChecksum` with fopen/fread-loop/ferror/fclose.
- [x] 7.3 In `core/src/deviceDescriptorHandler.c`, replace the 3 `digestFileHex` calls with `deviceServiceHashComputeFileMd5HexString`. Add `#include "deviceServiceHash.h"`.
- [x] 7.4 In `core/deviceDrivers/zigbee/zigbeeDriverCommon.c`, replace the 1 `digestFileHex` call in `validateMD5Checksum` with `deviceServiceHashComputeFileMd5HexString`. Add `#include "deviceServiceHash.h"`.
- [x] 7.5 Remove `#include <icCrypto/digest.h>` from both files.
- [x] 7.6 Create `core/test/src/deviceServiceHashTest.c` with cmocka unit tests covering: known digest, text, binary, large file, nonexistent file, deterministic reads, mid-read ferror, and partial-read-then-ferror.
- [x] 7.7 Add `testDeviceServiceHash` target to `core/test/CMakeLists.txt` with `WRAPPED_FUNCTIONS fread ferror` and `LINK_LIBRARIES BartonCommon::xhLog`.

### 8. Update unit tests

- [x] 8.1 In `core/test/src/deviceDescriptorsAvailabilityTest.c`, remove the `__wrap_digestFile` function body.
- [x] 8.2 In `core/test/CMakeLists.txt`, remove `digestFile` from the `WRAPPED_FUNCTIONS` list for the device descriptors test target.

### 9. Delete xhCrypto library

- [x] 9.1 Delete the entire `libs/crypto/` directory.
- [x] 9.2 In `libs/CMakeLists.txt`, remove the `add_subdirectory(crypto)` line.
- [x] 9.3 In `core/CMakeLists.txt`, remove `xhCrypto` from `target_link_libraries`.

### 10. Build, test, and format

- [x] 10.1 Build the project and verify zero compile/link errors.
- [x] 10.2 Run unit tests (`ctest --output-on-failure`) — all must pass.
- [x] 10.3 Run integration tests — all descriptor tests from Commit 1 must still pass.
- [x] 10.4 Run `clang-format` from repo root on all modified C files.
- [x] 10.5 Verify blank line style rules in all modified C code.
- [x] 10.6 Commit with message following project conventions.
