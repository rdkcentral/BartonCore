---
name: build
description: Build BartonCore from source. Use when the user wants to compile the project, rebuild after code changes, reconfigure CMake options, or understand the build system. Covers the build.sh convenience script, incremental cmake builds, CMake configuration flags, and the development build profile.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container (cmake, gcc/g++, GLib, and all build dependencies are pre-installed).
metadata:
  author: rdkcentral
  version: "1.0"
---

# Build BartonCore

## Build Hierarchy

BartonCore has a layered build system. Each layer calls the one below it:

```
build.sh  (top-level convenience script)
  └── cmake  (configure + build)
        └── build-matter.sh  (Matter SDK, invoked automatically by CMake when needed)
```

- `build.sh` is the human-friendly entry point that handles configuration and parallel builds.
- `cmake --build build` is the preferred command for incremental rebuilds after code changes.
- `build-matter.sh` builds the Matter SDK. CMake invokes it automatically — you almost never need to run it manually.

## Incremental Rebuild (most common)

After modifying source files, rebuild with:

```bash
cmake --build build
```

This only recompiles changed files and is fast. Use this as the default build command.

## Full Build From Scratch

For a first-time build or a clean rebuild:

```bash
./build.sh
```

This configures CMake (using the dev profile by default) and builds the entire project in parallel. It creates the `build/` directory if it doesn't exist.

Options:
- `./build.sh -d` — Delete the CMake cache before reconfiguring (clean reconfigure)
- `./build.sh -C <cache-file>` — Use a custom CMake initial-cache file instead of the dev profile
- `./build.sh --no-initial-cache` — Don't use any initial-cache; use default public options

You can append CMake flags directly:

```bash
./build.sh -DBCORE_ZIGBEE=OFF
```

## Development Build Profile

By default, `build.sh` uses `config/cmake/platforms/dev/linux.cmake` which enables:

| Setting | Value |
|---------|-------|
| `CMAKE_BUILD_TYPE` | Debug |
| `BCORE_BUILD_WITH_SSP` | ON (stack smash protection) |
| `BCORE_BUILD_WITH_ASAN` | ON (Address Sanitizer) |
| `BCORE_GEN_GIR` | ON (GObject Introspection) |
| `BCORE_MATTER_USE_RANDOM_PORT` | ON (avoids port 5540 conflicts) |
| `CMAKE_PREFIX_PATH` | `build/matter-install` |

## CMake Configuration Flags

Pass these with `-D` to cmake or appended to `./build.sh`:

### Subsystem Flags
| Flag | Default | Purpose |
|------|---------|---------|
| `BCORE_ZIGBEE` | ON | Enable Zigbee support |
| `BCORE_MATTER` | ON | Enable Matter support |
| `BCORE_THREAD` | ON | Enable Thread/OpenThread support |
| `BCORE_PHILIPS_HUE` | OFF | Enable Philips Hue support |

### Build Option Flags
| Flag | Default | Purpose |
|------|---------|---------|
| `BCORE_BUILD_REFERENCE` | ON | Build the reference application |
| `BCORE_BUILD_WITH_SSP` | OFF | Stack smash protection |
| `BCORE_BUILD_WITH_ASAN` | OFF | Address Sanitizer |
| `BCORE_GEN_GIR` | ON | Generate GObject Introspection files |
| `BCORE_TEST_COVERAGE` | OFF | Enable code coverage |
| `BCORE_MATTER_VALIDATE_SCHEMAS` | ON | Validate SBMD specs during build |
| `BCORE_MATTER_USE_RANDOM_PORT` | OFF | Use random Matter operational port |
| `BCORE_MATTER_SKIP_SDK` | OFF | Skip building Matter SDK separately |

### Example: Build Without Zigbee

```bash
cmake --build build  # if already configured
# or to reconfigure:
./build.sh -d -DBCORE_ZIGBEE=OFF
```

## Build Output

- Libraries and executables are in `build/`
- Reference app binary: `build/reference/barton-core-reference`
- Matter SDK installation: `build/matter-install/`

## Error Recovery

If a build command fails with a tool-not-found error (e.g., `cmake: command not found`, `make: command not found`, `gcc: command not found`):

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"Build tools are not available. Please run inside the BartonCore development container."**
3. If it does exist, the error is a genuine build problem — read the error output and diagnose
