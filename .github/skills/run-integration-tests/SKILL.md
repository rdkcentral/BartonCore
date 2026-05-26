---
name: run-integration-tests
description: Run BartonCore Python integration tests. Use when the user wants to run integration tests, verify end-to-end behavior, run specific test files, or filter tests by keyword or marker. Covers pytest via the py_test.sh wrapper, ASAN preloading, test filtering, prerequisites, and the CI all-in-one script.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container with Python, pytest, D-Bus, and Node.js pre-installed.
metadata:
  author: rdkcentral
  version: "1.0"
---

# Run Integration Tests

## Quick Start

Run all integration tests:

```bash
./testing/py_test.sh testing/
```

The `py_test.sh` wrapper handles Address Sanitizer (ASAN) `LD_PRELOAD` setup automatically. Always use it instead of calling `pytest` directly.

## Prerequisites

Integration tests require several services and artifacts to be in place:

1. **Build the project**: `cmake --build build`
2. **Install build artifacts**: `cmake --build build --target install`
3. **Start D-Bus**: `sudo service dbus start`
4. **Install matter.js dependencies** (for tests with `requires_matterjs` marker): `npm --prefix testing/mocks/devices/matterjs ci`

### All-in-One Alternative

The CI script handles all prerequisites automatically:

```bash
./scripts/ci/run_integration_tests.sh
```

This starts D-Bus, installs build artifacts, installs matter.js npm dependencies, and then runs all tests.

## Run Specific Tests

### By file path

```bash
./testing/py_test.sh testing/test/light_test.py
```

### By keyword (`-k`)

```bash
./testing/py_test.sh testing/ -k "light"
./testing/py_test.sh testing/ -k "commission and matter"
```

### By marker (`-m`)

```bash
./testing/py_test.sh testing/ -m "requires_matterjs"
```

### With extra verbosity

```bash
./testing/py_test.sh testing/test/light_test.py -s --log-cli-level=DEBUG
```

The `-s` flag disables output capture so you can see print statements and logs in real time.

## Test Locations

| Item | Location |
|------|----------|
| Test files | `testing/test/` |
| Test fixtures and helpers | `testing/helpers/`, `testing/mocks/`, `testing/utils/` |
| Pytest configuration | `pyproject.toml` (section `[tool.pytest.ini_options]`) |
| Pytest plugins/fixtures | `testing/conftest.py` |
| Matter device mocks | `testing/mocks/devices/matter/` |
| matter.js virtual devices | `testing/mocks/devices/matterjs/` |

## Pytest Configuration

From `pyproject.toml`:

```toml
[tool.pytest.ini_options]
testpaths = ["testing"]
addopts = "--capture=sys"
log_cli = true
log_cli_level = "DEBUG"
```

## ASAN Preloading

The dev build enables Address Sanitizer. `py_test.sh` automatically finds and preloads `libasan.so` so that ASAN-instrumented shared libraries work correctly when loaded by Python via GObject Introspection. Do not bypass this wrapper.

## Error Recovery

If `pytest` or `py_test.sh` fails with a command-not-found error, import error, or missing shared library:

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"Integration test dependencies are not available. Please run inside the BartonCore development container."**
3. If it does exist, check that prerequisites have been completed (build, install, dbus, npm ci)
