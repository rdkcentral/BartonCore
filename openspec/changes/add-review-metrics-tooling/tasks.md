# Tasks

## 1. Python quality tools via dependency group (Docker required)

- [x] 1.1 Add `[dependency-groups] quality` (ruff==0.17.2, mypy==2.4.0, gcovr==8.7, lizard==1.24.3, pip-audit==2.10.1, pytest-cov==7.1.0) to the root `pyproject.toml`; verify with a pip ≥ 25.1 `pip install --dry-run --group pyproject.toml:quality` inside a container venv (≤ 1h)
- [x] 1.2 Add root `.dockerignore` (`*`, `!pyproject.toml`) and `additional_contexts: repo: ..` to the `barton` service build in `docker/compose.yaml`; verify `docker/build.sh` and the devcontainer compose overlay still build, and record the `repo` context transfer size from the build log to confirm `.dockerignore` is honored (≤ 1h)

## 2. Docker `quality_tools` stage (Docker required)

- [x] 2.1 Add `quality_tools` stage with pinned apt packages (clang-tidy-18, cppcheck, abigail-tools), and make `dev_builder` inherit it; verify `./dockerw clang-tidy --version`, `cppcheck --version`, `abidiff --version` report pinned versions (≤ 2h)
- [x] 2.2 Add commit-verified bloaty source build, SHA-256-verified syft (linux amd64) download, and `npm install -g jscpd@<pin>`; verify each `--version` in `./dockerw` and that a deliberately wrong SHA fails the build (≤ 3h)
- [x] 2.3 Add the bootstrap-pip `RUN --mount=from=repo,source=pyproject.toml,...` step installing the `quality` group into the system interpreter; verify each tool's `--version` in `./dockerw`, `pip3 --version` still reports 24.0, and `pytest --help` lists `--cov` (≤ 2h)
- [x] 2.4 Bump `docker/version`, document the stage, named context, `.dockerignore`, dependency group, and pin-update procedure in `docker/README.md`, and record image size before/after in the PR description (≤ 1h)

## 3. CMake options and flags (API-neutral; Docker required)

- [x] 3.1 Add `BCORE_BUILD_WITH_UBSAN`, `BCORE_BUILD_WITH_TSAN`, `BCORE_WERROR` to `config/cmake/options.cmake`; verify all default OFF in `CMakeCache.txt` (≤ 1h)
- [x] 3.2 Refactor root sanitizer block to the combined `-fsanitize=` list with ASAN+TSAN `FATAL_ERROR`; verify ASAN-only flags unchanged in `compile_commands.json`, UBSan/TSan flags present when enabled, and ASAN+TSAN configure fails (≤ 2h)
- [x] 3.3 Enable `CMAKE_EXPORT_COMPILE_COMMANDS` and add `bCoreWarnings` interface target linked into first-party targets; verify `-Wall -Wextra` on `core/` entries and absent on `third_party/` entries, and `-Werror` only with `BCORE_WERROR=ON` (≤ 3h)
- [x] 3.7 Gate `-Wall -Wextra` behind private `BCORE_EXTRA_WARNINGS` (default OFF, ON in `linux.cmake`); verify `dev` compile commands keep the flags and a configure without the platform file has none (≤ 1h)
- [x] 3.4 Add `cppcheck` target and `config/quality/cppcheck-suppressions.txt`; verify `cmake --build build --target cppcheck` writes `<build-dir>/cppcheck.xml` (copied by `cppcheck.sh` to `build/reports/cppcheck/cppcheck.xml`) and excludes `third_party/` (≤ 2h)
- [x] 3.5 Add `CMakePresets.json` (version 1: `dev-linux` with `CMAKE_PROJECT_barton-core_INCLUDE=${sourceDir}/config/cmake/platforms/dev/linux.cmake` and `CMAKE_BUILD_TYPE=Debug`; `dev` alias; `coverage`, `asan`, `ubsan`, `tsan`, `tidy` inheriting `dev`) without changing `linux.cmake` or adding platform/preset logic to `CMakeLists.txt`; verify each `cmake --preset <name>` configures, `dev` cache entries match a `./build.sh` configuration, `tsan` ends with ASAN OFF, BartonCommon does not re-include `linux.cmake`, and `./build.sh` is unaffected; run existing `ctest` on `dev` to confirm no regressions (≤ 3h)

- [x] 3.6 Add `BCORE_MATTER_INSTALL_DIR` and `build-matter.sh -i`; have `build-matter.sh` pass `CHIP_ROOT=<its source dir>` to `barton-library/linux/build.sh` (which defaults to the committed `connectedhomeip` symlink) so each install dir builds from its own patched checkout; point presets at shared ASAN / non-ASAN Matter installs; verify `./build.sh` default paths unchanged and `cmake --preset dev` reuses `build/matter-install`

## 4. Integration test runner (Docker required)

- [x] 4.1 Register all existing pytest markers in `pyproject.toml` with `--strict-markers`; verify `./dockerw testing/py_test.sh --collect-only` succeeds (≤ 1h)
- [x] 4.2 Add default `--junitxml=build/reports/pytest/junit.xml` and `--sanitizer=<asan|ubsan|tsan|none>` to `testing/py_test.sh` (default asan); verify default run produces the JUnit file with unchanged pass/fail, and a `tsan` preset build runs a smoke test with the TSan runtime preloaded (≤ 3h)

## 5. Committed configuration and baselines (Docker required)

- [x] 5.1 Add `.clang-tidy`; run on the `tidy` preset and commit `config/quality/clang-tidy-baseline.txt`; verify a rerun reports zero new findings (≤ 4h)
- [x] 5.2 Add `[tool.ruff]`, `[tool.mypy]`, `[tool.coverage]` to `pyproject.toml` (alongside the `[dependency-groups]` from 1.1); verify ruff/mypy run and produce reports, recording existing findings as baseline (≤ 2h)
- [x] 5.3 Add `.jscpd.json`; verify `jscpd` ignores `build/`, `third_party/`, tests, and generated code (≤ 1h)
- [x] 5.4 Generate and commit `.size-snapshot.json` from a `dev` build of `libBartonCore.so`; add `.api/README.md` describing ABI baseline generation from a release tag (≤ 2h)

## 6. Wrapper scripts (Docker required)

- [x] 6.1 Add `scripts/quality/lib.sh` with summary/exit-code helpers; verify a stub check writes valid `summary.json` and exits 3 when its tool is hidden from `PATH` (≤ 2h)
- [x] 6.2 Add `clang-tidy.sh`, `cppcheck.sh`, `build-warnings.sh`, `complexity.sh` (CCN ≤ 15), `duplication.sh`; verify each writes to `build/reports/<check>/` and exits 0 against its baseline (≤ 4h)
- [x] 6.3 Add `coverage.sh` (lcov + gcovr), `pytest-cov.sh`, `sanitizers.sh`; verify reports and exit codes for each (≤ 4h)
- [x] 6.4 Add `abi.sh`, `binary-size.sh`, `python-lint.sh`, `dependency-health.sh`, `all.sh`; verify `abi.sh` exits 3 with no baseline, `binary-size.sh` compares to `.size-snapshot.json`, and `all.sh` writes `build/reports/summary.json` (≤ 4h)

## 7. Documentation

- [x] 7.1 Write `docs/REQUIRED_CHECKS.md` (checks, commands, report paths, baseline updates, exceptions process); link it from `.github/copilot-instructions.md` (no root `AGENTS.md`); verify every documented command runs as written via `./dockerw` (≤ 3h)

## 8. Integration verification (Docker required)

- [ ] 8.1 From a clean clone, run `./dockerw ./build.sh`, `ctest`, `testing/py_test.sh`, and `./dockerw scripts/quality/all.sh`; verify default build/tests unchanged and every check reports a status in `build/reports/` (≤ 3h)
