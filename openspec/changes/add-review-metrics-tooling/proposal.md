# Proposal: Add review metrics tooling (BARTON-415)

## Why

The agentic review suite (`review-metrics-*` skills) only uses tools already present in the project container (`./dockerw`, `docker/Dockerfile`) plus committed project configuration; it never installs tools and reports anything missing as a gap or BLOCKED. Today BartonCore's container and repo provide only clang-format, lcov-based coverage, and ASAN, so most metrics (static analysis, complexity, duplication, ABI compatibility, binary size, dependency health, Python lint/typing, UBSan/TSan) cannot run. Provisioning the tools, configs, and baselines once in the repo makes those checks reproducible for humans, agents, and future CI (BARTON-416).

## What Changes

- **Docker**: add a `quality_tools` stage to `docker/Dockerfile` that `dev_builder` inherits from, installing pinned versions of clang-tidy-18, cppcheck, abigail-tools (`abidiff`), bloaty, ruff, mypy, gcovr, lizard, pip-audit, pytest-cov, jscpd, and syft. All versions pinned; all direct binary/source downloads SHA-256 (or commit-SHA) verified. linux amd64 only.
- **Python quality tools**: declare ruff, mypy, gcovr, lizard, pip-audit, and pytest-cov (exact pins) as a PEP 735 `quality` group under `[dependency-groups]` in the root `pyproject.toml`. The image installs the group through a BuildKit named build context: `docker/compose.yaml` adds `additional_contexts: repo: ..`, the Dockerfile uses `RUN --mount=from=repo,source=pyproject.toml,...`, and a new root `.dockerignore` whitelists only `pyproject.toml`. The main build context stays `docker/`. Because the image's pip is 24.0 and `--group` needs pip ≥ 25.1, a hash-pinned bootstrap pip in a throwaway venv installs into the system interpreter. Existing Python dependencies in the Dockerfile are unchanged.
- **CMake**:
  - enable `CMAKE_EXPORT_COMPILE_COMMANDS`;
  - standardize `-Wall -Wextra` for first-party targets;
  - add `BCORE_WERROR` (default OFF);
  - add `BCORE_BUILD_WITH_UBSAN` and `BCORE_BUILD_WITH_TSAN` (default OFF) alongside `BCORE_BUILD_WITH_ASAN`, with TSAN mutually exclusive with ASAN;
  - add a `cppcheck` custom target;
  - add `CMakePresets.json` (schema v1, CMake ≥ 3.19) with `dev-linux`, `dev` (alias of `dev-linux`), and `coverage`, `asan`, `ubsan`, `tsan`, `tidy` inheriting `dev`. `dev-linux` loads the existing `config/cmake/platforms/dev/linux.cmake` by setting `CMAKE_PROJECT_barton-core_INCLUDE`, so neither the top-level `CMakeLists.txt` nor `linux.cmake` changes; `build.sh` remains supported.
- **Committed config**: `.clang-tidy` (bugprone, cert, clang-analyzer, performance; no style checks), `[tool.ruff]`, `[tool.mypy]`, `[tool.coverage]`, and `[dependency-groups]` in `pyproject.toml`, root `.dockerignore`, `.jscpd.json`, `.lizardrc`-equivalent thresholds in the lizard wrapper (CCN ≤ 15), `.size-snapshot.json` for `libBartonCore.so`, and an ABI baseline location `.api/libBartonCore.abi`.
- **Integration tests**: `testing/py_test.sh` emits JUnit XML to `build/reports/`; pytest markers registered in `pyproject.toml` with `--strict-markers`.
- **Scripts**: `scripts/quality/<check>.sh` wrappers, each writing its report to `build/reports/<check>/` and returning a documented exit code.
- **Docs**: a repo-local required-checks document `docs/REQUIRED_CHECKS.md` (verification contract per Comcast EXE engineering policy: required checks, how to run, exceptions process), referenced from `.github/copilot-instructions.md`.

No behavior of the library, public API, or default build changes. No item is **BREAKING**: every new CMake option defaults OFF, and the warning flags do not use `-Werror` unless `BCORE_WERROR=ON`.

## Non-goals

- SAST / container scanning (trivy, grype, semgrep) — BARTON-418.
- CI jobs or GitHub Actions wiring — BARTON-416.
- Report caching or baseline-diff storage — BARTON-417.
- Benchmarks and mutation testing.
- Fixing existing findings from any new tool; findings are baselined or suppressed, not remediated.
- Commercial editions (e.g., Cppcheck Premium) or any paid tooling.
- Changing or splitting `config/cmake/platforms/dev/linux.cmake` (ASAN + SSP stay ON), or adding platform/preset knowledge to the top-level `CMakeLists.txt`.
- arm64 support.
- Pinning, moving, or otherwise changing existing Python dependencies (pygobject, pygobject-stubs, setuptools, python-stdnum, pytest).
- Creating a root `AGENTS.md`.
- Valgrind memcheck. It was implemented and then removed: on this codebase it took ~30 min per run (about two-thirds of `all.sh`), its only unique coverage over ASan/UBSan/TSan is uninitialized reads, and all 11 baseline findings were in the Matter SBMD/mquickjs code (not triaged; possibly JS-engine false positives). Re-adding it, e.g. as an opt-in strict check, is a separate change.

## Capabilities

### New Capabilities

- `code-quality-checks`: The repo-provided quality check suite — containerized tools, committed configuration and baselines, `scripts/quality/` wrappers, report layout under `build/reports/`, integration-test JUnit output, and the required-checks verification contract.

### Modified Capabilities

- `build-system`: Sanitizer support expands from ASAN-only to ASAN/UBSan/TSan; adds compile-command export, standard warnings, `BCORE_WERROR`, a `cppcheck` target, CMake presets, and a `quality_tools` Docker stage installing Python tools from a `repo` named build context.

## Impact

- **Layers affected**: build system and developer tooling only (`docker/`, `config/cmake/`, root `CMakeLists.txt`, `CMakePresets.json`, `pyproject.toml`, `.dockerignore`, `testing/py_test.sh`, `scripts/quality/`, `docs/`, `.github/copilot-instructions.md`). No changes to API (`api/c/public/`), core, drivers, or subsystems source.
- **CMake feature flags**: new `BCORE_WERROR`, `BCORE_BUILD_WITH_UBSAN`, `BCORE_BUILD_WITH_TSAN`; interacts with existing `BCORE_BUILD_WITH_ASAN`, `BCORE_BUILD_WITH_SSP`, `BCORE_TEST_COVERAGE`. Protocol flags (`BCORE_ZIGBEE`, `BCORE_MATTER`, `BCORE_THREAD`, `BCORE_ONVIF`) are unaffected but determine which sources clang-tidy/cppcheck analyze via `compile_commands.json`.
- **Docker image**: larger `dev_builder` image; `docker/version` bump required so `./dockerw` pulls/rebuilds.
- **Dependencies**: new OSS tools only (Apache-2.0, MIT, GPL-2.0/3.0, LGPL). GPL tools run only inside the image and are never linked into BartonCore, so the Apache-2.0 licensing of the code is unaffected.
