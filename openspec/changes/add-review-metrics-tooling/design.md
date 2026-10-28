# Design: add-review-metrics-tooling (BARTON-415)

Provision the BartonCore dev container and repository with the tools, configuration, baselines, and wrapper scripts the `review-metrics-*` agentic review skills need, without changing default build or test behavior.

## Context

See `proposal.md` (Why) for motivation. Current state relevant to this design:

- `docker/Dockerfile` is a multi-stage Ubuntu 24.04 build: `base` → `base_builder` → `third_party_builder` → `dev_builder`. `base` installs clang-18 and makes it the default `cc`/`c++`. `base_builder` runs `pip3 install --break-system-packages` for pygobject, pygobject-stubs, setuptools, `python_stdnum`, and `pytest`; those existing dependencies are out of scope and stay as they are. The image's system pip is `pip 24.0` (Debian package `python3-pip 24.0+dfsg-1ubuntu1.3`, verified by running `pip3 --version` in `ghcr.io/rdkcentral/barton_builder:2.17`), Python is 3.12.3, CMake is 3.28.3, and pipx is 1.4.3. `third_party_builder` installs Node.js 22 from NodeSource. `dev_builder` adds clang-format, lcov, pipx, `stack-pr`, and cocogitto (SHA-256 verified with an `ARG` hash). Image version is tracked in `docker/version` (currently `2.17`) and consumed by `./dockerw`.
- CMake options are declared with `bcore_option()` in `config/cmake/options.cmake`. Sanitizer and SSP flags are applied globally in the root `CMakeLists.txt` by appending to `CMAKE_C_FLAGS`/`CMAKE_CXX_FLAGS`/linker flags. `core/CMakeLists.txt` also branches on `BCORE_BUILD_WITH_ASAN`. Coverage (`BCORE_TEST_COVERAGE`) uses `CodeCoverage.cmake` + lcov 2.x + `lcov_cobertura.py`.
- Docker images are built only through `docker compose` using `docker/compose.yaml` (`build.context: .`, i.e. `docker/`): `docker/build.sh` runs `docker compose -f compose.yaml build`, `./dockerw` runs `docker compose -f docker/compose.yaml [...] run` (building on demand, including in `.github/workflows/run-tests.yaml` when the image pull fails), and the devcontainer layers `docker/compose.devcontainer.yaml`, which builds `docker/Dockerfile.devcontainer` `FROM $IMAGE_REPO:$IMAGE_TAG`. There is no `.dockerignore` anywhere in the repo.
- The dev initial cache `config/cmake/platforms/dev/linux.cmake` (passed by `build.sh` via `-C`) sets `CMAKE_BUILD_TYPE=Debug`, turns ON ASAN, SSP, GIR, ONVIF, and reference camera support, sets `CMAKE_PREFIX_PATH`, and calls `BCoreMatterHelper.cmake` functions. The root `CMakeLists.txt` calls `project(barton-core)` at line 26 and includes `config/cmake/options.cmake` at line 69. `testing/py_test.sh` locates the ASAN runtime (`libclang_rt.asan-<arch>.so` or gcc `libasan.so`) and runs `LD_PRELOAD="$ASAN_LIB" pytest ...`.
- `pyproject.toml` holds only `[tool.pytest.ini_options]`. There is no `.clang-tidy`, no cppcheck config, no Python lint/type config, no `CMakePresets.json`. Agent guidance lives in `.github/copilot-instructions.md` and `.github/skills/`; a root `AGENTS.md` is out of scope for this change.
- The public shared library is `libBartonCore.so` (`SOVERSION` = major) with headers under `api/c/public/`.

No `docs/dev/architecture.md` exists in the project; architecture is described in `docs/barton-device-service-architecture.md`, which this change does not need to modify because no runtime component changes.

```text
 ./dockerw ──► dev_builder image
                  │  (inherits)
                  ▼
            quality_tools  (NEW: clang-tidy-18, cppcheck, abidiff, bloaty,
                  │          ruff, mypy, gcovr, lizard, pip-audit, pytest-cov, jscpd, syft)
                  │   ◄── named context "repo" (= repo root, .dockerignore → pyproject.toml only)
                  ▼        RUN --mount=from=repo,source=pyproject.toml → pip --group quality
           third_party_builder ─► base_builder ─► base (ubuntu:24.04, clang-18)

 repo ──► CMakePresets.json ─► dev-linux (CMAKE_PROJECT_barton-core_INCLUDE = linux.cmake)
                                 └─► dev ─► coverage / asan / ubsan / tsan / tidy
       ──► scripts/quality/<check>.sh ─► build/reports/<check>/...
       ──► .clang-tidy, .jscpd.json, pyproject [tool.*] + [dependency-groups], .size-snapshot.json, .api/
       ──► docs/REQUIRED_CHECKS.md ◄── .github/copilot-instructions.md
```

## References

- [Ubuntu noble package archive (Launchpad API)](https://api.launchpad.net/1.0/ubuntu/+archive/primary) — Used to confirm noble versions: `cppcheck 2.13.0-2ubuntu3` (universe), `abigail-tools 2.4-3build1` from source `libabigail` (universe), `llvm-toolchain-18 1:18.1.3-1ubuntu1` (updates). No `bloaty` package exists in noble.
- [google/bloaty](https://github.com/google/bloaty) — Only tags are `v1.0` and `v1.1` (2020); `main` HEAD `f1a83e63f36beb628dcf022e1201a2eb391a22a6` (2026-09-24) is the candidate pin for a source build.
- [anchore/syft releases](https://github.com/anchore/syft/releases) — Latest `v1.55.0`; publishes `syft_1.55.0_checksums.txt` (linux amd64 tarball SHA-256 `bb65249542db5fedeeb4d3f9b018b75e7264dbfbbb10486f044581a92f90490d`).
- PyPI JSON API — Latest versions at time of writing: ruff 0.17.2, mypy 2.4.0, gcovr 8.7, lizard 1.24.3, pip-audit 2.10.1, pytest-cov 7.1.0, pip 26.2.2.
- [pip changelog, 25.1](https://pip.pypa.io/en/stable/news/) — `pip install --group [path:]group` (PEP 735 dependency groups) was added in pip 25.1 (2025-04-26); the image's pip 24.0 lacks it.
- [PEP 735 – Dependency Groups](https://peps.python.org/pep-0735/) — Defines the top-level `[dependency-groups]` table used for the `quality` group.
- [Docker build context](https://docs.docker.com/build/concepts/context/) — Named contexts via `--build-context` and `RUN --mount=from=<name>`; states the client reads `.dockerignore` from the root of "the context", without explicitly addressing named contexts.
- [Compose Build Specification](https://docs.docker.com/reference/compose-file/build/) — `additional_contexts` maps a name to a local directory (relative to the compose file), Git URL, or image.
- [docker/buildx#1765](https://github.com/docker/buildx/issues/1765) — Build logs there show `[context <name>] load .dockerignore` for named contexts, suggesting the ignore file at a named context's root is honored; not confirmed by official docs (see Open Questions).
- npm registry — jscpd latest 5.4.2.
- [CMake presets manual](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) — Presets were added in CMake 3.19 (schema version 1); version 1 already provides `inherits`, `hidden`, `generator`, `binaryDir`, `cacheVariables`, `displayName`, and `cmakeMinimumRequired`, and requires non-hidden presets to have (or inherit) `generator` and `binaryDir`.
- [CMake `project()` code injection](https://cmake.org/cmake/help/latest/command/project.html#code-injection) — `CMAKE_PROJECT_<PROJECT-NAME>_INCLUDE` (CMake ≥ 3.15) is included as the last step of `project(<PROJECT-NAME>)`, only for that project name.
- CMake 3.28 `Modules/CMakeInitializeConfigs.cmake` (in the image) — For single-config generators, language enablement creates the `CMAKE_BUILD_TYPE` cache entry from `CMAKE_BUILD_TYPE_INIT` (empty on Linux), so a later `set(CMAKE_BUILD_TYPE ... CACHE ...)` does not override it.
- [clang-tidy checks list](https://clang.llvm.org/extra/clang-tidy/checks/list.html) — Check groups `bugprone-*`, `cert-*`, `clang-analyzer-*`, `performance-*`.
- [Clang sanitizers](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html) / [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html) — TSan cannot be combined with ASan; UBSan can.
- [libabigail abidw/abidiff manual](https://sourceware.org/libabigail/manual/) — ABI XML generation and comparison exit-code bit semantics.

## Goals / Non-Goals

**Goals:**

- One `quality_tools` image stage, pinned and SHA-verified, inherited by `dev_builder`.
- Every check runnable as `./dockerw scripts/quality/<check>.sh`, writing to `build/reports/<check>/`.
- `compile_commands.json` always available for clang-tidy/cppcheck.
- New sanitizers and `-Werror` opt-in only; default `./build.sh` and `testing/py_test.sh` behavior unchanged apart from the extra JUnit file.
- Committed baselines (size snapshot, ABI baseline slot, suppression lists) so metrics compare against something.
- A verification contract (`docs/REQUIRED_CHECKS.md`) listing required checks, commands, and exception process.

**Non-Goals:**

- Remediating findings; large existing finding counts are baselined.
- Making any check fail the build by default.
- Running clang-tidy/cppcheck on `third_party/` or the Matter SDK.
- arm64 support of any kind; the dev image targets linux amd64 only.
- Pinning, moving, or otherwise changing existing Python dependencies (pygobject, pygobject-stubs, setuptools, python-stdnum, pytest) in `base_builder`.
- A root `AGENTS.md`.
- Valgrind memcheck. It was implemented and then removed: on this codebase it took ~30 min per run (about two-thirds of `all.sh`), its only unique coverage over ASan/UBSan/TSan is uninitialized reads, and all 11 baseline findings were in the Matter SBMD/mquickjs code (not triaged; possibly JS-engine false positives). Re-adding it, e.g. as an opt-in strict check, is a separate change.
- Teaching the top-level `CMakeLists.txt` about platforms or presets, or splitting/changing `config/cmake/platforms/dev/linux.cmake`.

## Decisions

### D1: New `quality_tools` stage between `third_party_builder` and `dev_builder`

**Decision:** Insert `FROM third_party_builder AS quality_tools` and change `dev_builder` to `FROM quality_tools AS dev_builder`. apt tools use noble archive versions pinned with `=<version>`; Python quality tools are installed from the `quality` dependency group in the root `pyproject.toml` (D2); jscpd via `npm install -g jscpd@<ver>` against the Node 22 already in `third_party_builder`; syft and bloaty are fetched with SHA-256 / commit-SHA verification. Only linux amd64 artifacts are referenced.

```dockerfile
# docker/Dockerfile
###############################################################################
# Quality tools stage
#
# Static analysis, metrics, and SBOM tools consumed by scripts/quality/ and the
# review-metrics-* agentic review skills. All versions are pinned; direct
# downloads are SHA-256 verified. Python tools come from the `quality`
# dependency group in the repo-root pyproject.toml, provided by the `repo`
# named build context (see docker/compose.yaml additional_contexts).
###############################################################################
FROM third_party_builder AS quality_tools

ARG CLANG_TIDY_VERSION="1:18.1.3-1ubuntu1"
ARG CPPCHECK_VERSION="2.13.0-2ubuntu3"
ARG ABIGAIL_VERSION="2.4-3build1"

RUN apt-get update && DEBIAN_FRONTEND='noninteractive' apt-get install -y --no-install-recommends \
    clang-tidy-18="${CLANG_TIDY_VERSION}" \
    cppcheck="${CPPCHECK_VERSION}" \
    abigail-tools="${ABIGAIL_VERSION}" \
    && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 50 \
    && rm -rf /var/lib/apt/lists/*

# bloaty is not packaged for noble; build a pinned commit from source.
ARG BLOATY_COMMIT="f1a83e63f36beb628dcf022e1201a2eb391a22a6"
RUN git clone --filter=blob:none https://github.com/google/bloaty.git /tmp/bloaty && \
    cd /tmp/bloaty && git checkout "${BLOATY_COMMIT}" && \
    test "$(git rev-parse HEAD)" = "${BLOATY_COMMIT}" && \
    git submodule update --init --recursive --depth 1 && \
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBLOATY_ENABLE_RE2=ON && \
    cmake --build build && cmake --install build --prefix /usr/local && \
    rm -rf /tmp/bloaty

ARG SYFT_VERSION="1.55.0"
ARG SYFT_SHA256="bb65249542db5fedeeb4d3f9b018b75e7264dbfbbb10486f044581a92f90490d"
RUN curl -fsSL "https://github.com/anchore/syft/releases/download/v${SYFT_VERSION}/syft_${SYFT_VERSION}_linux_amd64.tar.gz" \
    -o /tmp/syft.tar.gz && \
    echo "${SYFT_SHA256}  /tmp/syft.tar.gz" | sha256sum --check && \
    tar xzf /tmp/syft.tar.gz -C /usr/local/bin syft && \
    rm /tmp/syft.tar.gz

ARG JSCPD_VERSION="5.4.2"
RUN npm install -g --ignore-scripts "jscpd@${JSCPD_VERSION}"

# Python quality tools (see D2). The system pip (24.0) lacks `--group`, so a
# throwaway venv provides a pinned pip >= 25.1 that installs into the system
# interpreter via `--python`, keeping the tools importable by the system pytest.
ARG BOOTSTRAP_PIP_VERSION="26.2.2"
ARG BOOTSTRAP_PIP_SHA256="fd7f4dfa8190e014013b9f08f3e973f4e4c26ffd179a204895533a7254813a70"
RUN --mount=from=repo,source=pyproject.toml,target=/tmp/repo/pyproject.toml \
    python3 -m venv /tmp/pip-bootstrap && \
    printf 'pip==%s --hash=sha256:%s\n' "${BOOTSTRAP_PIP_VERSION}" "${BOOTSTRAP_PIP_SHA256}" > /tmp/pip-bootstrap.txt && \
    /tmp/pip-bootstrap/bin/pip install --require-hashes -r /tmp/pip-bootstrap.txt && \
    /tmp/pip-bootstrap/bin/pip --python /usr/bin/python3 install --break-system-packages \
        --group /tmp/repo/pyproject.toml:quality && \
    rm -rf /tmp/pip-bootstrap /tmp/pip-bootstrap.txt

FROM quality_tools AS dev_builder
```

A git commit pin is verified by `git rev-parse` (content-addressed SHA-1), which is the bloaty equivalent of a tarball SHA-256. `RUN --mount=from=<named context>` requires BuildKit, which `docker compose build` uses by default.

**Alternative considered:** Installing tools directly in `dev_builder`. Rejected — the Jira scope asks for a separable stage, which also lets BARTON-416 CI target `quality_tools` without dev-only packages. **Alternative considered:** bloaty `v1.1` tag. Rejected — 2020 release predates fixes for current ELF/DWARF5 output from clang-18.

**Trade-offs:** Image size grows (bloaty build deps are already present from `third_party_builder`; final layer cost is mostly clang-tidy and Python packages). Bloaty source build adds several minutes to image builds.

---

### D2: Python quality tools as a PEP 735 dependency group via a named build context

**Decision:** Declare the new Python quality tools as the `quality` group in a top-level `[dependency-groups]` table in the root `pyproject.toml`, pinned with `==`. The image installs that group from a BuildKit named build context `repo` that points at the repo root; the main build context stays `docker/`. Existing Python dependencies in `base_builder` are untouched.

```toml
# pyproject.toml
[dependency-groups]
quality = [
    "ruff==0.17.2",
    "mypy==2.4.0",
    "gcovr==8.7",
    "lizard==1.24.3",
    "pip-audit==2.10.1",
    "pytest-cov==7.1.0",
]
```

```yaml
# docker/compose.yaml (barton service)
        build:
            context: .
            dockerfile: Dockerfile
            additional_contexts:
                repo: ..
            args:
                - MATTER_REF=$MATTER_REF
```

```text
# .dockerignore (repo root; applies to the `repo` named context only)
*
!pyproject.toml
```

`additional_contexts` paths are relative to the compose file, so `..` is the repo root. Every image build path goes through `docker/compose.yaml` — `docker/build.sh`, `./dockerw` (including the CI fallback build in `.github/workflows/run-tests.yaml`), and the devcontainer — so this is the only build definition that needs the context. `docker/compose.devcontainer.yaml` overrides `build` with `Dockerfile.devcontainer` (`FROM $IMAGE_REPO:$IMAGE_TAG`); Compose merges the `build` mapping, so the `repo` context is passed there too but unused (Compose may print an unused-context warning), and no change to the overlay is required. The main context `docker/` has no `.dockerignore` and is unaffected by the new root file.

Pip version: the image's system pip is 24.0 (Ubuntu noble `python3-pip 24.0+dfsg-1ubuntu1.3`, verified in `barton_builder:2.17`), and `--group` requires pip ≥ 25.1. The Dockerfile therefore creates a temporary venv, installs a hash-pinned pip 26.2.2 into it, and runs that pip with `--python /usr/bin/python3 --break-system-packages` so the tools land in the system site-packages next to the existing `pytest` (required for the `pytest-cov` plugin to load in `testing/py_test.sh`). The venv is deleted in the same layer; the system pip stays 24.0. pip-audit audits the installed Python environment in the image (its default mode), which includes the `quality` group.

**Alternative considered:** `testing/requirements*.txt` files copied into the image. Rejected by the user in favor of standard PEP 735 groups in the existing `pyproject.toml`. **Alternative considered:** Symlinking `pyproject.toml` into `docker/`. Rejected — Docker does not follow symlinks that point outside the build context. **Alternative considered:** Changing the main build context to the repo root. Rejected — enlarges every context transfer and changes all existing `COPY` paths in `docker/Dockerfile`. **Alternative considered:** Upgrading the system pip in place. Rejected — overwrites the Debian-managed pip. **Alternative considered:** pipx per-tool venvs. Rejected — `pytest-cov` must be importable by the system `pytest`, and pipx venvs would hide it.

**Trade-offs:** Introduces the first named build context and the first root `.dockerignore`. Group pins are exact versions but not hash-locked; transitive dependencies resolve at image build time.

---

### D3: Sanitizer options and mutual exclusion

**Decision:** Add `BCORE_BUILD_WITH_UBSAN` and `BCORE_BUILD_WITH_TSAN` beside ASAN; refactor the root `CMakeLists.txt` block into a list so linker/compiler flags stay consistent. TSAN with ASAN is a configure-time `FATAL_ERROR`.

```cmake
# config/cmake/options.cmake
bcore_option(NAME BCORE_BUILD_WITH_UBSAN
           DEFINITION BARTON_CONFIG_BUILD_WITH_UBSAN
           DESCRIPTION "Build Barton with Undefined Behavior Sanitizer")
bcore_option(NAME BCORE_BUILD_WITH_TSAN
           DEFINITION BARTON_CONFIG_BUILD_WITH_TSAN
           DESCRIPTION "Build Barton with Thread Sanitizer (incompatible with ASAN)")
bcore_option(NAME BCORE_WERROR
           DEFINITION BARTON_CONFIG_WERROR
           DESCRIPTION "Treat compiler warnings as errors for first-party code")
```

```cmake
# CMakeLists.txt (replaces the existing BCORE_BUILD_WITH_ASAN block)
if (BCORE_BUILD_WITH_TSAN AND BCORE_BUILD_WITH_ASAN)
    message(FATAL_ERROR "BCORE_BUILD_WITH_TSAN cannot be combined with BCORE_BUILD_WITH_ASAN")
endif()

set(BCORE_SANITIZERS "")
if (BCORE_BUILD_WITH_ASAN)
    list(APPEND BCORE_SANITIZERS address)
endif()
if (BCORE_BUILD_WITH_UBSAN)
    list(APPEND BCORE_SANITIZERS undefined)
endif()
if (BCORE_BUILD_WITH_TSAN)
    list(APPEND BCORE_SANITIZERS thread)
endif()

if (BCORE_SANITIZERS)
    list(JOIN BCORE_SANITIZERS "," _bcoreSanList)
    set(_bcoreSanFlags "-fsanitize=${_bcoreSanList} -fno-omit-frame-pointer")
    if (BCORE_BUILD_WITH_UBSAN)
        string(APPEND _bcoreSanFlags " -fno-sanitize-recover=undefined")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            string(APPEND _bcoreSanFlags " -fno-sanitize=vptr")
        endif()
    endif()
    foreach (_var CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_EXE_LINKER_FLAGS)
        set(${_var} "${${_var}} ${_bcoreSanFlags}")
    endforeach()
endif()
```

With ASAN only, the resulting flag is still `-fsanitize=address` (plus `-fno-omit-frame-pointer`, which is already the practical effect of Debug builds), satisfying the existing build-system scenario. Because the dev initial cache enables ASAN, the `tsan` preset explicitly sets `BCORE_BUILD_WITH_ASAN=OFF`.

Under clang, UBSan excludes `vptr`: C executables that link `libBartonCore` (the reference app, the g-ir-scanner dump program) use the C driver, which omits clang's C++ UBSan runtime and fails with undefined `__ubsan_vptr_type_cache`. Linking the C++ runtime everywhere breaks pure-C test executables, and `-shared-libsan` would require runtime library-path plumbing for every test and the Python suite. The exclusion applies only to clang builds.

**Alternative considered:** Per-target `target_compile_options`. Rejected — existing ASAN is global and must also cover the Matter glue and tests; changing scope is out of this story.

---

### D4: Warnings and `BCORE_WERROR` scoped to first-party targets

**Decision:** Define an INTERFACE target `bCoreWarnings` carrying `-Wall -Wextra` (and `-Werror` when `BCORE_WERROR=ON`) and link it into first-party targets (`bCoreObject`, `libs/*`, reference app, unit tests). Third-party and Matter SDK builds do not get it.

`-Wall -Wextra` are gated by `BCORE_EXTRA_WARNINGS` (default OFF, set ON in `config/cmake/platforms/dev/linux.cmake`, so `./build.sh` and every preset get them). The cross-project review found that zilker injects `-Wall -Werror` into BartonCore's build; unconditional `-Wextra` would turn the ~50 existing `-Wextra`-class baseline warnings (e.g. `-Wunused-parameter`) into errors on its next tag bump. The option is deliberately private (advanced, not listed in user docs, not a `bcore_option`, no compile definition) and temporary: once the existing findings are fixed it is removed and the flags become unconditional.

```cmake
# config/cmake/modules/BCoreWarnings.cmake
option(BCORE_EXTRA_WARNINGS "Internal: enable -Wall -Wextra on first-party targets" OFF)
mark_as_advanced(BCORE_EXTRA_WARNINGS)
add_library(bCoreWarnings INTERFACE)
if (BCORE_EXTRA_WARNINGS)
    target_compile_options(bCoreWarnings INTERFACE -Wall -Wextra)
endif()
if (BCORE_WERROR)
    target_compile_options(bCoreWarnings INTERFACE -Werror)
endif()
```

**Alternative considered:** Appending to `CMAKE_C_FLAGS`. Rejected — would leak `-Werror` into `third_party/` subprojects.

**Trade-offs:** Builds will print many new warnings; this is the intended signal for `review-metrics-build-health`. Default builds still succeed.

---

### D5: `CMAKE_EXPORT_COMPILE_COMMANDS` and `cppcheck` target

**Decision:** Set `CMAKE_EXPORT_COMPILE_COMMANDS ON` unconditionally near `project()`. Add a `cppcheck` custom target (only when `find_program(CPPCHECK cppcheck)` succeeds) that consumes `compile_commands.json`, filters out `third_party/` and `_deps/`, and writes XML to `${CMAKE_BINARY_DIR}/cppcheck.xml`. The build tree is the only location the target knows; `cppcheck.sh` deletes any stale XML before running and copies the result to `${REPORT_DIR}/cppcheck.xml`, so the report root (`BCORE_REPORTS_DIR`) has a single source of truth in `lib.sh` and is never cached at configure time.

```cmake
# config/cmake/modules/BCoreCppcheck.cmake
find_program(BCORE_CPPCHECK_EXE NAMES cppcheck)
if (BCORE_CPPCHECK_EXE)
    add_custom_target(cppcheck
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/cppcheck.xml
        COMMAND ${BCORE_CPPCHECK_EXE}
            --project=${CMAKE_BINARY_DIR}/compile_commands.json
            --enable=warning,performance,portability
            --inline-suppr
            --suppressions-list=${CMAKE_SOURCE_DIR}/config/quality/cppcheck-suppressions.txt
            -i${CMAKE_SOURCE_DIR}/third_party -i${CMAKE_BINARY_DIR}
            --xml --output-file=${CMAKE_BINARY_DIR}/cppcheck.xml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck"
        VERBATIM)
endif()
```

`--error-exitcode` is intentionally omitted so the target reports but does not fail.

**Implementation findings (apply phase):**

- CMake list-valued config defines (`BARTON_CONFIG_MATTER_*_HEADER_PATHS`, `*_IMPLEMENTATIONS`) contain `;`, which makes cppcheck bail out of every translation unit ("bad macro syntax"). The target first runs `config/cmake/modules/cppcheck_compile_db.py`, which writes `compile_commands.cppcheck.json` without `-D` arguments containing `;` (these defines are consumed only by CMake).
- cppcheck does not predefine compiler macros, so `sbrm.h` ("GCC is required"), `nlbyteorder.h`, and `gstconfig.h` hit `#error` and most translation units were silently skipped. The target passes `-D__GNUC__=13 -D__x86_64__ -D__ORDER_LITTLE_ENDIAN__=1234 -D__ORDER_BIG_ENDIAN__=4321 -D__BYTE_ORDER__=1234` and `-j<logical cores>`.
- Full-tree analysis then takes ~17 min wall / ~6.3 CPU-hours on a 128-core host. Decision (user): `cppcheck.sh` analyzes only C/C++ sources changed since the merge base with `QUALITY_BASE_REF` (default `origin/main`, else `main`), plus sources that `#include` a changed header; `--full` and `--update-baseline` analyze the whole tree. The selection reaches the target through the `BCORE_CPPCHECK_ONLY` environment variable (a file listing source paths), which the helper uses to filter the compile database.

---

### D6: `CMakePresets.json` loading the dev platform file via `CMAKE_PROJECT_barton-core_INCLUDE`

**Decision:** Add a root `CMakePresets.json` (schema version 1, CMake ≥ 3.19) with configure presets `dev-linux`, `dev` (an alias that inherits `dev-linux` unchanged), and `coverage`, `asan`, `ubsan`, `tsan`, `tidy`, each inheriting `dev`. `dev-linux` does not copy values from `config/cmake/platforms/dev/linux.cmake`; it sets the cache variable `CMAKE_PROJECT_barton-core_INCLUDE` to that file, so CMake includes it as the last step of `project(barton-core)`. The top-level `CMakeLists.txt` gains no knowledge of platforms or presets, and `linux.cmake` is neither split nor changed.

```json
{
  "version": 1,
  "cmakeMinimumRequired": { "major": 3, "minor": 19, "patch": 0 },
  "configurePresets": [
    {
      "name": "dev-linux",
      "displayName": "Dev (Linux): loads config/cmake/platforms/dev/linux.cmake",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_PROJECT_barton-core_INCLUDE": "${sourceDir}/config/cmake/platforms/dev/linux.cmake",
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    { "name": "dev", "inherits": "dev-linux", "displayName": "Dev (alias of dev-linux)" },
    { "name": "coverage", "inherits": "dev", "cacheVariables": { "BCORE_TEST_COVERAGE": "ON", "BCORE_BUILD_WITH_ASAN": "OFF" } },
    { "name": "asan", "inherits": "dev", "cacheVariables": { "BCORE_BUILD_WITH_ASAN": "ON" } },
    { "name": "ubsan", "inherits": "dev", "cacheVariables": { "BCORE_BUILD_WITH_ASAN": "OFF", "BCORE_BUILD_WITH_UBSAN": "ON" } },
    { "name": "tsan", "inherits": "dev", "cacheVariables": { "BCORE_BUILD_WITH_ASAN": "OFF", "BCORE_BUILD_WITH_TSAN": "ON" } },
    { "name": "tidy", "inherits": "dev", "cacheVariables": { "BCORE_BUILD_WITH_ASAN": "OFF", "CMAKE_C_CLANG_TIDY": "clang-tidy", "CMAKE_CXX_CLANG_TIDY": "clang-tidy" } }
  ]
}
```

Every field used (`inherits`, `hidden`-free presets with `generator` and `binaryDir`, `cacheVariables`, `displayName`, `cmakeMinimumRequired`, `${sourceDir}`/`${presetName}` macros) exists in presets schema version 1, introduced in CMake 3.19; the image ships CMake 3.28.3. The project's `cmake_minimum_required(VERSION 3.16.5)` is unchanged, and `./build.sh` (which passes `-C linux.cmake`) remains the supported default entrypoint.

Per the [CMake `project()` code-injection docs](https://cmake.org/cmake/help/latest/command/project.html#code-injection), `CMAKE_PROJECT_<PROJECT-NAME>_INCLUDE` (CMake ≥ 3.15) is included only when `project()` is called with that exact name, so FetchContent/ExternalProject subprojects such as BartonCommon are unaffected. It runs after language enablement and before the root `CMakeLists.txt` includes `config/cmake/options.cmake` (line 69), which is the point where the `-C` values were previously consumed.

Differences from `-C`:

- The file runs after compiler detection rather than before. `linux.cmake` sets no compiler or toolchain variables, so detection is unaffected.
- `set(... CACHE ...)` semantics are the same: an entry already in the cache (including values from preset `cacheVariables`, which are applied as `-D`) is not overridden. This is what lets `tsan`, `ubsan`, `coverage`, and `tidy` turn `BCORE_BUILD_WITH_ASAN` OFF even though `linux.cmake` sets it ON.
- Because of that rule, `set(CMAKE_BUILD_TYPE "Debug" CACHE ...)` in `linux.cmake` is a no-op under presets: for single-config generators, language enablement in `project()` already creates the `CMAKE_BUILD_TYPE` cache entry from `CMAKE_BUILD_TYPE_INIT` (empty on Linux; see `Modules/CMakeInitializeConfigs.cmake` in CMake 3.28). `dev-linux` therefore sets `CMAKE_BUILD_TYPE=Debug` itself. This is the only value repeated from `linux.cmake`, and it is required for parity with `build.sh`.
- `-C` scripts run in a throwaway scope, while the include runs in the top-level directory scope, so any normal (non-cache) variables would persist. `linux.cmake` only sets cache variables and calls `BCoreMatterHelper` functions, so behavior is the same.

**Alternative considered:** Copying `linux.cmake` values into preset `cacheVariables`. Rejected — duplicates the platform file and cannot express the `BCoreMatterHelper` function calls. **Alternative considered:** Teaching `CMakeLists.txt` to include a platform file chosen by a preset variable. Rejected — the top-level project must stay platform-agnostic. **Alternative considered:** `CMAKE_PROJECT_INCLUDE` (no project name). Rejected — it would also run for every subproject `project()` call. **Alternative considered:** Extending `build.sh` flags only. Rejected — presets are what agents and IDEs discover generically.

**Matter install directory (added during implementation):** `build-matter.sh` previously always installed to `<repo>/build/matter-install`, while `linux.cmake` and several `CMakeLists.txt` files used `${CMAKE_BINARY_DIR}/matter-install`, so preset trees under `build/<preset>/` could not find Matter. A new cache path `BCORE_MATTER_INSTALL_DIR` (default `${CMAKE_BINARY_DIR}/matter-install`, defined in `options.cmake` and `linux.cmake`) replaces those references and is passed to a new `build-matter.sh -i <install-dir>` option. Because Matter is built with or without `-a` (ASAN), presets share two installs: `build/matter-install` for ASAN presets (the same one `build.sh` uses) and `build/matter-noasan/matter-install` for `coverage`, `ubsan`, `tsan`, and `tidy`.

**Trade-offs:** Preset build trees live under `build/<preset>/` rather than `build/`, so preset and `build.sh` builds do not share a tree.

---

### D7: Committed configuration and baselines

**Decision:**

- `.clang-tidy`: `Checks: '-*,bugprone-*,cert-*,clang-analyzer-*,performance-*,-bugprone-easily-swappable-parameters,-cert-err33-c'`, `HeaderFilterRegex: '^(api|core|libs|reference)/'`, `WarningsAsErrors: ''`, `FormatStyle: none`. Excluded checks are noisy-but-low-value for this C codebase and are listed with reasons in `docs/REQUIRED_CHECKS.md`.
- `pyproject.toml` additions:

```toml
[tool.ruff]
target-version = "py312"
line-length = 120
extend-exclude = ["build", "third_party"]

[tool.ruff.lint]
select = ["E", "F", "W", "B", "I", "UP"]

[tool.mypy]
python_version = "3.12"
files = ["testing"]
ignore_missing_imports = true
warn_unused_ignores = true

[tool.coverage.run]
branch = true
source = ["testing"]
data_file = "build/reports/pytest-cov/.coverage"

[tool.coverage.xml]
output = "build/reports/pytest-cov/coverage.xml"
```

  `[tool.pytest.ini_options]` gains `markers = [...]` (every marker currently used under `testing/`, collected by grep during implementation) and `--strict-markers` in `addopts`.
- `.jscpd.json`: `{"minTokens": 70, "reporters": ["json","console"], "output": "build/reports/jscpd", "ignore": ["build/**","third_party/**","**/test/**","**/tests/**","**/testing/**","**/*.pb.*","**/zap-generated/**","**/gen/**"], "format": ["c","cpp","python","javascript"]}`.
- Lizard thresholds live in `scripts/quality/lizard.sh` (`-C 15 -L 1000 -a 10`) because lizard has no standard config file.
- `.size-snapshot.json`: generated from a `dev` preset build of `libBartonCore.so` via bloaty, schema in Data Structures.
- `.api/libBartonCore.abi`: abidw XML. Until generated from a release tag it is absent and `abi.sh` reports `BLOCKED: no baseline`; `.api/README.md` documents regeneration.
- `config/quality/cppcheck-suppressions.txt` and `config/quality/clang-tidy-baseline.txt`: initial finding baselines.

**Alternative considered:** Enabling `readability-*`/`modernize-*`. Rejected per Jira (no style checks; clang-format already owns style).

---

### D8: `scripts/quality/` wrapper contract

**Decision:** One script per check sharing `scripts/quality/lib.sh`. Each script: resolves repo root, creates `build/reports/<check>/`, runs the tool, writes the native report plus `summary.json`, prints the report path, and exits `0` (ran, no new findings vs baseline), `1` (ran, new findings), `2` (tool error), `3` (BLOCKED: missing tool, missing build artifact, or missing baseline). Scripts never install anything.

```bash
#!/usr/bin/env bash
# scripts/quality/lib.sh
set -euo pipefail

QUALITY_ROOT="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
REPORTS_ROOT="${BCORE_REPORTS_DIR:-${QUALITY_ROOT}/build/reports}"

quality_init() {
    CHECK_NAME="$1"
    REPORT_DIR="${REPORTS_ROOT}/${CHECK_NAME}"
    mkdir -p "${REPORT_DIR}"
}

quality_require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        quality_summary blocked "missing tool: $1"
        exit 3
    fi
}

quality_summary() {
    local status="$1" message="$2"
    python3 - "$REPORT_DIR/summary.json" "$CHECK_NAME" "$status" "$message" <<'EOF'
import json, sys, datetime
path, check, status, message = sys.argv[1:5]
json.dump({"check": check, "status": status, "message": message,
           "generated": datetime.datetime.now(datetime.timezone.utc).isoformat()},
          open(path, "w"), indent=2)
EOF
    echo "${CHECK_NAME}: ${status} — ${message} (${REPORT_DIR})"
}
```

## Data Structures

### `build/reports/<check>/summary.json` (output)

| Field       | Type   | Description                                                 |
| ----------- | ------ | ----------------------------------------------------------- |
| `check`     | string | Check name, equal to the script basename without `.sh`      |
| `status`    | string | One of `pass`, `findings`, `error`, `blocked`               |
| `message`   | string | Human-readable one-line result                              |
| `generated` | string | ISO-8601 UTC timestamp                                      |

### `.size-snapshot.json` (committed input)

```json
{
  "artifact": "libBartonCore.so",
  "preset": "dev",
  "tool": "bloaty",
  "generated_from": "<git describe>",
  "sections": { ".text": 0, ".rodata": 0, ".data": 0, ".bss": 0 },
  "vm_total": 0,
  "file_total": 0
}
```

## Interfaces

### CLI — `scripts/quality/`

| Command               | Tool(s)                           | Primary report                                 |
| --------------------- | --------------------------------- | ---------------------------------------------- |
| `clang-tidy.sh`       | clang-tidy-18 (`run-clang-tidy`)  | `build/reports/clang-tidy/clang-tidy.txt`      |
| `cppcheck.sh`         | cppcheck via `cmake --build --target cppcheck` | `build/reports/cppcheck/cppcheck.xml` |
| `build-warnings.sh`   | compiler (`dev` preset, `-Wall -Wextra`) | `build/reports/build-warnings/warnings.txt` |
| `coverage.sh`         | lcov + gcovr (`coverage` preset)  | `build/reports/coverage/{lcov.info,cobertura.xml}` |
| `pytest-cov.sh`       | pytest-cov via `testing/py_test.sh` | `build/reports/pytest-cov/coverage.xml`      |
| `sanitizers.sh <asan\|ubsan\|tsan>` | preset build + ctest; reports reduced to keys by `sanitizer_reports.py`, compared with `config/quality/sanitizers-<kind>-baseline.txt` | `build/reports/sanitizers-<kind>/ctest.xml` |
| `complexity.sh`       | lizard (CCN ≤ 15)                 | `build/reports/complexity/lizard.xml`          |
| `duplication.sh`      | jscpd                             | `build/reports/jscpd/jscpd-report.json`        |
| `abi.sh`              | abidw + abidiff                   | `build/reports/abi/abidiff.txt`                |
| `binary-size.sh`      | bloaty vs `.size-snapshot.json`   | `build/reports/binary-size/bloaty.csv`         |
| `python-lint.sh`      | ruff + mypy                       | `build/reports/python-lint/{ruff.json,mypy.txt}` |
| `dependency-health.sh`| pip-audit + syft                  | `build/reports/dependency-health/{pip-audit.json,sbom.spdx.json}` |
| `all.sh`              | runs every check, aggregates      | `build/reports/summary.json`                   |

Exit codes: `0` pass, `1` findings, `2` tool error, `3` blocked. Every script supports `--help`. All are invoked as `./dockerw scripts/quality/<check>.sh`.

### Integration test JUnit

`testing/py_test.sh` appends `--junitxml=${BCORE_REPORTS_DIR:-build/reports}/pytest/junit.xml` unless the caller already passed `--junitxml`. Other arguments and the ASAN `LD_PRELOAD` behavior are unchanged.

## Implementation Detail

### `testing/py_test.sh` sanitizer handling

`py_test.sh` currently always preloads the ASAN runtime. It gains a `--sanitizer=<asan|ubsan|tsan|none>` option (default `asan`, preserving behavior). For `ubsan` alone no preload is needed when UBSan is linked into `libBartonCore.so` via the minimal runtime; for `tsan` it preloads `libclang_rt.tsan-<arch>.so`; for `none` (coverage build) it skips `LD_PRELOAD`. Python itself is not sanitized, which is why the runtime must be preloaded for ASAN/TSAN.

### `.github/copilot-instructions.md` link

Add a short "Required checks" paragraph pointing to `docs/REQUIRED_CHECKS.md` for the verification contract rather than duplicating its content. No root `AGENTS.md` is created.

## Migrations

1. `[dependency-groups] quality` in `pyproject.toml`, root `.dockerignore`, and `additional_contexts: repo: ..` in `docker/compose.yaml`.
2. `quality_tools` stage; bump `docker/version` (2.17 → 2.18).
3. CMake: compile commands, warnings target, sanitizer refactor, new options, cppcheck target, presets.
4. Committed configs and baselines (requires the new image to generate).
5. `scripts/quality/` and `py_test.sh` changes.
6. `docs/REQUIRED_CHECKS.md` and the `.github/copilot-instructions.md` link.

Rollback: revert the commit and `docker/version`; no data or API migration.

## Testing Philosophy

### Default behavior unchanged

Build with `./dockerw ./build.sh` and run `ctest` and `testing/py_test.sh` before and after; the same tests pass and the only new artifact is `build/reports/pytest/junit.xml`. Confirm `BCORE_WERROR`, `BCORE_BUILD_WITH_UBSAN`, and `BCORE_BUILD_WITH_TSAN` default OFF in `CMakeCache.txt`.

### Option behavior

Configure with ASAN+TSAN and verify `FATAL_ERROR`. Configure each preset and verify the expected `-fsanitize=` flags appear in `compile_commands.json`. Configure with `BCORE_WERROR=ON` and verify `-Werror` appears for `core/` sources but not `third_party/` sources.

### Tool availability

Run a script that invokes `--version` for every tool inside `./dockerw` and compares against the pinned versions; failure if any is missing. Confirm the system `pip3 --version` is unchanged (24.0), that `pytest --help` lists the `--cov` option (pytest-cov visible to the system pytest), and that the image build log shows a small `repo` context transfer.

### Preset parity

Configure `cmake --preset dev` and `./build.sh`'s `-C` configuration and diff the relevant `CMakeCache.txt` entries (`CMAKE_BUILD_TYPE`, `BCORE_BUILD_WITH_ASAN`, `BCORE_BUILD_WITH_SSP`, `BCORE_GEN_GIR`, `BCORE_ONVIF`, `BCORE_REFERENCE_CAMERA_SUPPORT`, `BCORE_MATTER_USE_RANDOM_PORT`, `CMAKE_PREFIX_PATH` relative to each binary dir); they must match. Verify the `tsan` preset ends with `BCORE_BUILD_WITH_ASAN=OFF` despite `linux.cmake`, and that BartonCommon's FetchContent `project()` does not re-include `linux.cmake`.

### Wrapper contract

For each script: run normally and assert `summary.json` exists with a valid status; temporarily hide the tool from `PATH` and assert exit code `3` with `status: blocked`.

## Documentation Plan

### `docs/REQUIRED_CHECKS.md`

**Audience:** Developers, reviewers, agents

The verification contract: table of required checks (name, script, tool, report path, blocking vs advisory), how to run each via `./dockerw`, how baselines are updated, and the exceptions process (who approves, how suppressions are recorded inline or in `config/quality/`). States that BARTON-416 will wire these into CI.

### `.github/copilot-instructions.md`

**Audience:** Agents

Add a reference to `docs/REQUIRED_CHECKS.md` as the verification contract to follow before proposing changes.

### `docker/README.md`

**Audience:** Developers

Document the `quality_tools` stage, pinned tool list, how to update a pin and its SHA, the `repo` named build context and root `.dockerignore` whitelist, the `quality` dependency group in `pyproject.toml`, and why a bootstrap pip ≥ 25.1 is used for `--group`.

### `.api/README.md`

**Audience:** Maintainers

How to regenerate the ABI baseline from a release tag with `abidw`.

## Risks / Trade-offs

### Image size growth

**Risk:** clang-tidy, Python tooling, and bloaty enlarge the dev image and slow pulls.

**Mitigation:** Single `apt-get` layer with `--no-install-recommends` and list cleanup; bloaty build tree removed in the same layer. Measure before/after and record in the PR.

### Large existing finding counts

**Risk:** clang-tidy, cppcheck, `-Wall -Wextra`, lizard, and jscpd will report many existing issues in ~110k lines of C.

**Mitigation:** Baseline/suppression files committed at introduction. Finding keys omit line numbers so unrelated edits do not churn them, and baselines are compared as multisets (one line per distinct location), so an additional occurrence of an already-baselined key in the same file is still reported as new; wrappers report only new findings against the baseline. Fixing is explicitly out of scope.

### Sanitizer runtime conflicts in `py_test.sh`

**Risk:** `py_test.sh` hard-codes the ASAN `LD_PRELOAD`; a UBSan or TSan build of `libBartonCore.so` loaded under that preload will fail or produce false reports.

**Mitigation:** `--sanitizer` option selects the correct runtime; TSAN+ASAN rejected at configure time; default remains ASAN.

### No ABI baseline initially

**Risk:** `abidiff` has nothing to compare to until a baseline is generated from a release tag.

**Mitigation:** `abi.sh` reports BLOCKED with a clear message; baseline generation is a documented maintainer step.

### Bloaty unpackaged and untagged since 2020

**Risk:** Source build of a `main` commit may break on future toolchain updates; no upstream checksum.

**Mitigation:** Pin by commit SHA and verify with `git rev-parse`; isolated in its own layer.

### Tool version drift

**Risk:** apt pins break when noble publishes updates and drops older versions from the mirror.

**Mitigation:** Pins are `ARG`s documented in `docker/README.md`; failures are loud at image build time.

### Named context transfer size and builder support

**Risk:** If the root `.dockerignore` is not applied to the `repo` named context, image builds may upload the full repo including large `build/` trees. Builders without BuildKit cannot use `RUN --mount=from=`.

**Mitigation:** Verify the transfer size in the first image build (see Open Questions). `docker compose build` uses BuildKit by default; the legacy builder is not supported for this image.

### Bootstrap pip and unlocked transitive dependencies

**Risk:** The `quality` group pins top-level tools only; transitive dependencies can drift between image builds, and installing into system site-packages with `--break-system-packages` can upgrade packages shared with existing tooling.

**Mitigation:** The bootstrap pip itself is hash-pinned; images are versioned via `docker/version`; the tool availability check confirms existing `pytest` still works. Hash-locking the group is deferred.

## Open Questions

- Does a `.dockerignore` at the root of a local named build context (`additional_contexts: repo: ..`) filter that context? Official docs only say the client reads `.dockerignore` from "the root directory of the context"; buildx issue #1765 logs show `[context <name>] load .dockerignore`, suggesting yes. Confirm during task 1.2 by checking the `repo` context transfer size in the build log; if it is not honored, the whole repo (including `build/` and `third_party/`) may be transferred and an alternative (for example a dedicated small directory as the named context) must be chosen.
- Final clang-tidy disabled-check list after the first baseline run (which noisy checks to disable is not yet known).
