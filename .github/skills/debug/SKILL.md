---
name: debug
description: Debug BartonCore applications and tests. Use when the user needs to set breakpoints, step through code, inspect state, or diagnose crashes. Covers three workflows — gdb for the C/C++ reference app and unit tests, pdb/debugpy for Python integration tests, and gdb-with-Python for debugging C code called from Python tests.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container with gdb, python3-gdb, and SYS_PTRACE capability.
metadata:
  author: rdkcentral
  version: "1.0"
---

# Debug

Three debugging workflows depending on where the problem is.

## 1. GDB — Reference App and Unit Tests

### Reference App

The binary is at `build/reference/barton-core-reference`.

```bash
gdb --args build/reference/barton-core-reference -b "core/deviceDrivers/matter/sbmd/specs"
```

CLI flags:

| Flag | Short | Description |
|------|-------|-------------|
| `--sbmd-dirs` | `-b` | Semicolon-delimited SBMD spec directories |
| `--noZigbee` | `-z` | Disable Zigbee subsystem |
| `--noThread` | `-t` | Disable Thread subsystem |
| `--noMatter` | `-m` | Disable Matter subsystem |
| `--novt100` | `-1` | Disable interactive linenoise prompt |
| `--storage-dir` | `-d` | Persisted storage directory |
| `--wifi-ssid` | `-s` | Wi-Fi SSID for commissioning |
| `--wifi-password` | `-p` | Wi-Fi password for commissioning |

Common combinations:

```bash
# No Zigbee or Thread (Matter only)
gdb --args build/reference/barton-core-reference -b "core/deviceDrivers/matter/sbmd/specs" -z -t

# No Matter or Thread (Zigbee only)
gdb --args build/reference/barton-core-reference -m -t
```

### Unit Tests

Debug a specific unit test with gdb:

```bash
gdb --args build/core/test/<test-binary>
```

In VS Code, use the Testing panel's debug button or the CMake panel's debug icon on a specific test target.

### VS Code Launch Configs

Pre-configured launch configs in `.vscode/launch.json`:
- **(gdb) Reference App** — launches with `-b` pointing to SBMD specs
- **(gdb) Reference App No Zigbee** — adds `-z`
- **(gdb) Reference App No Thread** — adds `-t`
- **(gdb) CMake Target** — debug any CMake build target
- **(gdb) CMake Test** — debug a specific CTest test

All gdb configs enable pretty-printing.

## 2. PDB / debugpy — Python Integration Tests

For pure Python debugging of integration tests:

```python
# Insert in test code
breakpoint()
```

Then run with output capture disabled:

```bash
./testing/py_test.sh -s --no-header testing/test/<test_file>.py
```

The `-s` flag is required for `breakpoint()` to work (disables output capture).

In VS Code, use the Testing panel's debug button or gutter debug icons on test functions. The **Python Debugger: Current File** launch config works for non-pytest Python files.

**Limitation:** The Python debugger can only step through Python code. To debug into BartonCore C/C++ code called from Python, use workflow 3 below.

## 3. GDB + Python — C Code Called from Python Tests

When you need to set breakpoints in C/C++ code that is invoked from Python integration tests:

```bash
gdb python3
```

At the gdb prompt:

```gdb
# If ASAN is enabled in the build (default for dev builds):
set env LD_PRELOAD /path/to/libasan.so

# Find the ASAN path:
# shell gcc -print-file-name=libasan.so

# Set breakpoints in C code (symbols load after first run):
break deviceServiceStart
# For additional breakpoints, use a real symbol from the current target:
# info functions deviceService
break Matter::Init

# Run the test:
run -m pytest testing/test/<test_file>.py
```

After the first run, C symbols become available for tab-completion. Set breakpoints and re-run as needed.

## Prerequisites

- **Build first**: The project must be built before debugging (`cmake --build build`)
- **Install step for integration tests**: Run the CMake install task before debugging integration tests — it installs the shared library, GIR/typelib, and `.pyi` stubs
- **Docker paths**: Custom `GI_TYPELIB_PATH` and `LD_LIBRARY_PATH` are auto-configured in the dev container via `docker/setupDockerEnv.sh`

## Error Recovery

If `gdb` is not found or reports missing debug symbols:

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"gdb and debug symbols are not available. Please run inside the BartonCore development container."**
3. If it does exist, ensure the build was done with debug symbols (`CMAKE_BUILD_TYPE=Debug`, which is the default dev profile)
