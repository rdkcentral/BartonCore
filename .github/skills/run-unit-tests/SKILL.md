---
name: run-unit-tests
description: Run BartonCore C/C++ unit tests. Use when the user wants to run unit tests, verify code changes pass tests, run a specific test, or list available tests. Covers ctest commands, test filtering, CMocka (C) and Google Test (C++) frameworks, and test source locations.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container and a completed build.
metadata:
  author: rdkcentral
  version: "1.0"
---

# Run Unit Tests

## Prerequisites

The project must be built before running tests. If not yet built, run:

```bash
cmake --build build
```

## Run All Unit Tests

```bash
ctest --output-on-failure --test-dir build
```

This runs all registered unit tests and prints output only for failing tests.

## Run Specific Tests

Filter by name pattern:

```bash
ctest -R <pattern> --output-on-failure --test-dir build
```

Examples:

```bash
ctest -R deviceService --output-on-failure --test-dir build
ctest -R matter --output-on-failure --test-dir build
ctest -R "sbmd.*validator" --output-on-failure --test-dir build
```

## List Available Tests

```bash
ctest -N --test-dir build
```

This lists all test names without running them. Useful for finding the right filter pattern.

## Test Frameworks

BartonCore uses two unit test frameworks:

- **CMocka** — for C tests. Defined in CMakeLists.txt using `bcore_add_cmocka_test()`.
- **Google Test / Google Mock** — for C++ tests. Defined using `bcore_add_cpp_test()` with auto-discovery via `gtest_discover_tests()`.

## Test Source Locations

| Area | Location |
|------|----------|
| Core library tests | `core/test/` |
| Device library tests | `libs/device/test/` |
| C API tests | `api/c/test/` |

Test files are typically prefixed with `test` or suffixed with `Test`.

## Verbose Output

For more detail on all tests (not just failures):

```bash
ctest --output-on-failure --test-dir build -V
```

## Error Recovery

If `ctest` fails with a command-not-found error:

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"Build/test tools are not available. Please run inside the BartonCore development container."**
3. If it does exist but tests fail to run, ensure the project has been built first with `cmake --build build`
