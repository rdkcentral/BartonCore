## Context

The `xhCrypto` library (`libs/crypto/c/`) contains five source files. Investigation shows that only `digest.c` (file hashing) is consumed by the project — the remaining files (`verifySignature.c`, `compat.c`, `util.c`, `cryptoPrivate.h`) are dead code with zero callers. The sole consumer is the `core` CMake target, which includes `icCrypto/digest.h` from three source files:

- `core/src/deviceDescriptorHandler.c` — 3 call sites: MD5 checksums for allowlist, denylist, and update-avoidance comparison
- `core/deviceDrivers/zigbee/zigbeeDriverCommon.c` — 1 call site: MD5 checksum validation for Zigbee firmware
- `core/test/src/deviceDescriptorsAvailabilityTest.c` — `__wrap_digestFile` mock (returns NULL, never exercises real hashing)

The device descriptor update lifecycle (download → checksum → store → compare on next run) has no integration test coverage. The existing unit test mocks `digestFile` to return NULL, bypassing the hashing entirely. This means we have no regression safety net for the checksum-dependent code paths.

```
┌─────────────────────────────────────────────────────────────────┐
│                       BEFORE                                     │
│                                                                  │
│  deviceDescriptorHandler.c ──┐                                  │
│  zigbeeDriverCommon.c ───────┤                                  │
│  (test mock) ────────────────┘                                  │
│              │                                                   │
│              ▼                                                   │
│     ┌──────────────────┐      ┌───────────┐                    │
│     │    xhCrypto       │─────▶│  OpenSSL  │                    │
│     │ digest.h/digest.c │      │  EVP API  │                    │
│     │ verifySignature.c │      └───────────┘                    │
│     │ compat.c, util.c  │  ← all dead code                     │
│     └──────────────────┘                                        │
│                                                                  │
│                       AFTER                                      │
│                                                                  │
│  deviceDescriptorHandler.c ──┐                                  │
│  zigbeeDriverCommon.c ───────┘                                  │
│              │                                                   │
│              ▼                                                   │
│     ┌──────────────────────────────┐                            │
│     │  deviceServiceHash.h/.c      │── GLib GChecksum           │
│     │  + deviceServiceHashTest.c   │   (fread/ferror wraps)     │
│     └──────────────────────────────┘                            │
│  (test mock removed)                                             │
│                                                                  │
│     libs/crypto/ ── DELETED                                     │
└─────────────────────────────────────────────────────────────────┘
```

## Goals / Non-Goals

**Goals:**
- Add integration tests for the device descriptor download and update lifecycle
- Make descriptor handler retry delays configurable for fast test execution
- Replace `digestFile`/`digestFileHex` calls with a focused `deviceServiceHash` module using GLib `GChecksum`
- Delete the entire `xhCrypto` library (all contents are dead code or replaced)
- Structure as two independent commits: tests first, then replacement

**Non-Goals:**
- Replacing OpenSSL usage elsewhere in the project
- Changing the public API (`api/c/`) layer
- Adding integration coverage for Zigbee firmware checksum validation
- Thread safety changes — current usage is single-threaded per call site

## Decisions

### 1. Extract a focused `deviceServiceHash` unit rather than inlining at each call site

**Decision**: A `deviceServiceHash` module (`core/src/deviceServiceHash.h` and `.c`) provides `deviceServiceHashComputeFileMd5HexString(const char *filePath)`. All 4 call sites invoke this single function. A dedicated cmocka unit test file (`core/test/src/deviceServiceHashTest.c`) exercises the happy path (empty file, text, binary, large/multi-buffer), error paths (nonexistent file, mid-read `ferror`, partial read then `ferror`), and determinism. The test uses `--wrap=fread,ferror` to inject I/O faults via cmocka `will_return`/`mock_type`.

**Rationale**: 4 identical ~15-line blocks (fopen, GChecksum new, fread loop, ferror check, get_string, cleanup) are a maintenance hazard — any bug fix (e.g., adding `ferror` checks) must be replicated identically at every site. A single function with its own unit tests centralizes the logic and makes error-path coverage straightforward. The module-namespaced function name (`deviceServiceHash` prefix) leaves room for future expansion (other algorithms, data hashing, digest output).

**Alternative considered**: Inline at each site. Initially implemented, but the duplicated error handling made review feedback harder to apply consistently.

### 2. Delete `xhCrypto` entirely rather than keeping the shell

**Decision**: Remove the entire `libs/crypto/` directory.

**Rationale**: `verifySignature.h` has zero includes across the project, `util.c`/`cryptoPrivate.h` are internal-only helpers for the unused signature code, and `compat.c` provides OpenSSL init shims only needed by the deleted code. Keeping an empty library serves no purpose.

### 3. Integration tests exercise the descriptor lifecycle via public API

**Decision**: Tests use `BCore.Client` (via GObject Introspection/Python), `b_core_client_get_system_property` for assertion, and `STATUS_CHANGED` with `READY_FOR_PAIRING` for synchronization. Additionally, the `FixtureHTTPServer` tracks request counts per path, enabling assertions that verify download behavior without polling properties (e.g., confirming no re-download by checking request count stays at 1).

**Rationale**: The public API already provides all needed observability. `get_system_property` can read the stored MD5 and URL. The `STATUS_CHANGED` signal with `READY_FOR_PAIRING` reason fires when descriptors become valid. Property provider changes trigger the `propertyChangedHandler` in `deviceEventHandler.c` which calls `deviceDescriptorsUpdateAllowlist`/`Denylist`. Server request counts provide a direct, non-polling way to assert download behavior. No new C API needed.

### 4. Local HTTP server and DeviceDescriptorServer mock

**Decision**: A `FixtureHTTPServer` spins up Python's `http.server` on a random port with thread-safe request tracking. A `DeviceDescriptorServer` mock wraps it to serve AllowList.xml and DenyList.xml from `testing/resources/deviceDescriptor/`, supports `request.param` indirect parametrize for custom file paths, and exposes an `allowlist_content` property for raw test data access.

**Rationale**: The descriptor handler uses `urlHelperDownloadFile` (libcurl). A local HTTP server exercises the full download → validate → checksum → store path. Test resources live in `testing/resources/deviceDescriptor/` (the `testing/test/resources/` subtree is reserved for per-test overrides). The `allowlist_content` property lets tests access served file bytes without hard-coding paths.

### 5. Configurable backoff delays via property provider

**Decision**: Read the exponential delay base and incremental init delay from the property provider, falling back to current compile-time defaults.

**Rationale**: Both repeating task policies execute the first attempt immediately (delay=0). Retries use exponential (base 2s) or incremental (init 15s) backoff. For retry-path tests, the 2s/15s delays are unacceptable. Making them configurable via property provider allows tests to set 1-second retries while production behavior is unchanged.

### 6. Condition-based waits only — no sleeps

**Decision**: All test synchronization uses condition-based waits with timeouts — `READY_FOR_PAIRING` status events, HTTP server request count waits, and inotify-based file content waits. No fixed sleeps.

**Rationale**: Sleeps make tests slow and flaky. The system produces observable state changes (system properties, STATUS_CHANGED signals) that condition waits can react to immediately.

### 7. Remove `__wrap_digestFile` from unit test

**Decision**: After inlining GLib calls, there is no `digestFile` symbol to wrap. Remove it from the wrapped functions list and delete the mock function body.

**Rationale**: The mock currently returns NULL — it never tested real hashing. With inline GLib calls the wrapping is no longer applicable. The existing test continues to function because it mocks at higher levels (property provider, URL helper) and the hashing result was never asserted.

### 8. Fix native C log capture in forked pytest tests

**Decision**: Add `--capture=sys` to pytest addopts (replacing default `--capture=fd`) and monkey-patch `pytest_forked.forked_run_report` in `conftest.py` to write child process stdout/stderr to the parent's stderr.

**Rationale**: `icLog`'s debug backend writes to stdout (fd 1) via `fprintf`, but `pytest-forked`'s `ForkedFunc` redirects fd 1 to temp files and discards the output for passing tests. pytest's default `--capture=fd` mode does a second `dup2` that overrides ForkedFunc's redirect entirely. Switching to `--capture=sys` prevents pytest from overriding the fd-level redirect, and the monkey-patch ensures captured C output is emitted to the parent's stderr instead of being discarded.

### 9. Add 5th integration test: redownload on local file tampering

**Decision**: A test verifies that when the local allowlist file is modified on disk (simulating corruption/tampering), the descriptor handler detects the MD5 mismatch and re-downloads the file.

**Rationale**: The `fileNeedsUpdating` code path computes MD5 of the local file and compares to the stored checksum. If they differ, a re-download is triggered even though the URL hasn't changed. This is critical safety behavior that should be covered.

## Risks / Trade-offs

- **[Risk] Inlined code duplication** — 4 copies of the GChecksum pattern → **Mitigation**: Each is ~10 lines, all doing MD5 only. If a 5th site appears, reconsider extraction.

- **[Risk] Integration test HTTP server port collision** — Random port could collide → **Mitigation**: Use port 0 (OS-assigned) and pass the port to the property URL.

- **[Risk] Integration test timing on slow CI** → **Mitigation**: All waits have explicit timeouts with generous bounds (5-10s). First attempt is immediate (delay=0), so happy path completes in <1s.

- **[Risk] Negative assertion ("no re-download") requires timeout** — Can't observe a non-event without waiting → **Mitigation**: Use server request count assertions with `wait_for_request_count`. If count doesn't increase within the timeout, the test passes. This is inherent to testing idempotency.

## Open Questions

None — all decisions are settled.
