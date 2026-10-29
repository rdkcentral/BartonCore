# Required Checks

This document is the verification contract for BartonCore changes. It lists the quality checks, how to run them, where reports go, and how baselines are maintained. Human reviewers, CI, and agentic review tools (the `review-metrics-*` skills) all use the same commands.

All checks run inside the builder container. Every tool comes from the `quality_tools` stage of [docker/Dockerfile](../docker/Dockerfile) (see [docker/README.md](../docker/README.md#quality-tools-stage)). The scripts never install anything.

## Running

```sh
./dockerw scripts/quality/all.sh                 # every check
./dockerw scripts/quality/all.sh clang-tidy abi  # selected checks
./dockerw scripts/quality/<check>.sh --help      # usage for a single check
```

`./build.sh` stays the default developer build. The checks use CMake presets from [CMakePresets.json](../CMakePresets.json), which build into `build/<preset>/` and do not share a tree with `./build.sh`.

| Preset | Purpose |
| --- | --- |
| `dev-linux` | Loads `config/cmake/platforms/dev/linux.cmake` through `CMAKE_PROJECT_barton-core_INCLUDE`; same cache values as `./build.sh` |
| `dev` | Alias of `dev-linux` |
| `coverage` | `dev` with `BCORE_TEST_COVERAGE=ON`, ASAN off |
| `asan` | `dev` with ASAN (already the dev default) |
| `ubsan` | `dev` with UBSan, ASAN off |
| `tsan` | `dev` with TSan, ASAN off (TSan and ASAN cannot be combined) |
| `tidy` | `dev` with clang-tidy run during compilation (IDE and ad hoc use) |

## Exit codes and reports

Each check writes its native report(s) and a `summary.json` to `build/reports/<check>/` (override the root with `BCORE_REPORTS_DIR`). `all.sh` also writes `build/reports/summary.json`.

| Exit code | `status` | Meaning |
| --- | --- | --- |
| 0 | `pass` | Tool ran, no new findings versus the baseline |
| 1 | `findings` | Tool ran and reported new findings |
| 2 | `error` | Tool failed to run |
| 3 | `blocked` | Missing tool, build artifact, or baseline |

`all.sh` exits with the most severe result: `2` before `1` before `3` before `0`.

## Checks

| Check | Command | Tool(s) | Primary report | Baseline |
| --- | --- | --- | --- | --- |
| Build warnings | `build-warnings.sh` | clang-18, `-Wall -Wextra` on first-party targets | `build-warnings/warnings.txt` | `config/quality/build-warnings-baseline.txt` |
| clang-tidy | `clang-tidy.sh` | clang-tidy-18 (`run-clang-tidy`), `.clang-tidy` | `clang-tidy/clang-tidy.txt` | `config/quality/clang-tidy-baseline.txt` |
| cppcheck | `cppcheck.sh` | cppcheck via the `cppcheck` CMake target; by default only sources changed since the merge base with `QUALITY_BASE_REF` (default `origin/main`). `--full` analyzes the whole tree (CPU-hours) | `cppcheck/cppcheck.xml`, `cppcheck/analyzed-files.txt` | `config/quality/cppcheck-baseline.txt`, `config/quality/cppcheck-suppressions.txt` |
| Complexity | `complexity.sh` | lizard (CCN 15, length 1000, 10 arguments) | `complexity/lizard.xml` | `config/quality/complexity-baseline.txt` |
| Duplication | `duplication.sh` | jscpd, `.jscpd.json` | `duplication/jscpd-report.json` | `config/quality/duplication-baseline.json` |
| Python lint | `python-lint.sh` | ruff and mypy, `pyproject.toml` | `python-lint/ruff.json`, `python-lint/mypy.txt` | `config/quality/python-lint-baseline.txt` |
| Dependency health | `dependency-health.sh` | pip-audit (needs network), syft | `dependency-health/pip-audit.json`, `dependency-health/sbom.spdx.json` | `config/quality/dependency-health-baseline.txt` |
| Binary size | `binary-size.sh` | bloaty | `binary-size/bloaty.csv` | `.size-snapshot.json` (more than 10% `vm_total` growth is a finding) |
| ABI | `abi.sh` | abidw and abidiff (libabigail) | `abi/abidiff.txt` | `.api/libBartonCore.abi` (see [.api/README.md](../.api/README.md)) |
| Unit coverage | `coverage.sh` | lcov and gcovr (`coverage` preset) | `coverage/lcov.info`, `coverage/cobertura.xml` | None; reviewers compare against the merge-base |
| Sanitizers | `sanitizers.sh <asan\|ubsan\|tsan>` | `<kind>` preset and ctest | `sanitizers-<kind>/ctest.xml`, `sanitizers-<kind>/findings.txt` | `config/quality/sanitizers-<kind>-baseline.txt` (report kind + first first-party frame, and failing tests); without a baseline any report fails |
| Integration coverage | `pytest-cov.sh` | pytest-cov via `testing/py_test.sh`; needs BartonCore installed in the same container first (`sudo cmake --build build --target install`, as in `scripts/ci/run_integration_tests.sh`), otherwise reports blocked | `pytest-cov/coverage.xml` | None |

Integration tests (`testing/py_test.sh`) always write JUnit XML to `build/reports/pytest/junit.xml` unless `--junitxml` is passed. Use `--sanitizer=<asan|ubsan|tsan|none>` to match the build being tested (default `asan`).

The ABI check reports `blocked` until `.api/libBartonCore.abi` has been generated from a release tag.

## Baselines

Findings are compared against committed baselines so only new findings fail a check. Keys omit line numbers so unrelated edits do not churn them. Baselines hold one line per distinct finding location and are compared by count, so a second occurrence of an already-baselined key in the same file is reported as new. To update a baseline after deliberately accepting findings, run the check with `--update-baseline` and commit the resulting file:

```sh
./dockerw scripts/quality/clang-tidy.sh --update-baseline
./dockerw scripts/quality/binary-size.sh --update-baseline   # rewrites .size-snapshot.json
```

Baseline updates must be called out in the pull request description with a reason.

## clang-tidy configuration

`.clang-tidy` enables `bugprone-*`, `cert-*`, `clang-analyzer-*`, and `performance-*`. Style families (`readability-*`, `modernize-*`) are not enabled because clang-format already owns style. The following checks are disabled:

| Check | Reason |
| --- | --- |
| `bugprone-easily-swappable-parameters` | Flags most multi-parameter C functions; very noisy with little signal for this codebase |
| `cert-err33-c` | Requires checking the return value of every standard library call (e.g. `printf`, `snprintf`); very noisy for logging code |

## Exceptions

If a check cannot pass for a legitimate reason:

1. Prefer fixing the finding.
2. Otherwise, use an inline suppression with a justification comment (`// NOLINT(<check>): <reason>` for clang-tidy, `// cppcheck-suppress <id>` for cppcheck, `# noqa: <code>` for ruff).
3. As a last resort, update the baseline as described above and explain it in the pull request.

A `blocked` result must be reported as blocked, never as passing.
