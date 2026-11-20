## ADDED Requirements

### Requirement: Consolidated observability metrics report
The SBMD observability test suite (`testing/observability/`) SHALL write one consolidated report per invocation to a report directory (`testing/.metrics-reports/` by default, overridable via the `SBMD_REPORT_DIR` environment variable): `sbmd_metrics.txt` (human-readable), `sbmd_metrics.json` (machine-parseable), and one `<scenario>.csv` file per scenario that samples a metric over a sweep. The report SHALL be keyed by a run id shared across all test subprocesses in one invocation (`SBMD_REPORT_RUN_ID`, assigned once by the parent `pytest_configure` and inherited by child subprocesses via the environment), so that a fresh invocation truncates the previous report rather than appending to it.

#### Scenario: One consolidated report per suite invocation
- **WHEN** the observability suite is run in one invocation, covering multiple test files and scenarios
- **THEN** all scenarios' metrics land in the same `sbmd_metrics.txt` / `sbmd_metrics.json` under one run id

#### Scenario: Fresh invocation truncates the prior report
- **WHEN** the observability suite is invoked again
- **THEN** the previous run's report content is not mixed with the new run's content

#### Scenario: Report directory is overridable
- **WHEN** `SBMD_REPORT_DIR` is set in the environment before the suite runs
- **THEN** the report is written under that directory instead of the default `testing/.metrics-reports/`

### Requirement: Per-metric graded verdict
The report writer SHALL assess each recorded metric and assign one of five statuses — `ok`, `watch`, `concern`, `devonly`, or `info` — and SHALL compute an overall verdict as the worst status across all scenarios. `sbmd.js.heap.live_bytes` SHALL be graded as true occupancy (`watch` at ≥50% of the configured arena, `concern` at ≥75%); `sbmd.js.heap.peak_bytes`, `free_bytes`, and `used_bytes` SHALL be reported as `info` only and SHALL NOT be graded as capacity signals, because under a lazy compacting GC they reflect transient high-water or instantaneous churn rather than retained state.

#### Scenario: Live-set breach is graded a concern
- **WHEN** `sbmd.js.heap.live_bytes` is at or above 75% of the configured arena size
- **THEN** the report marks that metric `concern` and the overall verdict is at least `concern`

#### Scenario: Peak/used/free are informational only
- **WHEN** `sbmd.js.heap.peak_bytes` reads near 100% of the arena
- **THEN** the report marks it `info`, not `concern`, and it does not by itself drive the overall verdict

### Requirement: Per-series heap-delta allocation ceiling
The report writer SHALL grade `sbmd.handler.heap_delta_bytes` by finding the maximum observed value within each distinct `(driver, op_type[, resource_id])` attribute series and comparing that single worst-series maximum against an arena-scaled ceiling (`watch` at ≥0.8% of arena, `concern` at ≥3%). It SHALL NOT grade this metric using the scenario-wide average across all series, because the average is diluted by whichever mix of operations a given scenario happens to exercise and can obscure a single pathological series.

#### Scenario: A heterogeneous mix of light-weight and heavier handlers grades ok
- **WHEN** a scenario exercises several different `(driver, op_type)` series whose individual per-call maximums each stay under the watch ceiling, even though the scenario-wide average across all series would exceed a flat per-call threshold
- **THEN** the report grades `sbmd.handler.heap_delta_bytes` `ok` for that scenario

#### Scenario: A single pathological series is not diluted by a benign average
- **WHEN** one `(driver, op_type)` series' maximum per-call allocation exceeds the concern ceiling, even if the scenario's overall average would not
- **THEN** the report grades `sbmd.handler.heap_delta_bytes` `concern` for that scenario and names the offending series in the assessment note

### Requirement: Fault-injection outcome downgrade
When a scenario's context marks `fault_injection: true`, the report writer SHALL downgrade a `concern`-graded `sbmd.handler.outcome` non-success observation to `info`, annotated as an expected fault-injection result, rather than letting a deliberately-triggered failure (e.g. a timeout or runaway deferred chain, injected to prove the corresponding circuit breaker fires) drive the scenario or overall verdict to `concern`.

#### Scenario: Deliberately injected timeout does not fail the verdict
- **WHEN** a scenario tagged `fault_injection: true` deliberately triggers `sbmd.deferred.timeout`, producing one non-success `sbmd.handler.outcome` observation
- **THEN** that observation is graded `info` (not `concern`) and does not by itself drive the scenario verdict to `concern`

### Requirement: Multi-device concurrent-load scenario
The suite SHALL include a scenario that commissions a configurable number of devices of one driver (default 20, via the `SBMD_SCALE_DEVICES` environment variable) plus one deferred-command-capable device, drives concurrent load across all of them (one worker thread per device performing writes, plus a worker exercising the deferred device), deliberately injects the deferred-timeout and max-depth circuit-breaker faults, and asserts on every runtime metric family: zero unexpected non-success outcomes in the pre-fault phase, zero JS exceptions, non-zero JS-mutex contention under concurrency, bounded handler and deferred durations, GC activity within bounds (when GC instrumentation is present), and `sbmd.deferred.in_flight` returning to zero after the injected faults resolve.

#### Scenario: Concurrent load across many devices settles cleanly
- **WHEN** the concurrent multi-device scenario completes its healthy-load phase
- **THEN** there are zero JS exceptions, zero unexpected non-success handler outcomes, and non-zero recorded JS-mutex wait time

#### Scenario: Injected faults resolve without leaving state behind
- **WHEN** the scenario's fault-injection phase triggers the deferred timeout and max-depth conditions
- **THEN** `sbmd.deferred.timeout` and `sbmd.deferred.max_depth` each increment by at least one and `sbmd.deferred.in_flight` returns to zero afterward

### Requirement: Heterogeneous multi-driver concurrent scenario
The suite SHALL include a scenario that commissions one device of each drivable device type reachable by the test harness (light, door lock, thermostat, temperature sensor, humidity sensor) plus one deferred-command-capable device, and drives all of them concurrently across their respective invocation paths — attribute writes, command executes, and sideband-triggered attribute reports — asserting the same full runtime metric family as the multi-device concurrent-load scenario. This scenario exists because a large *count* of devices sharing one driver does not exercise contention between *distinct* drivers on the single shared JS context; commissioning several device types at once does.

#### Scenario: Distinct drivers exercised in one concurrent run
- **WHEN** the heterogeneous-driver scenario runs
- **THEN** handler invocations attributed to at least five distinct `"driver"` values appear in the same run's metrics, with non-zero JS-mutex contention recorded

### Requirement: Driver-count scaling scenario
The suite SHALL include a scenario that generates a configurable number (default 100, via the `SBMD_SCALE_DRIVERS` environment variable) of minimal, schema-valid, inert SBMD stub drivers into a temporary directory, injects that directory via the `BARTON_EXTRA_SBMD_DIRS` test-only environment hook, and asserts on driver-count-scaling metrics only: `sbmd.driver.registered.count` reflects the expected total, `sbmd.driver.load.failure` is zero, `sbmd.js.gc_roots` stays bounded, and heap occupancy (`live_bytes`/`peak_bytes`) stays within the arena. This scenario SHALL NOT assert on handler-invocation, mutex, or deferred-operation metrics, because the generated stub drivers match no device type any commissioned device presents and their handlers are never invoked.

#### Scenario: Many stub drivers load without failure
- **WHEN** the driver-count scaling scenario generates and registers its configured stub-driver count alongside the production and test drivers
- **THEN** `sbmd.driver.load.failure` remains zero and `sbmd.driver.registered.count` equals the expected total

### Requirement: Test-only extra SBMD directory injection hook
`BaseEnvironmentOrchestrator` SHALL append the value of the `BARTON_EXTRA_SBMD_DIRS` environment variable, if set, to the SBMD driver directories passed to the runtime, in addition to the production and test-spec directories it already configures. This variable SHALL be read only at orchestrator construction time and SHALL have no effect when unset, which SHALL always be the case in production.

#### Scenario: Extra directory is included when set
- **WHEN** `BARTON_EXTRA_SBMD_DIRS` is set to a directory path before an environment orchestrator is constructed
- **THEN** that directory is included among the SBMD driver directories the runtime scans

#### Scenario: No effect when unset
- **WHEN** `BARTON_EXTRA_SBMD_DIRS` is not set
- **THEN** the runtime scans exactly the production and test-spec directories it would scan without this hook

### Requirement: DEBUG_GC build flip automation
`testing/gcflip.sh` SHALL provide `on`, `off`, and `status` subcommands that ensure the installed mquickjs static library (and the BartonCore shared library relinked against it) matches the requested DEBUG_GC compile-time state, without requiring a manual mquickjs rebuild on every flip. Each DEBUG_GC variant's built archive SHALL be cached (by default under `~/.cache/bartoncore-gcflip/`) after its first build so that subsequent flips to an already-built variant are a cached-archive swap and relink only. `gcflip.sh status` SHALL determine the current state by comparing the installed archive's checksum against the cached archives, not by tracking state in a separate file.

#### Scenario: First flip to a variant builds and caches it
- **WHEN** `gcflip.sh off` is run and no cached DEBUG_GC-off archive exists
- **THEN** mquickjs is built with DEBUG_GC disabled, the result is cached, and BartonCore is relinked against it

#### Scenario: Subsequent flips are a cached swap
- **WHEN** `gcflip.sh off` is run again after the DEBUG_GC-off archive is already cached
- **THEN** no mquickjs build occurs; the cached archive is installed and BartonCore is relinked

#### Scenario: Flipping to the already-installed state is a no-op
- **WHEN** `gcflip.sh <state>` is run and the installed archive already matches the cached archive for `<state>`
- **THEN** neither a build nor a relink occurs

### Requirement: Observability suite runner DEBUG_GC control
`run_observability_suite.py` SHALL accept a `--debug-gc {on,off,both}` flag (default `off`) that flips the mquickjs DEBUG_GC build via `gcflip.sh` before running the suite. For `off` (the default) and `both`, it SHALL restore the DEBUG_GC-on build in a `finally` block after the run completes or fails, so a suite invocation never leaves the dev environment on a non-default build. For `both`, it SHALL run the full suite once under each state, writing each run's report to a separate directory (`.metrics-reports-debug_gc_on/` and `.metrics-reports-debug_gc_off/`).

#### Scenario: Default invocation runs under DEBUG_GC off and restores on
- **WHEN** `run_observability_suite.py` is invoked with no `--debug-gc` flag
- **THEN** the suite runs under the DEBUG_GC-off build and the DEBUG_GC-on build is restored before the script exits, even if the suite run fails

#### Scenario: `--debug-gc both` produces two separate reports
- **WHEN** `run_observability_suite.py --debug-gc both` is invoked
- **THEN** the suite runs once under each DEBUG_GC state, and each run's report is written to its own directory
