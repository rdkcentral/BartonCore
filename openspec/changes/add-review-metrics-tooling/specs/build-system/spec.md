## MODIFIED Requirements

### Requirement: Stack protection and sanitizer support
The build system SHALL support optional stack smash protection (`BCORE_BUILD_WITH_SSP`) adding `-fstack-protector-strong`, and AddressSanitizer (`BCORE_BUILD_WITH_ASAN`) adding `-fsanitize=address` to both compiler and linker flags.

#### Scenario: Enable ASAN
- **WHEN** `BCORE_BUILD_WITH_ASAN=ON`
- **THEN** `-fsanitize=address` SHALL be added to C flags, C++ flags, shared linker flags, and EXE linker flags

#### Scenario: Enable UBSan
- **WHEN** `BCORE_BUILD_WITH_UBSAN=ON`
- **THEN** `-fsanitize=undefined` SHALL be added to C flags, C++ flags, shared linker flags, and EXE linker flags

#### Scenario: Enable TSan
- **WHEN** `BCORE_BUILD_WITH_TSAN=ON` and `BCORE_BUILD_WITH_ASAN=OFF`
- **THEN** `-fsanitize=thread` SHALL be added to C flags, C++ flags, shared linker flags, and EXE linker flags

#### Scenario: TSan with ASAN rejected
- **WHEN** `BCORE_BUILD_WITH_TSAN=ON` and `BCORE_BUILD_WITH_ASAN=ON`
- **THEN** CMake configuration SHALL fail with an error

#### Scenario: New sanitizers default off
- **WHEN** the project is configured without setting `BCORE_BUILD_WITH_UBSAN` or `BCORE_BUILD_WITH_TSAN`
- **THEN** both SHALL be OFF

## ADDED Requirements

### Requirement: Compile command export
The build system SHALL always generate `compile_commands.json` in the build directory.

#### Scenario: Compile database present
- **WHEN** the project is configured
- **THEN** `compile_commands.json` SHALL exist in the binary directory

### Requirement: Standard compiler warnings
When the private, temporary option `BCORE_EXTRA_WARNINGS` is ON, first-party targets SHALL compile with `-Wall -Wextra`; third-party code SHALL NOT receive these flags from BartonCore. The option SHALL default to OFF so external consumers that inject `-Werror` are unaffected, and the dev platform file (and therefore every preset) SHALL enable it.

#### Scenario: Warnings on core sources
- **WHEN** a file under `core/` is compiled in a `dev`-based build
- **THEN** its compile command SHALL include `-Wall` and `-Wextra`

#### Scenario: External consumer build
- **WHEN** BartonCore is configured without the dev platform file and without `BCORE_EXTRA_WARNINGS`
- **THEN** first-party compile commands SHALL NOT gain `-Wall` or `-Wextra` from BartonCore

### Requirement: Warnings as errors option
The build system SHALL provide `BCORE_WERROR` (default OFF) that adds `-Werror` to first-party targets only.

#### Scenario: Default does not fail on warnings
- **WHEN** `BCORE_WERROR` is not set
- **THEN** compiler warnings SHALL NOT fail the build

#### Scenario: Werror enabled
- **WHEN** `BCORE_WERROR=ON`
- **THEN** first-party compile commands SHALL include `-Werror` and third-party compile commands SHALL NOT

### Requirement: cppcheck target
When cppcheck is available, the build system SHALL provide a `cppcheck` target that analyzes first-party sources from `compile_commands.json` and writes XML to `<build-dir>/cppcheck.xml`, which `scripts/quality/cppcheck.sh` copies into its report directory.

#### Scenario: Run cppcheck target
- **WHEN** `cmake --build <dir> --target cppcheck` runs in the dev container
- **THEN** `<dir>/cppcheck.xml` SHALL be written, any previous report SHALL be removed first, and `third_party/` SHALL be excluded

#### Scenario: Restrict cppcheck to selected sources
- **WHEN** `BCORE_CPPCHECK_ONLY` names a file listing source paths and the `cppcheck` target runs
- **THEN** only the listed translation units SHALL be analyzed

#### Scenario: Changed-files cppcheck check
- **WHEN** `scripts/quality/cppcheck.sh` runs without `--full` or `--update-baseline`
- **THEN** it SHALL analyze only C/C++ sources changed since the merge base with `QUALITY_BASE_REF` (default `origin/main`) plus sources including a changed header, and SHALL report `blocked` if the merge base cannot be resolved

### Requirement: CMake presets
The repository SHALL provide `CMakePresets.json` (presets schema version 1, requiring CMake ≥ 3.19) with configure presets `dev-linux`, `dev` (inheriting `dev-linux` unchanged), and `coverage`, `asan`, `ubsan`, `tsan`, and `tidy` inheriting `dev`. `dev-linux` SHALL load `config/cmake/platforms/dev/linux.cmake` by setting the cache variable `CMAKE_PROJECT_barton-core_INCLUDE` to that file rather than duplicating its values. The top-level `CMakeLists.txt` SHALL NOT reference platforms or presets, the project's minimum CMake version SHALL remain 3.16.5, and `build.sh` SHALL remain supported.

#### Scenario: Dev preset matches build.sh
- **WHEN** `cmake --preset dev` runs
- **THEN** `CMAKE_BUILD_TYPE` SHALL be `Debug` and the cache values set by `config/cmake/platforms/dev/linux.cmake` (including `BCORE_BUILD_WITH_ASAN` and `BCORE_BUILD_WITH_SSP` ON) SHALL match a `./build.sh` configuration

#### Scenario: Platform file scoped to the top-level project
- **WHEN** a FetchContent subproject such as BartonCommon calls `project()` during a preset configure
- **THEN** `config/cmake/platforms/dev/linux.cmake` SHALL NOT be included again

#### Scenario: Sanitizer preset
- **WHEN** `cmake --preset tsan` runs
- **THEN** `BCORE_BUILD_WITH_TSAN` SHALL be ON and `BCORE_BUILD_WITH_ASAN` SHALL be OFF

#### Scenario: Tidy preset
- **WHEN** `cmake --preset tidy` runs
- **THEN** clang-tidy SHALL run on each first-party C and C++ source during the build

### Requirement: Configurable Matter install directory
The build SHALL expose a `BCORE_MATTER_INSTALL_DIR` cache path (default `${CMAKE_BINARY_DIR}/matter-install`) used for the Matter SDK prefix path, include and link directories, and as the install target passed to `build-matter.sh -i`. Presets SHALL point ASAN builds (`dev-linux`, `dev`, `asan`) at the shared `build/matter-install` used by `build.sh`, and non-ASAN presets (`coverage`, `ubsan`, `tsan`, `tidy`) at a shared `build/matter-noasan/matter-install`.

#### Scenario: Default unchanged
- **WHEN** `./build.sh` configures without setting `BCORE_MATTER_INSTALL_DIR`
- **THEN** Matter SHALL be built into and found at `build/matter-install` as before

#### Scenario: Preset reuses shared Matter
- **WHEN** `cmake --preset dev` runs after `./build.sh` has built Matter
- **THEN** configure SHALL find Matter in `build/matter-install` without rebuilding it

### Requirement: Quality tools Docker stage
`docker/Dockerfile` SHALL define a `quality_tools` stage from which `dev_builder` inherits. The main Docker build context SHALL remain `docker/`; `docker/compose.yaml` SHALL provide the repo root as a named build context `repo`, and a root `.dockerignore` SHALL exclude everything except `pyproject.toml`.

#### Scenario: Stage inheritance
- **WHEN** the `dev_builder` image is built
- **THEN** it SHALL include every tool installed in `quality_tools`

#### Scenario: Repo named context
- **WHEN** the image is built via `docker/build.sh` or `./dockerw`
- **THEN** the `quality_tools` stage SHALL read `pyproject.toml` from the `repo` named context via `RUN --mount=from=repo` without copying other repository files
