## Purpose

Defines the repository-provided code quality check suite: containerized tools, committed configuration and baselines, wrapper scripts, report locations, and the required-checks verification contract used by developers and agentic reviewers.

## ADDED Requirements

### Requirement: Containerized quality toolchain
The dev container image SHALL provide clang-tidy, cppcheck, abidiff, bloaty, ruff, mypy, gcovr, lizard, pip-audit, pytest-cov, jscpd, and syft at pinned versions, so every quality check runs via `./dockerw` without installing tools at check time.

#### Scenario: Tools available in dev image
- **WHEN** a developer runs `<tool> --version` via `./dockerw` for any listed tool
- **THEN** the command SHALL succeed and report the version pinned in `docker/Dockerfile` or in the `quality` dependency group of `pyproject.toml`

### Requirement: Verified tool downloads
Every quality tool fetched outside the Ubuntu archive or package registries' hash-pinned installs SHALL be verified against a committed SHA-256 checksum or pinned commit SHA before installation.

#### Scenario: Checksum mismatch
- **WHEN** a downloaded tool artifact does not match its committed checksum
- **THEN** the image build SHALL fail

### Requirement: Python quality tools declared as a dependency group
The Python quality tools (ruff, mypy, gcovr, lizard, pip-audit, pytest-cov) SHALL be declared with exact versions in a PEP 735 `quality` group under `[dependency-groups]` in the root `pyproject.toml`, and the image SHALL install them from that group. Existing Python dependencies installed by `docker/Dockerfile` SHALL remain unchanged.

#### Scenario: Dependency group drives image install
- **WHEN** the image is built
- **THEN** the Python quality tools SHALL be installed from the `quality` group of the root `pyproject.toml` provided through the `repo` named build context

#### Scenario: Quality tools visible to system pytest
- **WHEN** `pytest --help` runs via `./dockerw`
- **THEN** the pytest-cov `--cov` option SHALL be listed

#### Scenario: System pip unchanged
- **WHEN** `pip3 --version` runs via `./dockerw`
- **THEN** it SHALL report the distribution-provided pip, not the bootstrap pip used to install the group

### Requirement: Quality check wrapper scripts
The repository SHALL provide one script per check under `scripts/quality/`, each runnable via `./dockerw` and never installing tools.

#### Scenario: Run a check
- **WHEN** a developer runs `./dockerw scripts/quality/<check>.sh`
- **THEN** the check SHALL run and print the path to its report

### Requirement: Report location and summary
Each quality check SHALL write its native report and a `summary.json` (check, status, message, timestamp) under `build/reports/<check>/`.

#### Scenario: Report written
- **WHEN** a quality check completes
- **THEN** `build/reports/<check>/summary.json` SHALL exist with `status` one of `pass`, `findings`, `error`, or `blocked`

### Requirement: Check exit codes
Quality check scripts SHALL exit 0 for pass, 1 for new findings, 2 for tool error, and 3 for blocked.

#### Scenario: Missing tool
- **WHEN** a required tool is not on `PATH`
- **THEN** the script SHALL write `status: blocked`, name the missing tool, and exit 3

#### Scenario: Missing ABI baseline
- **WHEN** the ABI check runs and `.api/libBartonCore.abi` does not exist
- **THEN** the script SHALL report blocked with a message describing how to generate the baseline

### Requirement: Baseline comparison
Checks with committed baselines (size snapshot, ABI baseline, static-analysis suppressions) SHALL report only findings not present in the baseline as new findings.

#### Scenario: Pre-existing finding
- **WHEN** a static-analysis finding is listed in the committed baseline
- **THEN** it SHALL NOT cause a `findings` status

### Requirement: Committed analysis configuration
The repository SHALL commit configuration for clang-tidy (bugprone, cert, clang-analyzer, performance checks; no style checks), ruff, mypy, coverage, jscpd (excluding build, third-party, test, and generated code), and a lizard cyclomatic complexity threshold of 15.

#### Scenario: Complexity threshold
- **WHEN** a function has cyclomatic complexity greater than 15 and is not baselined
- **THEN** the complexity check SHALL report it as a finding

#### Scenario: Style checks excluded
- **WHEN** clang-tidy runs with the committed configuration
- **THEN** no `readability-*` or `modernize-*` checks SHALL be enabled

### Requirement: Integration test JUnit output
`testing/py_test.sh` SHALL write JUnit XML to `build/reports/pytest/junit.xml` unless the caller supplies `--junitxml`, and pytest markers SHALL be registered with strict marker checking.

#### Scenario: Default JUnit output
- **WHEN** `testing/py_test.sh` runs without `--junitxml`
- **THEN** `build/reports/pytest/junit.xml` SHALL be produced

#### Scenario: Unregistered marker
- **WHEN** a test uses a marker not registered in `pyproject.toml`
- **THEN** pytest collection SHALL fail

### Requirement: Sanitizer-aware integration test runner
`testing/py_test.sh` SHALL preload the sanitizer runtime matching the selected sanitizer, defaulting to ASAN.

#### Scenario: Default unchanged
- **WHEN** `testing/py_test.sh` runs without a sanitizer option
- **THEN** it SHALL preload the ASAN runtime as before

#### Scenario: TSan run
- **WHEN** `testing/py_test.sh --sanitizer=tsan` runs
- **THEN** it SHALL preload the TSan runtime and not the ASAN runtime

### Requirement: Required checks verification contract
The repository SHALL contain `docs/REQUIRED_CHECKS.md` listing each required check, how to run it, its report path, and the exceptions process, referenced from `.github/copilot-instructions.md`.

#### Scenario: Contract discoverable
- **WHEN** an agent reads `.github/copilot-instructions.md`
- **THEN** it SHALL find a reference to `docs/REQUIRED_CHECKS.md`

### Requirement: Open-source tooling only
All quality tools SHALL be free open-source editions and SHALL run only inside the container, never linked into BartonCore artifacts.

#### Scenario: No paid editions
- **WHEN** the quality toolchain is reviewed
- **THEN** it SHALL contain no commercial editions such as Cppcheck Premium
