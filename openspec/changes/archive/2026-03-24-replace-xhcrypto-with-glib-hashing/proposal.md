## Why

The `xhCrypto` library (`libs/crypto/c/`) depends on OpenSSL solely for file hashing via `digestFile`/`digestFileHex`. Investigation reveals that hashing is the **only** part of `xhCrypto` consumed by the project — `verifySignature.h`, `util.c`, `compat.c`, and `cryptoPrivate.h` are all dead code with no callers. GLib (already a mandatory dependency at 2.62+) provides `GChecksum`, a built-in hashing API that supports the same algorithms. Rather than swapping backends within the library, we can delete `xhCrypto` entirely and inline GLib `GChecksum` calls directly at the 4 call sites (3 in `deviceDescriptorHandler.c`, 1 in `zigbeeDriverCommon.c`).

Additionally, the device descriptor update lifecycle — including download, checksum storage, and update-avoidance via MD5 comparison — currently has no integration test coverage. This change adds integration tests for that feature area first, then performs the hashing replacement.

This change is structured as **two independent commits**:
1. **Commit 1**: Add device descriptor integration tests (exercising the existing hashing code)
2. **Commit 2**: Extract a `deviceServiceHash` module with `deviceServiceHashComputeFileMd5HexString`, replace all `digestFile`/`digestFileHex` calls with it, add unit tests with `fread`/`ferror` fault injection, delete `xhCrypto`

## What Changes

### Commit 1 — Device descriptor integration tests
- Add a local HTTP server pytest fixture to serve AllowList.xml and DenyList.xml test fixtures
- Add a `DeviceDescriptorServer` mock that manages fixture serving and exposes `allowlist_content` for test assertions
- Add a descriptor-focused test environment orchestrator that configures descriptor URLs and disables Matter/Zigbee subsystems
- Add integration tests covering: initial allowlist download, denylist download, update detection on URL change, no-op when file is up-to-date, and re-download when local file is tampered with
- Make descriptor handler backoff delay parameters configurable via property provider (with existing defaults preserved) so tests can use fast retries
- Use READY_FOR_PAIRING status events, HTTP request-count waits, and inotify-based file content waits for test synchronization (no sleeps)
- Fix native C log capture in forked pytest tests (`--capture=sys` + monkey-patch of `forked_run_report`)
- Move test resource files to `testing/resources/deviceDescriptor/`

### Commit 2 — Replace xhCrypto hashing with `deviceServiceHash` module
- Create `core/src/deviceServiceHash.h` and `core/src/deviceServiceHash.c` providing `deviceServiceHashComputeFileMd5HexString(const char *filePath)` using GLib `GChecksum`
- Replace all `digestFileHex` calls in `deviceDescriptorHandler.c` (3 sites) and `zigbeeDriverCommon.c` (1 site) with `deviceServiceHashComputeFileMd5HexString`
- Add `core/test/src/deviceServiceHashTest.c` with cmocka unit tests: known digest, text, binary, large file, nonexistent file, deterministic reads, mid-read ferror, and partial-read-then-ferror (using `--wrap=fread,ferror` fault injection)
- Remove `#include <icCrypto/digest.h>` from all callers
- Remove `__wrap_digestFile` from `deviceDescriptorsAvailabilityTest.c` and its CMake wrapped functions list
- Delete the entire `libs/crypto/` directory (all files are dead code or replaced)
- Remove `xhCrypto` from `core/CMakeLists.txt` link libraries
- Remove `add_subdirectory` for crypto from `libs/CMakeLists.txt`

## Non-goals

- Replacing OpenSSL usage elsewhere in the project (Matter SDK, etc.)
- Changing hashing algorithms used by callers (MD5 remains where MD5 is currently used)
- Modifying the `api/c/` public API layer
- Adding integration test coverage for Zigbee firmware checksum validation (separate concern)

## Capabilities

### New Capabilities
- `descriptor-integration-tests`: Integration tests for the device descriptor download and update lifecycle, native C log capture fix for forked pytest tests
- `inline-glib-hashing`: Extract `deviceServiceHash` module using GLib `GChecksum`, replace xhCrypto digest calls, add unit tests with fault injection, and delete the xhCrypto library

### Modified Capabilities

## Impact

- **Libraries layer** (`libs/crypto/`): Entire directory deleted — all contents are dead code or replaced by `deviceServiceHash`
- **Core layer** (`core/src/`): New `deviceServiceHash.h`/`.c` module. `deviceDescriptorHandler.c` and `zigbeeDriverCommon.c` call `deviceServiceHashComputeFileMd5HexString`, lose `icCrypto/digest.h` include
- **Core layer** (`core/src/deviceDescriptorHandler.c`): Backoff delay parameters become configurable via property provider
- **Unit tests** (`core/test/`): New `deviceServiceHashTest.c` with `--wrap=fread,ferror` fault injection. `deviceDescriptorsAvailabilityTest.c` removes `digestFile` from wrapped functions
- **Integration tests** (`testing/`): New test module, environment orchestrator, HTTP fixture, and utility
- **Build** (`core/CMakeLists.txt`, `libs/CMakeLists.txt`): `xhCrypto` removed from link targets and subdirectory list
- **CMake flags**: No new feature flags required
